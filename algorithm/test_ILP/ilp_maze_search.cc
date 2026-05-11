#include "ilp_maze_search.hh"

#include "mcf_hw_map.hh"

#include <algorithm>
#include <stdexcept>

namespace PR_tool {

namespace {

auto check_found(const std::HashSet<hardware::Track*>& end_tracks, hardware::Track* track) -> bool {
    return end_tracks.find(track) != end_tracks.end();
}

} // namespace

auto collect_accessible_cobs(
    const std::Vector<int>& mcf_nodes,
    const CobMcfGridDims cob_grid
) -> std::HashSet<hardware::COBCoord> {
    auto out = std::HashSet<hardware::COBCoord> {};
    const int cob_count = cob_grid.rows * cob_grid.cols;
    for (const auto node : mcf_nodes) {
        if (node < 0 || node >= cob_count) {
            continue;
        }
        out.insert(mcf::linear_to_cob(static_cast<std::size_t>(node)));
    }
    return out;
}

auto track_in_accessible_map(
    const hardware::Track* track,
    const std::HashSet<hardware::COBCoord>& accessible_cobs
) -> bool {
    auto* mutable_track = const_cast<hardware::Track*>(track);
    for (const auto& [dir, cob] : mutable_track->adjacent_cob_coords()) {
        (void)dir;
        if (accessible_cobs.contains(cob)) {
            return true;
        }
    }
    return false;
}

auto maze_search_accessible(
    hardware::Interposer* interposer,
    const std::Vector<hardware::Track*>& begin_tracks,
    const std::HashSet<hardware::Track*>& end_tracks,
    const std::HashSet<hardware::Track*>& occupied_tracks,
    const std::HashSet<hardware::COBCoord>& accessible_cobs
) -> MazePath {
    auto queue = std::Queue<hardware::Track*> {};
    auto prev_track_infos = std::HashMap<hardware::Track*, std::Option<std::Tuple<hardware::Track*, hardware::COBConnector>>> {};

    for (auto* t : begin_tracks) {
        if (!track_in_accessible_map(t, accessible_cobs)) {
            continue;
        }
        queue.push(t);
        prev_track_infos.insert({t, std::nullopt});
    }

    while (!queue.empty()) {
        auto* track = queue.front();
        queue.pop();

        if (check_found(end_tracks, track)) {
            auto reverse_path = MazePath {};
            auto* cur_track = track;
            while (true) {
                auto prev_track_info = prev_track_infos.find(cur_track);
                if (prev_track_info == prev_track_infos.end()) {
                    throw std::runtime_error("maze_search_accessible: previous track missing");
                }
                if (!prev_track_info->second.has_value()) {
                    break;
                }
                reverse_path.emplace_back(cur_track, std::get<1>(*prev_track_info->second));
                cur_track = std::get<0>(*prev_track_info->second);
            }
            reverse_path.emplace_back(cur_track, std::nullopt);

            auto path = MazePath {};
            std::transform(reverse_path.rbegin(), reverse_path.rend(), std::back_inserter(path), [](const auto& p) {
                return p;
            });
            return path;
        }

        for (auto& [next_track, connector] : interposer->adjacent_idle_tracks(track)) {
            if (prev_track_infos.find(next_track) != prev_track_infos.end() || occupied_tracks.contains(next_track)) {
                continue;
            }
            if (!track_in_accessible_map(next_track, accessible_cobs)) {
                continue;
            }

            queue.push(next_track);
            prev_track_infos.insert({next_track, std::Tuple<hardware::Track*, hardware::COBConnector> {track, connector}});
        }
    }

    return {};
}

} // namespace PR_tool
