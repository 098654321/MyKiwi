#include "ilp_maze_finalize.hh"

#include "ilp_maze_search.hh"
#include "mcf_hw_map.hh"

#include <debug/debug.hh>

#include <map>
#include <sstream>
#include <tuple>

namespace PR_tool {

namespace {

auto tob_coord_from_linear(std::size_t tob_linear) -> hardware::TOBCoord {
    const auto w = static_cast<std::size_t>(hardware::Interposer::TOB_ARRAY_WIDTH);
    return hardware::TOBCoord {
        static_cast<std::i64>(tob_linear / w),
        static_cast<std::i64>(tob_linear % w)};
}

auto bump_flat_index(const Bump_coord& b) -> std::size_t {
    return b.Bank * 64 + b.Group * 8 + b.Index;
}

auto detail_key(const std::String& net_name, const Bump_coord& bump) -> std::tuple<std::String, std::size_t, std::size_t, std::size_t, std::size_t> {
    return {net_name, bump.TOB, bump.Bank, bump.Group, bump.Index};
}

auto pick_track_for_bump(
    hardware::Interposer* interposer,
    const Bump_coord& bump,
    const TobIlpNetRouteDetail* detail,
    bool is_start) -> std::Option<std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*>> {
    if (detail == nullptr) {
        return std::nullopt;
    }

    const auto tob_coord = tob_coord_from_linear(bump.TOB);
    auto bump_opt = interposer->get_bump(tob_coord, bump_flat_index(bump));
    if (!bump_opt.has_value()) {
        return std::nullopt;
    }
    auto* bump_ptr = *bump_opt;
    auto track_map = is_start ? interposer->available_tracks_bump_to_track(bump_ptr)
                              : interposer->available_tracks_track_to_bump(bump_ptr);
    for (auto& [track, connector] : track_map) {
        if (track->coord().index != detail->track) {
            continue;
        }
        connector.give_out();
        return std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*> {bump_ptr, connector, track};
    }
    return std::nullopt;
}

auto path_text(const MazePath& path) -> std::String {
    auto ss = std::stringstream {};
    for (std::size_t i = 0; i < path.size(); ++i) {
        const auto* t = std::get<0>(path[i]);
        if (i != 0) {
            ss << " -> ";
        }
        ss << "(" << t->coord().row << "," << t->coord().col << ","
           << (t->coord().dir == hardware::TrackDirection::Horizontal ? "H" : "V")
           << ",idx=" << t->coord().index << ")";
    }
    return ss.str();
}

} // namespace

auto run_ilp_maze_finalize(
    const std::Vector<Net_cost_record>& records,
    const TobIlpResult& ilp_result,
    const CobMcfFullResult& mcf_result,
    hardware::Interposer* interposer,
    const circuit::BaseDie* basedie,
    const CobMcfGridDims cob_grid
) -> bool {
    // get route details for each net(name, tob, bank, group, index)
    auto details = std::map<std::tuple<std::String, std::size_t, std::size_t, std::size_t, std::size_t>, const TobIlpNetRouteDetail*> {};
    for (const auto& d : ilp_result.route_details) {
        details[detail_key(d.net_name, d.bump)] = &d;
    }

    // get records by id
    auto records_by_id = std::map<std::size_t, const Net_cost_record*> {};
    for (const auto& r : records) {
        records_by_id[r.record_id] = &r;
    }

    auto occupied_tracks = std::HashSet<hardware::Track*> {};
    bool all_ok = true;

    for (std::size_t cobu = 0; cobu < mcf_result.paths_by_unit.size(); ++cobu) {    
        for (const auto& info : mcf_result.paths_by_unit[cobu]) { // solve each cob unit
            for (std::size_t pi = 0; pi < info.unit_paths.size(); ++pi) {   // solve each path
                const auto& mcf_nodes = info.unit_paths[pi];
                const auto allow_cobs = collect_accessible_cobs(mcf_nodes, cob_grid);
                if (allow_cobs.empty()) {
                    debug::warning_fmt("maze: skip empty accessible map for commodity={} path#{}", info.label, pi);
                    all_ok = false;
                    continue;
                }

                std::size_t rec_idx = pi;
                if (!info.record_indices.empty()) {
                    rec_idx %= info.record_indices.size();
                    rec_idx = info.record_indices[rec_idx];
                }
                auto rec_it = records_by_id.find(rec_idx);
                if (rec_it == records_by_id.end()) {
                    debug::warning_fmt("maze: record id {} not found for commodity={} path#{}", rec_idx, info.label, pi);
                    all_ok = false;
                    continue;
                }
                const auto& rec = *rec_it->second;

                auto begin_tracks = std::Vector<hardware::Track*> {};
                auto end_tracks = std::HashSet<hardware::Track*> {};

// debug: 这里要看一下mcf是怎么存start track & end track的。这里存储方式会不会有错误，导致maze失败
                if (rec.mcf_has_start_track) {  // 如果有start track，则直接获取
                    auto t = interposer->get_track(rec.mcf_start_track);
                    if (t.has_value()) {
                        begin_tracks.push_back(*t);
                    }
                }
                else {                          // start bump -> track
                    if (rec.start_bumps.empty()) {
                        debug::warning_fmt("maze: no start bump in record {}", rec.net_name);
                        all_ok = false;
                        continue;
                    }
                    auto key = detail_key(rec.net_name, rec.start_bumps.front());
                    const auto* d = details.contains(key) ? details.at(key) : nullptr;
                    auto picked = pick_track_for_bump(interposer, rec.start_bumps.front(), d, true);
                    if (!picked.has_value()) {
                        debug::warning_fmt("maze: start TOB connector not found for net={} bit={}", rec.net_name, rec.bit_id);
                        all_ok = false;
                        continue;
                    }
                    begin_tracks.push_back(std::get<2>(*picked));
                }

                if (rec.type == Net_type::PNnet) {  // Pose/Nege net，获取可用的end track
                    const auto& ports = (rec.power_kind == IlpPowerKind::Pose) ? basedie->pose_ports() : basedie->nege_ports();
                    for (const auto& tc : ports) {
                        if (map_track(tc.index) != cobu) {
                            continue;
                        }
                        auto t = interposer->get_track(tc);
                        if (t.has_value()) {
                            end_tracks.insert(*t);
                        }
                    }
                }
                else if (rec.mcf_has_end_track) {   // 普通的以track为端点的net
                    auto t = interposer->get_track(rec.mcf_end_track);
                    if (t.has_value()) {
                        end_tracks.insert(*t);
                    }
                }
                else {                              // end bump -> end track
                    if (rec.end_bumps.empty()) {
                        debug::warning_fmt("maze: no end bump in record {}", rec.net_name);
                        all_ok = false;
                        continue;
                    }
                    auto key = detail_key(rec.net_name, rec.end_bumps.front());
                    const auto* d = details.contains(key) ? details.at(key) : nullptr;
                    auto end_info = pick_track_for_bump(interposer, rec.end_bumps.front(), d, false);
                    if (!end_info.has_value()) {
                        debug::warning_fmt("maze: end TOB connector not found for net={} bit={}", rec.net_name, rec.bit_id);
                        all_ok = false;
                        continue;
                    }
                    end_tracks.insert(std::get<2>(*end_info));
                }

                auto path = maze_search_accessible(interposer, begin_tracks, end_tracks, occupied_tracks, allow_cobs);
                if (path.empty()) {
                    debug::warning_fmt("maze failed: net={} bit={} commodity={} path#{}", rec.net_name, rec.bit_id, info.label, pi);
                    all_ok = false;
                    continue;
                }

                for (auto& [track, cobconnector] : path) {
                    occupied_tracks.insert(track);
                    if (cobconnector.has_value()) {
                        cobconnector.value().suspend();
                    }
                }

                debug::info_fmt(
                    "maze success: net={} bit={} cobunit={} commodity={} path#{} {}",
                    rec.net_name,
                    rec.bit_id,
                    cobu,
                    info.label,
                    pi,
                    path_text(path));
            }
        }
    }

    return all_ok;
}

} // namespace PR_tool
