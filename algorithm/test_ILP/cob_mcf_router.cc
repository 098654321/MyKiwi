#include "cob_mcf_router.hh"

#include "highs.hh"
#include "mcf_hw_map.hh"
#include "highs/Highs.h"
#include "highs/lp_data/HConst.h"
#include "highs/lp_data/HighsLp.h"

#include "circuit/basedie.hh"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <format>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

#include <debug/debug.hh>

namespace PR_tool {

namespace {

using namespace mcf;

const double kCapNormal = 8.0;
const std::size_t kTob0 = 108U;
const int kTob0i = 108;
const int kVPi = 124;
const int kVNi = 125;
const int kNumNi = 126;

struct Arc {
    int u{0};
    int v{0};
    bool is_virt{false};
};

struct McfK {
    std::String label;
    int src{0};
    int snk{0};
    int demand{0};
    int class_id{0};
};

// 合并 net
auto mcf_merge_key(const Net_cost_record& r) -> std::String {
    const auto& o = r.origin_key.empty() ? r.net_name : r.origin_key;
    return std::format("{}#{}", o, static_cast<int>(r.type));
}

auto demand_bits_ceil(const Net_cost_record& r) -> int {
    const int b = static_cast<int>(std::lround(static_cast<double>(r.bits)));
    return b < 1 ? 1 : b;
}

void build_base_arcs(std::Vector<Arc>& arcs) {
    for (int r = 0; r < 9; ++r) {
        for (int c = 0; c < 12; ++c) {
            const int u = r * 12 + c;
            if (r + 1 < 9) {
                const int d = (r + 1) * 12 + c;
                arcs.push_back(Arc{u, d, false});
                arcs.push_back(Arc{d, u, false});
            }
            if (c + 1 < 12) {
                const int ri = u + 1;
                arcs.push_back(Arc{u, ri, false});
                arcs.push_back(Arc{ri, u, false});
            }
        }
    }
    for (int tr = 0; tr < 4; ++tr) {
        for (int tc = 0; tc < 4; ++tc) {
            const auto p = tob_pair_cob_coords(static_cast<std::size_t>(tr), static_cast<std::size_t>(tc));
            const int tnode = kTob0i + tr * 4 + tc;
            const int a = static_cast<int>(cob_to_linear(p.first));
            const int b = static_cast<int>(cob_to_linear(p.second));
            arcs.push_back(Arc{tnode, a, false});
            arcs.push_back(Arc{a, tnode, false});
            arcs.push_back(Arc{tnode, b, false});
            arcs.push_back(Arc{b, tnode, false});
        }
    }
}

void add_port_arcs(
    const hardware::Interposer& interposer, const circuit::BaseDie& basedie, std::Vector<Arc>& arcs, std::set<int>& p_cob,
    std::set<int>& n_cob) {
    (void)interposer;
    for (const auto& tc : basedie.pose_ports()) {
        const auto c = track_to_cob(tc);
        const int lin = static_cast<int>(cob_to_linear(c));
        p_cob.insert(lin);
        arcs.push_back(Arc{lin, kVPi, true});
    }
    for (const auto& tc : basedie.nege_ports()) {
        const auto c = track_to_cob(tc);
        const int lin = static_cast<int>(cob_to_linear(c));
        n_cob.insert(lin);
        arcs.push_back(Arc{lin, kVNi, true});
    }
}

static auto cls_b() -> int {
    return 0;
}
static auto cls_t() -> int {
    return 1;
}
static auto cls_p() -> int {
    return 2;
}
static auto cls_n() -> int {
    return 3;
}

auto arc_class_ok(int cid, int u, int v, const std::set<int>& p_cob, const std::set<int>& n_cob) -> bool {
    if (u < 0 || v < 0 || u >= kNumNi || v >= kNumNi) {
        return false;
    }
    // Path rules (LP variables): only restrict arcs incident to V_P / V_N so non-matching classes cannot
    // use 0/1 port trunk edges. All COB–COB/TOB edges stay available (full P/N COB set handled by paper's
    // optional cuts in a later revision).
    (void)p_cob;
    (void)n_cob;
    if (u == kVPi || u == kVNi || v == kVPi || v == kVNi) {
        if (u == kVPi || v == kVPi) {
            return cid == cls_p();
        }
        if (u == kVNi || v == kVNi) {
            return cid == cls_n();
        }
    }
    return true;
}

void build_commodities(
    const std::Vector<Net_cost_record>& records, const std::Vector<std::size_t>& gids, std::Vector<McfK>& out) {
    if (gids.empty()) {
        return;
    }
    const auto& t0 = records[gids[0]];

    // 目前不考虑混合类型的net
    for (const auto i : gids) {
        if (records[i].type != t0.type) {
            throw std::runtime_error(std::format("MCF: mixed type in group at {}", records[i].net_name));
        }
    }
    if (t0.type == Net_type::PNnet) {
        if (t0.power_kind == IlpPowerKind::None) {
            throw std::runtime_error(std::format("MCF: PN power kind unset: {}", t0.net_name));
        }
        const int vdst = t0.power_kind == IlpPowerKind::Pose ? kVPi : kVNi;
        const int cls = t0.power_kind == IlpPowerKind::Pose ? cls_p() : cls_n();
        std::map<std::size_t, int> tob_bumps;
        for (const auto i : gids) {
            const auto& r = records[i];
            for (const auto& b : r.start_bumps) {
                tob_bumps[b.TOB] += 1;
            }
        }
        for (const auto& [tob_lin, d] : tob_bumps) {
            out.push_back(McfK {std::format("{}#T{}", t0.net_name, tob_lin), static_cast<int>(kTob0 + tob_lin), vdst, d, cls});
        }
        return;
    }
    if (t0.type == Net_type::Bnet) {
        const auto& r0 = records[gids[0]];
        if (r0.start_bumps.empty() || r0.end_bumps.empty()) {
            throw std::runtime_error(std::format("MCF: Bnet no bump: {}", r0.net_name));
        }
        int s0 = static_cast<int>(kTob0 + r0.start_bumps[0].TOB);
        int t0b = static_cast<int>(kTob0 + r0.end_bumps[0].TOB);
        bool all_same = true;
        for (const auto i : gids) {
            const auto& r = records[i];
            if (r.start_bumps.empty() || r.end_bumps.empty()) {
                throw std::runtime_error(std::format("MCF: Bnet no bump: {}", r.net_name));
            }
            if (static_cast<int>(kTob0 + r.start_bumps[0].TOB) != s0
                || static_cast<int>(kTob0 + r.end_bumps[0].TOB) != t0b) {
                all_same = false;
                break;
            }
        }
        if (all_same) {
            int ssum = 0;
            for (const auto i : gids) {
                ssum += demand_bits_ceil(records[i]);
            }
            out.push_back(McfK {t0.net_name, s0, t0b, ssum, cls_b()});
            return;
        }
        for (const auto i : gids) {
            const auto& r = records[i];
            int ss = static_cast<int>(kTob0 + r.start_bumps[0].TOB);
            int tt = static_cast<int>(kTob0 + r.end_bumps[0].TOB);
            out.push_back(McfK {r.net_name, ss, tt, demand_bits_ceil(r), cls_b()});
        }
        return;
    }
    if (t0.type == Net_type::Tnet) {
        for (const auto i : gids) {
            const auto& r = records[i];
            if (r.mcf_has_end_track) {
                // BumpToTrack: source TOB, sink track COB
                if (r.start_bumps.empty()) {
                    throw std::runtime_error(std::format("MCF: Tnet no start bump: {}", r.net_name));
                }
                const int src = static_cast<int>(kTob0 + r.start_bumps[0].TOB);
                const int snk = static_cast<int>(cob_to_linear(track_to_cob(r.mcf_end_track)));
                out.push_back(McfK {r.net_name, src, snk, demand_bits_ceil(r), cls_t()});
            }
            else if (r.mcf_has_start_track) {
                // TrackToBump: ILP model stores the lone bump in start_bumps
                if (r.start_bumps.empty()) {
                    throw std::runtime_error(std::format("MCF: Tnet no bump (track->bump): {}", r.net_name));
                }
                const int src = static_cast<int>(cob_to_linear(track_to_cob(r.mcf_start_track)));
                const int snk = static_cast<int>(kTob0 + r.start_bumps[0].TOB);
                out.push_back(McfK {r.net_name, src, snk, demand_bits_ceil(r), cls_t()});
            }
            else {
                throw std::runtime_error(std::format("MCF: Tnet no track endpoint: {}", r.net_name));
            }
        }
    }
}

auto flow_row_id(int k, int node) -> int {
    return k * kNumNi + node;
}

auto solve_mcf_lp(
    const std::Vector<Arc>& arcs, const std::set<int>& p_cob, const std::set<int>& n_cob, const std::Vector<McfK>& com) -> std::pair<bool, double> {
    if (com.empty()) {
        return {true, 0.0};
    }
    const int A = static_cast<int>(arcs.size());
    const int K = static_cast<int>(com.size());
    struct VarE {
        int k;
        int a;
    };
    std::vector<VarE> vars;
    std::set<std::pair<int, int>> cap_edge_keys;
    for (int k = 0; k < K; ++k) {
        for (int a = 0; a < A; ++a) {
            if (arc_class_ok(
                    com[static_cast<std::size_t>(k)].class_id, arcs[static_cast<std::size_t>(a)].u, arcs[static_cast<std::size_t>(a)].v,
                    p_cob, n_cob
                )) {
                vars.push_back(VarE{k, a});
                if (!arcs[static_cast<std::size_t>(a)].is_virt) {
                    int u1 = arcs[static_cast<std::size_t>(a)].u;
                    int v1 = arcs[static_cast<std::size_t>(a)].v;
                    if (u1 > v1) {
                        std::swap(u1, v1);
                    }
                    cap_edge_keys.insert(std::make_pair(u1, v1));
                }
            }
        }
    }
    const int num_col = static_cast<int>(vars.size());
    if (num_col == 0) {
        return {false, 0.0};
    }
    std::map<std::pair<int, int>, int> e2id;
    {
        int eid = 0;
        for (const auto& p : cap_edge_keys) {
            e2id[p] = eid++;
        }
    }
    const int n_cap = static_cast<int>(e2id.size());
    const int n_flow = K * kNumNi;
    const int num_row = n_flow + n_cap;
    std::vector<HighsInt> astart(static_cast<std::size_t>(num_col) + 1, 0);
    std::vector<HighsInt> aind;
    std::vector<double> aval;
    aind.reserve(static_cast<std::size_t>(num_col) * 5u);
    aval.reserve(static_cast<std::size_t>(num_col) * 5u);
    for (int j = 0; j < num_col; ++j) {
        astart[static_cast<std::size_t>(j)] = static_cast<HighsInt>(aind.size());
        const int k = vars[static_cast<std::size_t>(j)].k;
        const int a = vars[static_cast<std::size_t>(j)].a;
        const int u = arcs[static_cast<std::size_t>(a)].u;
        const int v = arcs[static_cast<std::size_t>(a)].v;
        aind.push_back(static_cast<HighsInt>(flow_row_id(k, u)));
        aval.push_back(1.0);
        aind.push_back(static_cast<HighsInt>(flow_row_id(k, v)));
        aval.push_back(-1.0);
        if (!arcs[static_cast<std::size_t>(a)].is_virt) {
            int u1 = u;
            int v1 = v;
            if (u1 > v1) {
                std::swap(u1, v1);
            }
            aind.push_back(static_cast<HighsInt>(n_flow + e2id.at({u1, v1})));
            aval.push_back(1.0);
        }
    }
    astart[static_cast<std::size_t>(num_col)] = static_cast<HighsInt>(aind.size());
    std::vector<double> row_lo(static_cast<std::size_t>(num_row), -kHighsInf);
    std::vector<double> row_up(static_cast<std::size_t>(num_row), kHighsInf);
    std::vector<double> col_lo(static_cast<std::size_t>(num_col), 0.0);
    std::vector<double> col_up(static_cast<std::size_t>(num_col), kHighsInf);
    for (int k = 0; k < K; ++k) {
        for (int n = 0; n < kNumNi; ++n) {
            const int r = flow_row_id(k, n);
            row_lo[static_cast<std::size_t>(r)] = 0.0;
            row_up[static_cast<std::size_t>(r)] = 0.0;
        }
    }
    for (int k = 0; k < K; ++k) {
        const int d = com[static_cast<std::size_t>(k)].demand;
        const int s = com[static_cast<std::size_t>(k)].src;
        const int t = com[static_cast<std::size_t>(k)].snk;
        row_lo[static_cast<std::size_t>(flow_row_id(k, s))] = d;
        row_up[static_cast<std::size_t>(flow_row_id(k, s))] = d;
        row_lo[static_cast<std::size_t>(flow_row_id(k, t))] = -d;
        row_up[static_cast<std::size_t>(flow_row_id(k, t))] = -d;
    }
    for (int e = 0; e < n_cap; ++e) {
        row_up[static_cast<std::size_t>(n_flow + e)] = kCapNormal;
    }
    std::vector<double> cost(static_cast<std::size_t>(num_col), 1.0);
    HighsLp lp {};
    lp.num_col_ = static_cast<HighsInt>(num_col);
    lp.num_row_ = static_cast<HighsInt>(num_row);
    lp.sense_ = ObjSense::kMinimize;
    lp.offset_ = 0.0;
    lp.col_cost_ = std::move(cost);
    lp.col_lower_ = std::move(col_lo);
    lp.col_upper_ = std::move(col_up);
    lp.row_lower_ = std::move(row_lo);
    lp.row_upper_ = std::move(row_up);
    lp.integrality_.assign(static_cast<std::size_t>(num_col), HighsVarType::kInteger);
    lp.model_name_ = "MCF_COB";
    lp.a_matrix_.format_ = MatrixFormat::kColwise;
    lp.a_matrix_.num_col_ = lp.num_col_;
    lp.a_matrix_.num_row_ = lp.num_row_;
    lp.a_matrix_.start_ = std::move(astart);
    lp.a_matrix_.index_ = std::move(aind);
    lp.a_matrix_.value_ = std::move(aval);
    lp.setMatrixDimensions();
    Highs h {};
    h.setOptionValue("output_flag", false);
    h.setOptionValue("presolve", "on");
    const auto ps = h.passModel(std::move(lp));
    if (ps != HighsStatus::kOk) {
        return {false, 0.0};
    }
    if (h.run() != HighsStatus::kOk) {
        return {false, 0.0};
    }
    if (h.getModelStatus() != HighsModelStatus::kOptimal) {
        return {false, 0.0};
    }
    return {true, h.getObjectiveValue()};
}

} // namespace

auto run_mcf_global_routing_cob_units(
    const std::Vector<Net_cost_record>& records,
    const std::Vector<TobIlpNetAssignment>& ilp_assignments,
    const hardware::Interposer& interposer,
    const circuit::BaseDie& basedie) -> CobMcfRunSummary {
    CobMcfRunSummary out {};
    if (records.size() != ilp_assignments.size()) {
        for (std::size_t u = 0; u < 16; ++u) {
            out.per_cob.push_back(
                CobMcfCobUnitSummary {
                    u, 0, false, 0.0, 0, std::String("assignment size mismatch with records")});
        }
        out.all_ok = false;
        return out;
    }
    std::Vector<Arc> arcs;
    build_base_arcs(arcs);  // build base arcs
    std::set<int> p_cob;
    std::set<int> n_cob;
    add_port_arcs(interposer, basedie, arcs, p_cob, n_cob); // process p/n ports
    for (std::size_t cobu = 0; cobu < 16; ++cobu) {
        std::map<std::String, std::Vector<std::size_t>> by_key;
        for (std::size_t i = 0; i < records.size(); ++i) {
            if (ilp_assignments[i].cob_unit != cobu) {
                continue;
            }
            by_key[mcf_merge_key(records[i])].push_back(i);
        }
        std::Vector<McfK> coms;
        for (const auto& [key, g] : by_key) {
            (void)key;
            build_commodities(records, g, coms);
        }
        const auto t0 = std::chrono::steady_clock::now();
        const auto [ok, obj] = solve_mcf_lp(arcs, p_cob, n_cob, coms);
        const auto t1 = std::chrono::steady_clock::now();
        const int ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
        out.per_cob.push_back(
            CobMcfCobUnitSummary {
                cobu, static_cast<int>(coms.size()), ok, obj, ms,
                ok ? std::String("ok")
                   : std::String("infeasible or empty")});
        if (!ok) {
            out.all_ok = false;
        }
        debug::info_fmt("MCF cob unit {}: commodities={} ok={} obj={:.4g} time={}ms", cobu, coms.size(), ok, obj, ms);
    }
    return out;
}

} // namespace PR_tool
