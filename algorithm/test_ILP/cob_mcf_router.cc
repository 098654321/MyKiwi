#include "cob_mcf_router.hh"

#include "highs.hh"
#include "ilp_types.hh"
#include "mcf_hw_map.hh"
#include "highs/Highs.h"
#include "highs/lp_data/HConst.h"
#include "highs/lp_data/HighsLp.h"

#include "circuit/basedie.hh"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <format>
#include <future>
#include <queue>
#include <map>
#include <set>
#include <stdexcept>
#include <sys/resource.h>
#include <vector>

#include <debug/debug.hh>

namespace PR_tool {

namespace {

using namespace mcf;

const double kCapNormal = 8.0;
const std::size_t kTob0 = 108U; // TOB node 的编号跟在108个COB后面
const int kTob0i = 108;         // 和kTob0一样的意思
const int kVPi = 124;           // P-class node 的编号  
const int kVNi = 125;           // N-class node 的编号
const int kNumNi = 126;         // 总节点数

struct Arc {
    int u{0};
    int v{0};
    bool is_virt{false};
};

struct McfK {
    std::String label;
    std::String origin_name;
    int src{0};
    int snk{0};
    int demand{0};
    int class_id{0};
};

struct McfPathInfo {
    std::String label;
    std::String origin_name;
    int src{0};
    int snk{0};
    int demand{0};
    std::Vector<std::Vector<int>> unit_paths;
};

struct McfSolveResult {
    bool ok{false};
    double objective{0.0};
    int model_status{-1};
    bool has_primal_flow{false};
    std::map<std::pair<int, int>, double> undirected_edge_usage;
    std::map<std::pair<int, int>, double> directed_edge_usage;
    std::Vector<McfPathInfo> paths;
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

auto get_peak_rss_mb() -> double {
    rusage usage {};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return 0.0;
    }
#if defined(__APPLE__)
    return static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0);
#else
    return static_cast<double>(usage.ru_maxrss) / 1024.0;
#endif
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
    const hardware::Interposer& interposer,
    const circuit::BaseDie& basedie,
    std::size_t cob_unit,
    std::Vector<Arc>& arcs,
    std::set<int>& p_cob,
    std::set<int>& n_cob) {
    (void)interposer;
    p_cob.clear();
    n_cob.clear();
    for (const auto& tc : basedie.pose_ports()) {
        if (map_track(tc.index) != cob_unit) {
            continue;
        }
        const auto c = track_to_cob(tc);
        const int lin = static_cast<int>(cob_to_linear(c));
        p_cob.insert(lin);
        arcs.push_back(Arc{lin, kVPi, true});
    }
    for (const auto& tc : basedie.nege_ports()) {
        if (map_track(tc.index) != cob_unit) {
            continue;
        }
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

static auto is_cob_node(int n) -> bool {
    return n >= 0 && n < kTob0i;
}

auto arc_class_ok(int cid, int u, int v, const std::set<int>& p_cob, const std::set<int>& n_cob) -> bool {
    if (u < 0 || v < 0 || u >= kNumNi || v >= kNumNi) {
        return false;
    }
    if (u == kVPi || u == kVNi || v == kVPi || v == kVNi) {
        if (u == kVPi || v == kVPi) {
            return cid == cls_p();
        }
        if (u == kVNi || v == kVNi) {
            return cid == cls_n();
        }
    }
    // Constraint set 4: only P-class commodities may use edges incident to COBs in P; only N-class for N.
    if (cid != cls_p()) {
        if ((is_cob_node(u) && p_cob.count(u) != 0) || (is_cob_node(v) && p_cob.count(v) != 0)) {
            return false;
        }
    }
    if (cid != cls_n()) {
        if ((is_cob_node(u) && n_cob.count(u) != 0) || (is_cob_node(v) && n_cob.count(v) != 0)) {
            return false;
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
            out.push_back(McfK {
                std::format("{}#T{}", t0.net_name, tob_lin),
                t0.origin_key.empty() ? t0.net_name : t0.origin_key,
                static_cast<int>(kTob0 + tob_lin),
                vdst,
                d,
                cls});
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
        if (all_same) {         // 所有的起始/终止bump都在同一个COBUnit上
            int ssum = 0;
            for (const auto i : gids) {
                ssum += demand_bits_ceil(records[i]);
            }
            out.push_back(McfK {
                t0.net_name,
                t0.origin_key.empty() ? t0.net_name : t0.origin_key,
                s0,
                t0b,
                ssum,
                cls_b()});
            return;
        }
        for (const auto i : gids) { // 分成几个不同的COBUnit
            const auto& r = records[i];
            int ss = static_cast<int>(kTob0 + r.start_bumps[0].TOB);
            int tt = static_cast<int>(kTob0 + r.end_bumps[0].TOB);
            out.push_back(McfK {
                r.net_name,
                r.origin_key.empty() ? r.net_name : r.origin_key,
                ss,
                tt,
                demand_bits_ceil(r),
                cls_b()});
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
                out.push_back(McfK {
                    r.net_name,
                    r.origin_key.empty() ? r.net_name : r.origin_key,
                    src,
                    snk,
                    demand_bits_ceil(r),
                    cls_t()});
            }
            else if (r.mcf_has_start_track) {
                // TrackToBump: ILP model stores the lone bump in start_bumps
                if (r.start_bumps.empty()) {
                    throw std::runtime_error(std::format("MCF: Tnet no bump (track->bump): {}", r.net_name));
                }
                const int src = static_cast<int>(cob_to_linear(track_to_cob(r.mcf_start_track)));
                const int snk = static_cast<int>(kTob0 + r.start_bumps[0].TOB);
                out.push_back(McfK {
                    r.net_name,
                    r.origin_key.empty() ? r.net_name : r.origin_key,
                    src,
                    snk,
                    demand_bits_ceil(r),
                    cls_t()});
            }
            else {
                throw std::runtime_error(std::format("MCF: Tnet no track endpoint: {}", r.net_name));
            }
        }
    }
}

auto prepare_cob_unit_commodities(
    const std::Vector<Net_cost_record>& records,
    const std::Vector<TobIlpNetAssignment>& ilp_assignments,
    std::size_t cob_unit) -> std::Vector<McfK> {
    std::map<std::String, std::Vector<std::size_t>> by_key;
    for (std::size_t i = 0; i < records.size(); ++i) {
        if (ilp_assignments[i].cob_unit != cob_unit) {
            continue;
        }
        by_key[mcf_merge_key(records[i])].push_back(i);
    }
    std::Vector<McfK> coms;
    for (const auto& [key, g] : by_key) {
        (void)key;
        build_commodities(records, g, coms);
    }
    return coms;
}

auto flow_row_id(int k, int node) -> int {
    return k * kNumNi + node;
}

auto solve_mcf_lp(
    const std::Vector<Arc>& arcs, const std::set<int>& p_cob, const std::set<int>& n_cob, const std::Vector<McfK>& com) -> McfSolveResult {
    if (com.empty()) {
        return McfSolveResult {true, 0.0, static_cast<int>(HighsModelStatus::kOptimal), true, {}, {}, {}};
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
        return McfSolveResult {false, 0.0, static_cast<int>(HighsModelStatus::kNotset), false, {}, {}, {}};
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
        return McfSolveResult {false, 0.0, static_cast<int>(HighsModelStatus::kNotset), false, {}, {}, {}};
    }
    if (h.run() != HighsStatus::kOk) {
        return McfSolveResult {false, 0.0, static_cast<int>(h.getModelStatus()), false, {}, {}, {}};
    }
    const auto model_status = h.getModelStatus();
    if (model_status != HighsModelStatus::kOptimal) {
        return McfSolveResult {false, 0.0, static_cast<int>(model_status), false, {}, {}, {}};
    }

    std::Vector<McfPathInfo> all_paths;
    all_paths.resize(static_cast<std::size_t>(K));
    std::vector<std::map<std::pair<int, int>, int>> edge_count(static_cast<std::size_t>(K));
    std::map<std::pair<int, int>, double> undirected_usage;
    std::map<std::pair<int, int>, double> directed_usage;
    const auto sol = h.getSolution();
    for (int j = 0; j < num_col; ++j) {
        const int f = static_cast<int>(std::lround(sol.col_value[static_cast<std::size_t>(j)]));
        if (f <= 0) {
            continue;
        }
        const int k = vars[static_cast<std::size_t>(j)].k;
        const int a = vars[static_cast<std::size_t>(j)].a;
        const int u = arcs[static_cast<std::size_t>(a)].u;
        const int v = arcs[static_cast<std::size_t>(a)].v;
        directed_usage[{u, v}] += static_cast<double>(f);
        int u1 = u;
        int v1 = v;
        if (u1 > v1) {
            std::swap(u1, v1);
        }
        undirected_usage[{u1, v1}] += static_cast<double>(f);
        edge_count[static_cast<std::size_t>(k)][{u, v}] += f;
    }
    for (int k = 0; k < K; ++k) {
        const auto& c = com[static_cast<std::size_t>(k)];
        auto& info = all_paths[static_cast<std::size_t>(k)];
        info.label = c.label;
        info.origin_name = c.origin_name;
        info.src = c.src;
        info.snk = c.snk;
        info.demand = c.demand;
        auto& ec = edge_count[static_cast<std::size_t>(k)];
        for (int flow_id = 0; flow_id < c.demand; ++flow_id) {
            std::array<int, kNumNi> prev {};
            prev.fill(-1);
            std::queue<int> q;
            q.push(c.src);
            prev[static_cast<std::size_t>(c.src)] = c.src;
            while (!q.empty() && prev[static_cast<std::size_t>(c.snk)] < 0) {
                const int u = q.front();
                q.pop();
                for (const auto& [e, cnt] : ec) {
                    if (cnt <= 0 || e.first != u) {
                        continue;
                    }
                    const int v = e.second;
                    if (prev[static_cast<std::size_t>(v)] >= 0) {
                        continue;
                    }
                    prev[static_cast<std::size_t>(v)] = u;
                    q.push(v);
                }
            }
            if (prev[static_cast<std::size_t>(c.snk)] < 0) {
                break;
            }
            std::Vector<int> nodes;
            int cur = c.snk;
            while (cur != c.src) {
                nodes.push_back(cur);
                cur = prev[static_cast<std::size_t>(cur)];
            }
            nodes.push_back(c.src);
            std::reverse(nodes.begin(), nodes.end());
            for (std::size_t i = 0; i + 1 < nodes.size(); ++i) {
                auto key = std::make_pair(nodes[i], nodes[i + 1]);
                auto it = ec.find(key);
                if (it != ec.end() && it->second > 0) {
                    it->second -= 1;
                }
            }
            info.unit_paths.push_back(std::move(nodes));
        }
    }
    return McfSolveResult {
        true,
        h.getObjectiveValue(),
        static_cast<int>(model_status),
        true,
        std::move(undirected_usage),
        std::move(directed_usage),
        std::move(all_paths)};
}

} // namespace

struct CobSolveDetail {
    CobMcfCobUnitSummary summary;
    std::Vector<McfPathInfo> paths;
};

static auto solve_prepared_cob_unit(
    const std::Vector<Arc>& base_arcs,
    const hardware::Interposer& interposer,
    const circuit::BaseDie& basedie,
    std::size_t cobu,
    const std::Vector<McfK>& coms) -> CobSolveDetail {
    std::Vector<Arc> arcs = base_arcs;
    std::set<int> p_cob;
    std::set<int> n_cob;
    add_port_arcs(interposer, basedie, cobu, arcs, p_cob, n_cob);      // add port arcs for pose/nege ports
    const auto t0 = std::chrono::steady_clock::now();
    const auto res = solve_mcf_lp(arcs, p_cob, n_cob, coms);
    const auto t1 = std::chrono::steady_clock::now();
    const int ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
    debug::info_fmt("MCF cob unit {}: commodities={} ok={} obj={:.4g} time={}ms", cobu, coms.size(), res.ok, res.objective, ms);
    if (!res.ok) {
        const auto node_name = [](int n) -> std::String {
            if (n == kVPi) {
                return "V_P";
            }
            if (n == kVNi) {
                return "V_N";
            }
            if (n >= kTob0i && n < kVPi) {
                return std::format("TOB{}", n - kTob0i);
            }
            if (n >= 0 && n < kTob0i) {
                const auto c = linear_to_cob(static_cast<std::size_t>(n));
                return std::format("COB({},{})", c.row, c.col);
            }
            return std::format("N{}", n);
        };
        debug::debug_fmt(
            "MCF cob unit {} failure detail: model_status={}, p_cob_count={}, n_cob_count={}, commodities={}",
            cobu,
            res.model_status,
            p_cob.size(),
            n_cob.size(),
            coms.size());
        for (std::size_t i = 0; i < coms.size(); ++i) {
            const auto& c = coms[i];
            debug::debug_fmt(
                "  commodity[{}] label={} src={} snk={} demand={}",
                i,
                c.label,
                c.src,
                c.snk,
                c.demand);
        }
        debug::debug_fmt("MCF cob unit {} edge usage/capacity:", cobu);
        for (const auto& a : arcs) {
            const int u = a.u;
            const int v = a.v;
            if (a.is_virt) {
                if (res.has_primal_flow) {
                    auto it = res.directed_edge_usage.find({u, v});
                    const double used = (it == res.directed_edge_usage.end()) ? 0.0 : it->second;
                    debug::debug_fmt("  edge {} -> {} : use={:.0f}/INF", node_name(u), node_name(v), used);
                }
                else {
                    debug::debug_fmt("  edge {} -> {} : use=N/A/INF", node_name(u), node_name(v));
                }
                continue;
            }
            int u1 = u;
            int v1 = v;
            if (u1 > v1) {
                std::swap(u1, v1);
            }
            if (res.has_primal_flow) {
                auto it = res.undirected_edge_usage.find({u1, v1});
                const double used = (it == res.undirected_edge_usage.end()) ? 0.0 : it->second;
                debug::debug_fmt(
                    "  edge {} <-> {} : use={:.0f}/{:.0f}",
                    node_name(u1),
                    node_name(v1),
                    used,
                    kCapNormal);
            }
            else {
                debug::debug_fmt("  edge {} <-> {} : use=N/A/{:.0f}", node_name(u1), node_name(v1), kCapNormal);
            }
        }
    }
    return CobSolveDetail {
        CobMcfCobUnitSummary {
            cobu,
            static_cast<int>(coms.size()),
            res.ok,
            res.objective,
            ms,
            res.ok ? std::String("ok") : std::String("infeasible or empty")},
        std::move(res.paths)};
}

static auto node_to_text(int n) -> std::String {
    if (n == kVPi) {
        return "V_P";
    }
    if (n == kVNi) {
        return "V_N";
    }
    if (n >= kTob0i && n < kVPi) {
        return std::format("TOB{}", n - kTob0i);
    }
    if (n >= 0 && n < kTob0i) {
        const auto c = linear_to_cob(static_cast<std::size_t>(n));
        return std::format("COB({},{})", c.row, c.col);
    }
    return std::format("N{}", n);
}

static auto path_to_text(const std::Vector<int>& nodes) -> std::String {
    if (nodes.empty()) {
        return "(empty)";
    }
    auto out = std::String {};
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (i != 0) {
            out += " -> ";
        }
        out += node_to_text(nodes[i]);
    }
    return out;
}

static auto path_length_without_virtual_nodes(const std::Vector<int>& nodes) -> int {
    int len = 0;
    for (const auto n : nodes) {
        if (n == kVPi || n == kVNi) {
            continue;
        }
        if ((n >= 0 && n < kTob0i) || (n >= kTob0i && n < kVPi)) {
            len += 1;
        }
    }
    return len;
}

static auto is_tob_node(int n) -> bool {
    return n >= kTob0i && n < kVPi;
}

static auto is_cob_node_any(int n) -> bool {
    return n >= 0 && n < kTob0i;
}

static auto is_physically_adjacent_path_step(int u, int v) -> bool {
    if (u == kVPi || u == kVNi || v == kVPi || v == kVNi) {
        return true;
    }
    if (is_cob_node_any(u) && is_cob_node_any(v)) {
        const auto cu = linear_to_cob(static_cast<std::size_t>(u));
        const auto cv = linear_to_cob(static_cast<std::size_t>(v));
        const auto dr = std::llabs(cu.row - cv.row);
        const auto dc = std::llabs(cu.col - cv.col);
        return dr + dc == 1;
    }
    if (is_tob_node(u) && is_cob_node_any(v)) {
        const auto [tr, tc] = tob_index_from_linear(static_cast<std::size_t>(u - kTob0i));
        const auto [a, b] = tob_pair_cob_coords(tr, tc);
        const auto cv = linear_to_cob(static_cast<std::size_t>(v));
        return (cv.row == a.row && cv.col == a.col) || (cv.row == b.row && cv.col == b.col);
    }
    if (is_cob_node_any(u) && is_tob_node(v)) {
        return is_physically_adjacent_path_step(v, u);
    }
    return true;
}

auto run_mcf_global_routing_cob_units(
    const std::Vector<Net_cost_record>& records,
    const std::Vector<TobIlpNetAssignment>& ilp_assignments,
    const hardware::Interposer& interposer,
    const circuit::BaseDie& basedie,
    const bool enable_mcf_parallel) -> CobMcfRunSummary {
    const auto mcf_start = std::chrono::steady_clock::now();
    const double mcf_peak_before_mb = get_peak_rss_mb();
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
    std::Vector<Arc> base_arcs;
    build_base_arcs(base_arcs);     // build graph 

    std::array<std::Vector<McfK>, 16> prep {};
    for (std::size_t cobu = 0; cobu < 16; ++cobu) {
        prep[cobu] = prepare_cob_unit_commodities(records, ilp_assignments, cobu);      // make MCF
    }

    out.per_cob.resize(16);
    std::array<std::Vector<McfPathInfo>, 16> unit_paths {};
    if (enable_mcf_parallel) {
        std::Vector<std::future<CobSolveDetail>> futs;
        futs.reserve(16);
        for (std::size_t cobu = 0; cobu < 16; ++cobu) {
            futs.push_back(std::async(std::launch::async, [&, cobu, coms = prep[cobu]]() {
                return solve_prepared_cob_unit(base_arcs, interposer, basedie, cobu, coms);       // solve MCF
            }));
        }
        for (std::size_t cobu = 0; cobu < 16; ++cobu) {
            auto detail = futs[cobu].get();
            out.per_cob[cobu] = std::move(detail.summary);
            unit_paths[cobu] = std::move(detail.paths);
            if (!out.per_cob[cobu].ok) {
                out.all_ok = false;
            }
        }
    }
    else {
        for (std::size_t cobu = 0; cobu < 16; ++cobu) {
            auto detail = solve_prepared_cob_unit(base_arcs, interposer, basedie, cobu, prep[cobu]);
            out.per_cob[cobu] = std::move(detail.summary);
            unit_paths[cobu] = std::move(detail.paths);
            if (!out.per_cob[cobu].ok) {
                out.all_ok = false;
            }
        }
    }

    std::map<std::String, std::set<std::size_t>> net_units;
    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto& net = records[i].origin_key.empty() ? records[i].net_name : records[i].origin_key;
        net_units[net].insert(ilp_assignments[i].cob_unit);
    }
    std::map<std::String, std::map<std::size_t, std::Vector<McfPathInfo>>> net_paths;
    for (std::size_t cobu = 0; cobu < 16; ++cobu) {
        for (const auto& p : unit_paths[cobu]) {
            net_paths[p.origin_name][cobu].push_back(p);
        }
    }
    for (const auto& [net_name, units] : net_units) {
        auto unit_text = std::String {};
        bool first = true;
        for (const auto u : units) {
            if (!first) {
                unit_text += ",";
            }
            first = false;
            unit_text += std::format("{}", u);
        }
        debug::info_fmt("MCF net \"{}\": used COBUnits [{}]", net_name, unit_text);
        const auto np_it = net_paths.find(net_name);
        if (np_it == net_paths.end()) {
            debug::info_fmt("  unit detail: no MCF commodity generated");
            continue;
        }
        for (const auto& [cobu, infos] : np_it->second) {
            debug::info_fmt("  COBUnit {}:", cobu);
            for (const auto& info : infos) {
                debug::info_fmt(
                    "    commodity \"{}\" demand={} start={} end={}",
                    info.label,
                    info.demand,
                    node_to_text(info.src),
                    node_to_text(info.snk));
                if (info.unit_paths.empty()) {
                    debug::info("      path: (no extractable unit path from solution)");
                    continue;
                }
                for (std::size_t pi = 0; pi < info.unit_paths.size(); ++pi) {
                    const int plen = path_length_without_virtual_nodes(info.unit_paths[pi]);
                    debug::info_fmt("      path#{} (length={}) {}", pi, plen, path_to_text(info.unit_paths[pi]));
                    const auto& nodes = info.unit_paths[pi];
                    for (std::size_t ni = 0; ni + 1 < nodes.size(); ++ni) {
                        if (is_physically_adjacent_path_step(nodes[ni], nodes[ni + 1])) {
                            continue;
                        }
                        debug::warning_fmt(
                            "      adjacency warning: path#{} has non-adjacent step {} -> {}",
                            pi,
                            node_to_text(nodes[ni]),
                            node_to_text(nodes[ni + 1]));
                    }
                }
            }
        }
    }
    const auto mcf_end = std::chrono::steady_clock::now();
    const auto mcf_ms = std::chrono::duration_cast<std::chrono::milliseconds>(mcf_end - mcf_start).count();
    const double mcf_peak_after_mb = get_peak_rss_mb();
    const double mcf_stage_peak_delta_mb = std::max(0.0, mcf_peak_after_mb - mcf_peak_before_mb);
    debug::info_fmt(
        "MCF global routing summary: elapsed={} ms, peak_rss={:.2f} MB, stage_peak_delta={:.2f} MB",
        mcf_ms,
        mcf_peak_after_mb,
        mcf_stage_peak_delta_mb);
    return out;
}

} // namespace PR_tool
