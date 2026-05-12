#include "cob_mcf_router.hh"

#include "ilp_speedup.hh"
#include "mcf_hw_map.hh"

#include "circuit/basedie.hh"
#include "debug/debug.hh"
#include "hardware/cob/cobunit.hh"
#include "highs/Highs.h"
#include "highs/lp_data/HConst.h"
#include "highs/lp_data/HighsLp.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <format>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <tuple>
#include <utility>
#include <vector>

namespace PR_tool {

namespace {

using namespace mcf;

enum class McfClass : int {
    Plain = 0,
    P = 1,
    N = 2
};

struct NodeMeta {
    bool is_virtual{false};
    int virtual_kind{0}; // 0: physical, 1: VP, 2: VN
    std::size_t unit{0};
    int cob{-1};
    std::size_t track{0};
};

struct Arc {
    int u{0};
    int v{0};
    bool is_virtual{false};
    bool is_turn{false};
    std::size_t unit{0};
    int cob{-1};
    std::size_t track_in{0};
    std::size_t track_out{0};
};

struct GlobalGraph {
    int rows{0};
    int cols{0};
    int num_cob{0};
    int vp_node{-1};
    int vn_node{-1};
    std::Vector<NodeMeta> nodes;
    std::Vector<Arc> arcs;
    std::map<std::tuple<std::size_t, int, std::size_t>, int> node_id_by_key;
    std::set<std::pair<int, int>> directed_arc_set;
};

struct PreparedCommodity {
    std::String label;
    std::String origin_name;
    std::size_t record_index{0};
    std::size_t record_id{0};
    std::size_t cob_unit{0};
    std::size_t start_track{0};
    std::size_t end_track{0};
    int src{-1};
    int snk{-1};
    int demand{1};
    McfClass cls{McfClass::Plain};
    bool is_bus{false};
    std::String bus_key;
    std::Vector<IlpReachStep> reach_steps;
    std::Vector<int> bbox_cobs;
};

struct StageSolveResult {
    bool ok{false};
    std::String message;
    double objective{0.0};
    int model_status{static_cast<int>(HighsModelStatus::kNotset)};
    std::map<std::pair<int, int>, int> used_edges;
    std::map<int, int> used_nodes;
    std::Vector<McfPathInfo> paths;
};

struct ArcVar {
    int k{0};
    int a{0};
};

struct OVar {
    int k{0};
    int node{0};
};

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

auto unit_bank(const std::size_t unit) -> std::size_t {
    return unit < 8 ? 0 : 1;
}

auto unit_local(const std::size_t unit) -> std::size_t {
    return unit % 8;
}

auto track_from_unit_inner(const std::size_t unit, const std::size_t inner) -> std::size_t {
    return unit_bank(unit) * 64 + inner * 8 + unit_local(unit);
}

auto node_text(const GlobalGraph& g, const int node) -> std::String {
    if (node < 0 || node >= static_cast<int>(g.nodes.size())) {
        return std::format("N{}", node);
    }
    const auto& meta = g.nodes[static_cast<std::size_t>(node)];
    if (meta.is_virtual) {
        return meta.virtual_kind == 1 ? std::String("V_P") : std::String("V_N");
    }
    const auto c = linear_to_cob(static_cast<std::size_t>(meta.cob));
    return std::format("U{} COB({},{}) T{}", meta.unit, c.row, c.col, meta.track);
}

auto add_node(GlobalGraph& g, const NodeMeta& node) -> int {
    const int id = static_cast<int>(g.nodes.size());
    g.nodes.push_back(node);
    if (!node.is_virtual) {
        g.node_id_by_key[{node.unit, node.cob, node.track}] = id;
    }
    return id;
}

auto get_node_id(const GlobalGraph& g, const std::size_t unit, const int cob, const std::size_t track) -> int {
    const auto it = g.node_id_by_key.find({unit, cob, track});
    if (it == g.node_id_by_key.end()) {
        return -1;
    }
    return it->second;
}

auto add_arc(
    GlobalGraph& g,
    const int u,
    const int v,
    const bool is_virtual,
    const bool is_turn,
    const std::size_t unit,
    const int cob,
    const std::size_t track_in,
    const std::size_t track_out
) -> void {
    if (u < 0 || v < 0 || u >= static_cast<int>(g.nodes.size()) || v >= static_cast<int>(g.nodes.size())) {
        return;
    }
    if (g.directed_arc_set.contains({u, v})) {
        return;
    }
    g.directed_arc_set.insert({u, v});
    g.arcs.push_back(Arc {u, v, is_virtual, is_turn, unit, cob, track_in, track_out});
}

auto build_track_graph(const CobMcfGridDims& grid) -> GlobalGraph {
    GlobalGraph g {};
    g.rows = grid.rows;
    g.cols = grid.cols;
    g.num_cob = grid.rows * grid.cols;

    for (std::size_t unit = 0; unit < 16; ++unit) {
        const auto tracks = cobunit_to_tracks(unit);        // 128个
        for (int cob = 0; cob < g.num_cob; ++cob) {
            for (const auto tr : tracks) {                                      // 一个cob建128个node
                add_node(g, NodeMeta {false, 0, unit, cob, tr});
            }
        }
    }
    g.vp_node = add_node(g, NodeMeta {true, 1, 0, -1, 0});
    g.vn_node = add_node(g, NodeMeta {true, 2, 0, -1, 0});

    // Mesh edges between neighboring COB tiles: same track index.
    for (std::size_t unit = 0; unit < 16; ++unit) {
        const auto tracks = cobunit_to_tracks(unit);
        for (int r = 0; r < g.rows; ++r) {
            for (int c = 0; c < g.cols; ++c) {
                const int cob = r * g.cols + c;
                for (const auto tr : tracks) {
                    const int u = get_node_id(g, unit, cob, tr);
                    if (r + 1 < g.rows) {
                        const int cob_d = (r + 1) * g.cols + c;
                        const int v = get_node_id(g, unit, cob_d, tr);
                        add_arc(g, u, v, false, false, unit, -1, tr, tr);
                        add_arc(g, v, u, false, false, unit, -1, tr, tr);
                    }
                    if (c + 1 < g.cols) {
                        const int cob_r = r * g.cols + (c + 1);
                        const int v = get_node_id(g, unit, cob_r, tr);
                        add_arc(g, u, v, false, false, unit, -1, tr, tr);
                        add_arc(g, v, u, false, false, unit, -1, tr, tr);
                    }
                }
            }
        }
    }

    // Intra-COB Wilton turn edges.
    constexpr auto dirs = std::array {
        hardware::COBDirection::Left,
        hardware::COBDirection::Right,
        hardware::COBDirection::Up,
        hardware::COBDirection::Down
    };
    for (std::size_t unit = 0; unit < 16; ++unit) {
        const auto u_local = unit_local(unit);
        const auto bank = unit_bank(unit);
        for (int cob = 0; cob < g.num_cob; ++cob) {
            for (std::size_t inner = 0; inner < 8; ++inner) {
                const auto track_in = bank * 64 + inner * 8 + u_local;
                for (const auto from : dirs) {
                    for (const auto to : dirs) {
                        if (from == to) {
                            continue;
                        }
                        const auto mapped = static_cast<std::size_t>(hardware::COBUnit::index_map(from, inner, to));
                        const auto track_out = bank * 64 + mapped * 8 + u_local;
                        if (track_in == track_out) {
                            continue;
                        }
                        const int u = get_node_id(g, unit, cob, track_in);
                        const int v = get_node_id(g, unit, cob, track_out);
                        add_arc(g, u, v, false, true, unit, cob, track_in, track_out);
                    }
                }
            }
        }
    }
    return g;
}

auto tob_from_linear(const std::size_t tob_linear) -> hardware::TOBCoord {
    const auto width = static_cast<std::size_t>(hardware::Interposer::TOB_ARRAY_WIDTH);
    return hardware::TOBCoord {
        static_cast<std::i64>(tob_linear / width),
        static_cast<std::i64>(tob_linear % width)};
}

auto tob_anchor_cob(const std::size_t tob_linear) -> hardware::COBCoord {
    const auto tob = tob_from_linear(tob_linear);
    return hardware::COBCoord {
        static_cast<std::i64>(1 + 2 * tob.row),
        static_cast<std::i64>(3 * tob.col)};
}

auto bbox_cob_indices(
    const CobMcfGridDims& grid,
    const hardware::COBCoord& a,
    const hardware::COBCoord& b
) -> std::Vector<int> {
    std::Vector<int> out {};
    const auto r0 = std::max<std::i64>(0, std::min(a.row, b.row));
    const auto r1 = std::min<std::i64>(grid.rows - 1, std::max(a.row, b.row));
    const auto c0 = std::max<std::i64>(0, std::min(a.col, b.col));
    const auto c1 = std::min<std::i64>(grid.cols - 1, std::max(a.col, b.col));
    for (std::i64 r = r0; r <= r1; ++r) {
        for (std::i64 c = c0; c <= c1; ++c) {
            out.push_back(static_cast<int>(r * grid.cols + c));
        }
    }
    return out;
}

auto node_from_bump_track(
    const GlobalGraph& g,
    const std::size_t unit,
    const std::size_t tob_linear,
    const std::size_t track
) -> int {
    const auto [tr, tc] = tob_index_from_linear(tob_linear);
    const auto pair = tob_pair_cob_coords(tr, tc);
    const auto c0 = static_cast<int>(cob_to_linear(pair.first));
    const auto c1 = static_cast<int>(cob_to_linear(pair.second));
    const int n0 = get_node_id(g, unit, c0, track);
    if (n0 >= 0) {
        return n0;
    }
    return get_node_id(g, unit, c1, track);
}

auto node_from_track_coord(
    const GlobalGraph& g,
    const std::size_t unit,
    const hardware::TrackCoord& tc,
    const std::size_t track
) -> int {
    const auto c = track_to_cob(tc);
    return get_node_id(g, unit, static_cast<int>(cob_to_linear(c)), track);
}

auto prepare_commodities(
    const std::Vector<Net_cost_record>& records,
    const TobIlpResult& ilp_result,
    const CobMcfGridDims& grid,
    GlobalGraph& graph
) -> std::Vector<PreparedCommodity> {
    auto out = std::Vector<PreparedCommodity> {};
    if (records.size() != ilp_result.record_track_endpoints.size()) {
        throw std::runtime_error("MCF prepare: record_track_endpoints size mismatch");
    }

    auto split_count = std::map<std::String, int> {};
    for (const auto& record : records) {
        if (record.type == Net_type::PNnet) {
            continue;
        }
        const auto origin = record.origin_key.empty() ? record.net_name : record.origin_key;
        split_count[std::format("{}#{}", origin, static_cast<int>(record.type))] += 1;
    }

    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto& record = records[i];
        const auto& endpoint = ilp_result.record_track_endpoints[i];
        PreparedCommodity c {};
        c.label = std::format("{}#{}", record.net_name, record.record_id);
        c.origin_name = record.origin_key.empty() ? record.net_name : record.origin_key;
        c.record_index = i;
        c.record_id = record.record_id;
        c.cob_unit = endpoint.cob_unit;
        c.start_track = endpoint.start_track;
        c.end_track = endpoint.end_track;
        c.demand = 1;
        c.bus_key = c.origin_name;
        c.is_bus = (record.type != Net_type::PNnet)
                   && split_count[std::format("{}#{}", c.origin_name, static_cast<int>(record.type))] > 1;

        if (!endpoint.has_start_track) {
            debug::warning_fmt("MCF prepare: record {} has no start track", record.net_name);
            continue;
        }
        if (map_track(endpoint.start_track) != c.cob_unit) {
            debug::warning_fmt(
                "MCF prepare: start track {} not in assigned cobunit {} for record {}",
                endpoint.start_track,
                c.cob_unit,
                record.net_name);
            continue;
        }

        hardware::COBCoord bbox_start {};
        hardware::COBCoord bbox_end {};
        if (record.type == Net_type::Tnet && record.mcf_has_start_track) {
            c.src = node_from_track_coord(graph, c.cob_unit, record.mcf_start_track, endpoint.start_track);
            bbox_start = track_to_cob(record.mcf_start_track);
        }
        else {
            if (record.start_bumps.empty()) {
                continue;
            }
            c.src = node_from_bump_track(graph, c.cob_unit, record.start_bumps.front().TOB, endpoint.start_track);
            bbox_start = tob_anchor_cob(record.start_bumps.front().TOB);
        }

        if (record.type == Net_type::PNnet) {
            if (record.power_kind == IlpPowerKind::Pose) {
                c.cls = McfClass::P;
                c.snk = graph.vp_node;
            }
            else {
                c.cls = McfClass::N;
                c.snk = graph.vn_node;
            }

            auto virtual_edges_added = 0;
            for (const auto& [end_track, start_tracks] : record.starttrack_by_endtrack) {
                if (!std::binary_search(start_tracks.begin(), start_tracks.end(), endpoint.start_track)) {
                    continue;
                }
                if (map_track(end_track) != c.cob_unit) {
                    continue;
                }
                if (!record.pn_end_track_coord_by_index.contains(end_track)) {
                    continue;
                }
                const auto& tc = record.pn_end_track_coord_by_index.at(end_track);
                const auto n_end = node_from_track_coord(graph, c.cob_unit, tc, end_track);
                if (n_end < 0) {
                    continue;
                }
                add_arc(
                    graph,
                    n_end,
                    c.snk,
                    true,
                    false,
                    c.cob_unit,
                    -1,
                    end_track,
                    end_track);
                add_arc(
                    graph,
                    c.snk,
                    n_end,
                    true,
                    false,
                    c.cob_unit,
                    -1,
                    end_track,
                    end_track);
                bbox_end = track_to_cob(tc);
                c.end_track = end_track;
                virtual_edges_added += 1;
            }
            if (virtual_edges_added == 0 && endpoint.has_end_track
                && record.pn_end_track_coord_by_index.contains(endpoint.end_track)) {
                const auto& tc = record.pn_end_track_coord_by_index.at(endpoint.end_track);
                const auto n_end = node_from_track_coord(graph, c.cob_unit, tc, endpoint.end_track);
                if (n_end >= 0) {
                    add_arc(graph, n_end, c.snk, true, false, c.cob_unit, -1, endpoint.end_track, endpoint.end_track);
                    add_arc(graph, c.snk, n_end, true, false, c.cob_unit, -1, endpoint.end_track, endpoint.end_track);
                    bbox_end = track_to_cob(tc);
                    c.end_track = endpoint.end_track;
                }
            }
        }
        else if (record.type == Net_type::Tnet) {
            c.cls = McfClass::Plain;
            if (record.mcf_has_end_track) {
                if (!endpoint.has_end_track) {
                    continue;
                }
                c.snk = node_from_track_coord(graph, c.cob_unit, record.mcf_end_track, endpoint.end_track);
                bbox_end = track_to_cob(record.mcf_end_track);
            }
            else {
                if (!endpoint.has_end_track || record.start_bumps.empty()) {
                    continue;
                }
                c.snk = node_from_bump_track(graph, c.cob_unit, record.start_bumps.front().TOB, endpoint.end_track);
                bbox_end = tob_anchor_cob(record.start_bumps.front().TOB);
            }
        }
        else {
            c.cls = McfClass::Plain;
            if (record.end_bumps.empty() || !endpoint.has_end_track) {
                continue;
            }
            c.snk = node_from_bump_track(graph, c.cob_unit, record.end_bumps.front().TOB, endpoint.end_track);
            bbox_end = tob_anchor_cob(record.end_bumps.front().TOB);
        }

        if (c.src < 0 || c.snk < 0) {
            debug::warning_fmt(
                "MCF prepare: unresolved endpoint node for record {} (src={}, snk={})",
                record.net_name,
                c.src,
                c.snk);
            continue;
        }

        c.bbox_cobs = bbox_cob_indices(grid, bbox_start, bbox_end);
        if (endpoint.has_end_track) {
            const auto it_end = record.reach_by_end_start.find(endpoint.end_track);
            if (it_end != record.reach_by_end_start.end()) {
                const auto it_start = it_end->second.find(endpoint.start_track);
                if (it_start != it_end->second.end()) {
                    c.reach_steps = it_start->second;
                }
            }
        }
        out.push_back(std::move(c));
    }
    return out;
}

auto arc_usable_for_class(
    const Arc& arc,
    const McfClass cls,
    const std::size_t unit,
    const int vp,
    const int vn
) -> bool {
    if (arc.unit != unit) {
        return false;
    }
    if (arc.u == vp || arc.v == vp) {
        return cls == McfClass::P;
    }
    if (arc.u == vn || arc.v == vn) {
        return cls == McfClass::N;
    }
    return true;
}

auto extract_path(
    const int src,
    const int snk,
    std::map<std::pair<int, int>, int>& edge_count,
    const int num_nodes
) -> std::Vector<int> {
    auto prev = std::Vector<int>(static_cast<std::size_t>(num_nodes), -1);
    std::queue<int> q;
    q.push(src);
    prev[static_cast<std::size_t>(src)] = src;
    while (!q.empty() && prev[static_cast<std::size_t>(snk)] < 0) {
        const auto u = q.front();
        q.pop();
        for (const auto& [e, cnt] : edge_count) {
            if (cnt <= 0 || e.first != u) {
                continue;
            }
            const auto v = e.second;
            if (prev[static_cast<std::size_t>(v)] >= 0) {
                continue;
            }
            prev[static_cast<std::size_t>(v)] = u;
            q.push(v);
        }
    }
    if (prev[static_cast<std::size_t>(snk)] < 0) {
        return {};
    }
    auto nodes = std::Vector<int> {};
    auto cur = snk;
    while (cur != src) {
        nodes.push_back(cur);
        cur = prev[static_cast<std::size_t>(cur)];
    }
    nodes.push_back(src);
    std::reverse(nodes.begin(), nodes.end());
    for (std::size_t i = 0; i + 1 < nodes.size(); ++i) {
        auto key = std::make_pair(nodes[i], nodes[i + 1]);
        if (edge_count.contains(key) && edge_count[key] > 0) {
            edge_count[key] -= 1;
        }
    }
    return nodes;
}

auto solve_stage(
    const std::String& stage_name,
    const GlobalGraph& graph,
    const std::Vector<PreparedCommodity>& commodities,
    const std::Vector<std::size_t>& commodity_ids,
    const std::map<std::pair<int, int>, int>& edge_capacity_override,
    const std::map<int, int>& node_capacity_override,
    const bool enforce_bus_equal_length
) -> StageSolveResult {
    StageSolveResult out {};
    if (commodity_ids.empty()) {
        out.ok = true;
        out.message = "empty stage";
        out.model_status = static_cast<int>(HighsModelStatus::kOptimal);
        return out;
    }

    const auto K = static_cast<int>(commodity_ids.size());
    const auto A = static_cast<int>(graph.arcs.size());
    const auto N = static_cast<int>(graph.nodes.size());

    auto local_com = std::Vector<PreparedCommodity> {};
    local_com.reserve(commodity_ids.size());
    for (const auto cid : commodity_ids) {
        local_com.push_back(commodities[cid]);
    }

    auto x_vars = std::Vector<ArcVar> {};
    auto x_by_k = std::Vector<std::Vector<int>>(static_cast<std::size_t>(K));
    x_vars.reserve(static_cast<std::size_t>(K * A / 8 + 1));
    for (int k = 0; k < K; ++k) {
        for (int a = 0; a < A; ++a) {
            if (!arc_usable_for_class(
                    graph.arcs[static_cast<std::size_t>(a)],
                    local_com[static_cast<std::size_t>(k)].cls,
                    local_com[static_cast<std::size_t>(k)].cob_unit,
                    graph.vp_node,
                    graph.vn_node)) {
                continue;
            }
            const auto var_id = static_cast<int>(x_vars.size());
            x_vars.push_back(ArcVar {k, a});
            x_by_k[static_cast<std::size_t>(k)].push_back(var_id);
        }
    }
    if (x_vars.empty()) {
        out.ok = false;
        out.message = std::format("{}: no feasible arc-variable pairs", stage_name);
        return out;
    }

    auto row_lo = std::vector<double> {};
    auto row_up = std::vector<double> {};
    auto add_eq = [&](const double rhs) -> int {
        const auto id = static_cast<int>(row_lo.size());
        row_lo.push_back(rhs);
        row_up.push_back(rhs);
        return id;
    };
    auto add_le = [&](const double rhs) -> int {
        const auto id = static_cast<int>(row_lo.size());
        row_lo.push_back(-kHighsInf);
        row_up.push_back(rhs);
        return id;
    };

    auto flow_row = std::map<std::pair<int, int>, int> {};
    const auto ensure_flow_row = [&](const int k, const int n) -> int {
        const auto key = std::make_pair(k, n);
        if (flow_row.contains(key)) {
            return flow_row.at(key);
        }
        const auto row = add_eq(0.0);
        flow_row[key] = row;
        return row;
    };

    auto edge_row = std::map<std::pair<int, int>, int> {};
    for (const auto& arc : graph.arcs) {
        if (arc.is_virtual) {
            continue;
        }
        auto u = arc.u;
        auto v = arc.v;
        if (u > v) {
            std::swap(u, v);
        }
        if (edge_row.contains({u, v})) {
            continue;
        }
        auto cap = 1;
        if (edge_capacity_override.contains({u, v})) {
            cap = edge_capacity_override.at({u, v});
        }
        edge_row[{u, v}] = add_le(static_cast<double>(cap));
    }

    auto x_entries = std::Vector<std::Vector<std::pair<int, double>>>(x_vars.size());
    auto incident_x = std::map<std::pair<int, int>, std::Vector<int>> {};
    for (std::size_t j = 0; j < x_vars.size(); ++j) {
        const auto k = x_vars[j].k;
        const auto a = x_vars[j].a;
        const auto& arc = graph.arcs[static_cast<std::size_t>(a)];
        x_entries[j].push_back({ensure_flow_row(k, arc.u), 1.0});
        x_entries[j].push_back({ensure_flow_row(k, arc.v), -1.0});
        if (!arc.is_virtual) {
            auto u = arc.u;
            auto v = arc.v;
            if (u > v) {
                std::swap(u, v);
            }
            x_entries[j].push_back({edge_row.at({u, v}), 1.0});
        }
        if (!graph.nodes[static_cast<std::size_t>(arc.u)].is_virtual) {
            incident_x[{k, arc.u}].push_back(static_cast<int>(j));
        }
        if (!graph.nodes[static_cast<std::size_t>(arc.v)].is_virtual) {
            incident_x[{k, arc.v}].push_back(static_cast<int>(j));
        }
    }

    for (int k = 0; k < K; ++k) {
        const auto s = local_com[static_cast<std::size_t>(k)].src;
        const auto t = local_com[static_cast<std::size_t>(k)].snk;
        const auto d = local_com[static_cast<std::size_t>(k)].demand;
        const auto rs = ensure_flow_row(k, s);
        const auto rt = ensure_flow_row(k, t);
        row_lo[static_cast<std::size_t>(rs)] = static_cast<double>(d);
        row_up[static_cast<std::size_t>(rs)] = static_cast<double>(d);
        row_lo[static_cast<std::size_t>(rt)] = static_cast<double>(-d);
        row_up[static_cast<std::size_t>(rt)] = static_cast<double>(-d);
    }

    auto o_entries = std::Vector<std::Vector<std::pair<int, double>>> {};
    auto o_vars = std::Vector<OVar> {};
    auto node_row = std::map<int, int> {};
    auto physical_nodes_in_use = std::set<int> {};
    for (const auto& [kn, vars] : incident_x) {
        (void)vars;
        physical_nodes_in_use.insert(kn.second);
    }
    for (const auto n : physical_nodes_in_use) {
        if (graph.nodes[static_cast<std::size_t>(n)].is_virtual) {
            continue;
        }
        auto cap = 1;
        if (node_capacity_override.contains(n)) {
            cap = node_capacity_override.at(n);
        }
        node_row[n] = add_le(static_cast<double>(cap));
    }
    for (const auto& [kn, vars] : incident_x) {
        const auto k = kn.first;
        const auto n = kn.second;
        if (vars.empty()) {
            continue;
        }
        const auto row_link = add_le(0.0);
        for (const auto j : vars) {
            x_entries[static_cast<std::size_t>(j)].push_back({row_link, 1.0});
        }
        o_vars.push_back(OVar {k, n});
        auto col = std::Vector<std::pair<int, double>> {};
        col.push_back({row_link, -2.0});
        col.push_back({node_row.at(n), 1.0});
        o_entries.push_back(std::move(col));
    }

    for (int k = 0; k < K; ++k) {
        const auto& c = local_com[static_cast<std::size_t>(k)];
        if (c.reach_steps.empty()) {
            continue;
        }
        const auto bbox = std::set<int>(c.bbox_cobs.begin(), c.bbox_cobs.end());
        for (const auto& step : c.reach_steps) {
            const auto tr_in = track_from_unit_inner(c.cob_unit, step.index_out);
            const auto tr_out = track_from_unit_inner(c.cob_unit, step.index_in);
            auto matched = std::Vector<int> {};
            for (const auto j : x_by_k[static_cast<std::size_t>(k)]) {
                const auto& arc = graph.arcs[static_cast<std::size_t>(x_vars[static_cast<std::size_t>(j)].a)];
                if (!arc.is_turn) {
                    continue;
                }
                if (arc.track_in != tr_in || arc.track_out != tr_out) {
                    continue;
                }
                if (!bbox.empty() && arc.cob >= 0 && !bbox.contains(arc.cob)) {
                    continue;
                }
                matched.push_back(j);
            }
            if (matched.empty()) {
                debug::warning_fmt(
                    "MCF {}: commodity {} reach step ({}->{}, {}->{}) has no matching arc in bbox",
                    stage_name, c.label, step.from_dir, step.to_dir, step.index_in, step.index_out);
                continue;
            }
            const auto row = add_eq(1.0);
            for (const auto j : matched) {
                x_entries[static_cast<std::size_t>(j)].push_back({row, 1.0});
            }
        }
    }

    if (enforce_bus_equal_length) {
        auto by_bus = std::map<std::String, std::Vector<int>> {};
        for (int k = 0; k < K; ++k) {
            if (!local_com[static_cast<std::size_t>(k)].is_bus) {
                continue;
            }
            by_bus[local_com[static_cast<std::size_t>(k)].bus_key].push_back(k);
        }
        for (const auto& [key, group] : by_bus) {
            (void)key;
            if (group.size() <= 1) {
                continue;
            }
            const auto ref = group.front();
            for (std::size_t gi = 1; gi < group.size(); ++gi) {
                const auto row = add_eq(0.0);
                const auto cur = group[gi];
                for (const auto j : x_by_k[static_cast<std::size_t>(cur)]) {
                    const auto& arc = graph.arcs[static_cast<std::size_t>(x_vars[static_cast<std::size_t>(j)].a)];
                    if (arc.is_virtual) {
                        continue;
                    }
                    x_entries[static_cast<std::size_t>(j)].push_back({row, 1.0});
                }
                for (const auto j : x_by_k[static_cast<std::size_t>(ref)]) {
                    const auto& arc = graph.arcs[static_cast<std::size_t>(x_vars[static_cast<std::size_t>(j)].a)];
                    if (arc.is_virtual) {
                        continue;
                    }
                    x_entries[static_cast<std::size_t>(j)].push_back({row, -1.0});
                }
            }
        }
    }

    const auto num_x = static_cast<int>(x_vars.size());
    const auto num_o = static_cast<int>(o_vars.size());
    const auto num_col = num_x + num_o;
    const auto num_row = static_cast<int>(row_lo.size());

    auto col_cost = std::vector<double>(static_cast<std::size_t>(num_col), 0.0);
    auto col_lo = std::vector<double>(static_cast<std::size_t>(num_col), 0.0);
    auto col_up = std::vector<double>(static_cast<std::size_t>(num_col), 1.0);
    auto a_start = std::vector<HighsInt>(static_cast<std::size_t>(num_col) + 1, 0);
    auto a_index = std::vector<HighsInt> {};
    auto a_value = std::vector<double> {};
    a_index.reserve(static_cast<std::size_t>(num_col * 8));
    a_value.reserve(static_cast<std::size_t>(num_col * 8));

    for (int j = 0; j < num_x; ++j) {
        a_start[static_cast<std::size_t>(j)] = static_cast<HighsInt>(a_index.size());
        const auto& arc = graph.arcs[static_cast<std::size_t>(x_vars[static_cast<std::size_t>(j)].a)];
        col_cost[static_cast<std::size_t>(j)] = arc.is_virtual ? 0.0 : 1.0;
        for (const auto& [r, v] : x_entries[static_cast<std::size_t>(j)]) {
            a_index.push_back(static_cast<HighsInt>(r));
            a_value.push_back(v);
        }
    }
    for (int j = 0; j < num_o; ++j) {
        const auto col = num_x + j;
        a_start[static_cast<std::size_t>(col)] = static_cast<HighsInt>(a_index.size());
        for (const auto& [r, v] : o_entries[static_cast<std::size_t>(j)]) {
            a_index.push_back(static_cast<HighsInt>(r));
            a_value.push_back(v);
        }
    }
    a_start[static_cast<std::size_t>(num_col)] = static_cast<HighsInt>(a_index.size());

    HighsLp lp {};
    lp.num_col_ = static_cast<HighsInt>(num_col);
    lp.num_row_ = static_cast<HighsInt>(num_row);
    lp.sense_ = ObjSense::kMinimize;
    lp.offset_ = 0.0;
    lp.col_cost_ = std::move(col_cost);
    lp.col_lower_ = std::move(col_lo);
    lp.col_upper_ = std::move(col_up);
    lp.row_lower_ = std::move(row_lo);
    lp.row_upper_ = std::move(row_up);
    lp.integrality_.assign(static_cast<std::size_t>(num_col), HighsVarType::kInteger);
    lp.model_name_ = std::string(stage_name);
    lp.a_matrix_.format_ = MatrixFormat::kColwise;
    lp.a_matrix_.num_col_ = lp.num_col_;
    lp.a_matrix_.num_row_ = lp.num_row_;
    lp.a_matrix_.start_ = std::move(a_start);
    lp.a_matrix_.index_ = std::move(a_index);
    lp.a_matrix_.value_ = std::move(a_value);
    lp.setMatrixDimensions();

    Highs highs {};
    highs.setOptionValue("output_flag", false);
    highs.setOptionValue("presolve", "on");
    if (highs.passModel(std::move(lp)) != HighsStatus::kOk) {
        out.ok = false;
        out.message = std::format("{}: passModel failed", stage_name);
        return out;
    }
    if (highs.run() != HighsStatus::kOk) {
        out.ok = false;
        out.message = std::format("{}: solver run failed", stage_name);
        out.model_status = static_cast<int>(highs.getModelStatus());
        return out;
    }
    const auto status = highs.getModelStatus();
    out.model_status = static_cast<int>(status);
    if (status != HighsModelStatus::kOptimal) {
        out.ok = false;
        out.message = std::format("{}: model not optimal ({})", stage_name, static_cast<int>(status));
        return out;
    }
    out.ok = true;
    out.message = "ok";
    out.objective = highs.getObjectiveValue();

    const auto sol = highs.getSolution();
    auto x_values = std::Vector<int>(x_vars.size(), 0);
    for (std::size_t j = 0; j < x_vars.size(); ++j) {
        x_values[j] = static_cast<int>(std::lround(sol.col_value[j]));
        if (x_values[j] <= 0) {
            continue;
        }
        const auto& arc = graph.arcs[static_cast<std::size_t>(x_vars[j].a)];
        if (!arc.is_virtual) {
            auto u = arc.u;
            auto v = arc.v;
            if (u > v) {
                std::swap(u, v);
            }
            out.used_edges[{u, v}] = 1;
        }
    }
    for (std::size_t j = 0; j < o_vars.size(); ++j) {
        const auto col = static_cast<std::size_t>(num_x + static_cast<int>(j));
        const auto val = static_cast<int>(std::lround(sol.col_value[col]));
        if (val > 0) {
            out.used_nodes[o_vars[j].node] = 1;
        }
    }

    auto edge_count_by_k = std::Vector<std::map<std::pair<int, int>, int>>(static_cast<std::size_t>(K));
    for (std::size_t j = 0; j < x_vars.size(); ++j) {
        const auto val = x_values[j];
        if (val <= 0) {
            continue;
        }
        const auto k = x_vars[j].k;
        const auto& arc = graph.arcs[static_cast<std::size_t>(x_vars[j].a)];
        edge_count_by_k[static_cast<std::size_t>(k)][{arc.u, arc.v}] += val;
    }

    out.paths.reserve(static_cast<std::size_t>(K));
    for (int k = 0; k < K; ++k) {
        const auto& c = local_com[static_cast<std::size_t>(k)];
        McfPathInfo info {};
        info.label = c.label;
        info.origin_name = c.origin_name;
        info.record_id = c.record_id;
        info.src = c.src;
        info.snk = c.snk;
        info.demand = c.demand;
        info.cob_unit = c.cob_unit;
        info.start_track = c.start_track;
        info.end_track = c.end_track;
        info.record_indices.push_back(c.record_id);

        auto path = extract_path(c.src, c.snk, edge_count_by_k[static_cast<std::size_t>(k)], N);
        if (!path.empty()) {
            info.unit_paths.push_back(path);
            auto track_path = std::Vector<std::size_t> {};
            for (const auto n : path) {
                if (graph.nodes[static_cast<std::size_t>(n)].is_virtual) {
                    continue;
                }
                const auto track = graph.nodes[static_cast<std::size_t>(n)].track;
                if (track_path.empty() || track_path.back() != track) {
                    track_path.push_back(track);
                }
            }
            if (!track_path.empty()) {
                info.track_paths.push_back(std::move(track_path));
            }
        }
        out.paths.push_back(std::move(info));
    }
    return out;
}

auto path_to_text(const GlobalGraph& graph, const std::Vector<int>& path) -> std::String {
    if (path.empty()) {
        return "(empty)";
    }
    auto s = std::String {};
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i != 0) {
            s += " -> ";
        }
        s += node_text(graph, path[i]);
    }
    return s;
}

} // namespace

auto run_mcf_global_routing_cob_units(
    const std::Vector<Net_cost_record>& records,
    const TobIlpResult& ilp_result,
    const hardware::Interposer& interposer,
    const circuit::BaseDie& basedie,
    const CobMcfGridDims cob_grid,
    const bool enable_mcf_parallel
) -> CobMcfFullResult {
    (void)interposer;
    (void)basedie;
    (void)enable_mcf_parallel;

    const auto mcf_start = std::chrono::steady_clock::now();
    const auto peak_before = get_peak_rss_mb();

    CobMcfFullResult out {};
    out.summary.per_cob.resize(16);
    if (records.size() != ilp_result.assignments.size() || records.size() != ilp_result.record_track_endpoints.size()) {
        for (std::size_t u = 0; u < 16; ++u) {
            out.summary.per_cob[u] = CobMcfCobUnitSummary {
                u,
                0,
                false,
                0.0,
                0,
                std::String("record/ilp result size mismatch")};
        }
        out.summary.all_ok = false;
        return out;
    }

    auto graph = build_track_graph(cob_grid);
    auto commodities = prepare_commodities(records, ilp_result, cob_grid, graph);
    auto bus_ids = std::Vector<std::size_t> {};
    auto simple_ids = std::Vector<std::size_t> {};
    auto bus_count_by_unit = std::array<int, 16> {};
    auto simple_count_by_unit = std::array<int, 16> {};
    bus_count_by_unit.fill(0);
    simple_count_by_unit.fill(0);
    for (std::size_t i = 0; i < commodities.size(); ++i) {
        if (commodities[i].is_bus) {
            bus_ids.push_back(i);
            bus_count_by_unit[commodities[i].cob_unit] += 1;
        }
        else {
            simple_ids.push_back(i);
            simple_count_by_unit[commodities[i].cob_unit] += 1;
        }
    }

    const auto solve_t0 = std::chrono::steady_clock::now();
    auto bus_res = solve_stage("BusMCF", graph, commodities, bus_ids, {}, {}, true);
    auto edge_cap_for_simple = std::map<std::pair<int, int>, int> {};
    for (const auto& [e, used] : bus_res.used_edges) {
        edge_cap_for_simple[e] = std::max(0, 1 - used);
    }
    auto node_cap_for_simple = std::map<int, int> {};
    for (const auto& [n, used] : bus_res.used_nodes) {
        node_cap_for_simple[n] = std::max(0, 1 - used);
    }
    auto simple_res = solve_stage("SimpleMCF", graph, commodities, simple_ids, edge_cap_for_simple, node_cap_for_simple, false);
    const auto solve_t1 = std::chrono::steady_clock::now();
    const auto solve_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(solve_t1 - solve_t0).count());

    if (!bus_res.ok) {
        debug::error_fmt("BusMCF failed: {}", bus_res.message);
    }
    if (!simple_res.ok) {
        debug::error_fmt("SimpleMCF failed: {}", simple_res.message);
    }
    out.summary.all_ok = bus_res.ok && simple_res.ok;

    for (std::size_t u = 0; u < 16; ++u) {
        const auto obj = bus_res.objective + simple_res.objective;
        out.summary.per_cob[u] = CobMcfCobUnitSummary {
            u,
            bus_count_by_unit[u] + simple_count_by_unit[u],
            out.summary.all_ok,
            obj,
            solve_ms,
            out.summary.all_ok ? std::String("ok") : std::String("stage failed")};
    }

    for (auto& p : bus_res.paths) {
        out.paths_by_unit[p.cob_unit].push_back(std::move(p));
    }
    for (auto& p : simple_res.paths) {
        out.paths_by_unit[p.cob_unit].push_back(std::move(p));
    }

    for (std::size_t u = 0; u < 16; ++u) {
        debug::info_fmt(
            "MCF unit {}: bus_com={} simple_com={} ok={}",
            u,
            bus_count_by_unit[u],
            simple_count_by_unit[u],
            out.summary.all_ok);
        for (const auto& info : out.paths_by_unit[u]) {
            debug::info_fmt(
                "  commodity {} rec={} start_track={} end_track={} path_count={}",
                info.label,
                info.record_id,
                info.start_track,
                info.end_track,
                info.unit_paths.size());
            for (std::size_t pi = 0; pi < info.unit_paths.size(); ++pi) {
                debug::info_fmt("    path#{} {}", pi, path_to_text(graph, info.unit_paths[pi]));
            }
        }
    }

    const auto mcf_end = std::chrono::steady_clock::now();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(mcf_end - mcf_start).count();
    const auto peak_after = get_peak_rss_mb();
    const auto stage_peak_delta = std::max(0.0, peak_after - peak_before);
    debug::info_fmt(
        "MCF detailed(track-level) summary: elapsed={} ms, peak_rss={:.2f} MB, stage_peak_delta={:.2f} MB",
        elapsed_ms,
        peak_after,
        stage_peak_delta);
    return out;
}

} // namespace PR_tool
