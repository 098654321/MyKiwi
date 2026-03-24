#include "./constrained_maze.hh"
#include "hardware/track/trackcoord.hh"

#include <algo/router/routeerror.hh>
#include <hardware/track/track.hh>
#include <hardware/cob/cobcoord.hh>
#include <std/format.hh>

namespace kiwi::algo {

    static auto build_deferred(
        const std::Vector<std::Tuple<hardware::Track*, std::Option<hardware::COBConnector>>>& path
    ) -> DeferredRegularPath {
        auto out = DeferredRegularPath{};
        out.reserve(path.size());
        for (const auto& [t, cob] : path) {
            if (!cob.has_value()) {
                out.emplace_back(t->coord(), std::nullopt);
            } else {
                const auto& c = cob.value();
                out.emplace_back(
                    t->coord(),
                    std::make_optional(circuit::COBConnectorInfo{
                        c.coord(), c.from_dir(), c.from_track_index(), c.to_dir(), c.to_track_index()
                    })
                );
            }
        }
        return out;
    }

    static auto reached_cob(const hardware::TrackCoord& tc, const hardware::COBCoord& cob) -> bool {
        // multi_mode.md rule:
        // 1) same row/col with COB, OR
        // 2) row == cob.row+1 and col same, OR
        // 3) col == cob.col+1 and row same.
        if (tc.row == cob.row && tc.col == cob.col) {
            return true;
        }
        if (tc.row == cob.row + 1 && tc.col == cob.col && tc.dir == hardware::TrackDirection::Vertical) {
            return true;
        }
        if (tc.col == cob.col + 1 && tc.row == cob.row && tc.dir == hardware::TrackDirection::Horizontal) {
            return true;
        }
        return false;
    }

    template <class EndPredicate>
    static auto bfs_maze(
        const OccupancyView& view,
        HardwareRecorder& recorder,
        bool reuse_type,
        int mode,
        const std::Vector<hardware::Track*>& begin_tracks,
        const std::HashSet<hardware::Track*>& occupied_tracks,
        EndPredicate&& is_end
    ) -> DeferredRegularPath {
        using hardware::Track;

        if (begin_tracks.empty()) {
            throw std::runtime_error(std::format(
                "constrained_maze::bfs_maze(): begin_tracks is empty, mode={}, occupied_tracks_size={}",
                mode,
                occupied_tracks.size()
            ));
        }

        auto queue = std::MinHeap<std::Pair<Track*, float>, CompareTrack>{};
        auto prev = std::HashMap<Track*, std::Option<std::Tuple<Track*, hardware::COBConnector>>>{};    

        // init queue and prev map
        for (auto* t : begin_tracks) {
            queue.push(std::Pair<Track*, float>(t, 0));
            prev.insert({t, std::nullopt});
        }

        while (!queue.empty()) {
            auto [cur, current_cost] = queue.top();
            queue.pop();

            if (is_end(cur)) {
                // build negative then flip to positive (same as existing maze)
                auto neg = std::Vector<std::Tuple<Track*, std::Option<hardware::COBConnector>>>{};
                auto* p = cur;
                while (true) {
                    auto it = prev.find(p);
                    if (it == prev.end()) {
                        throw std::runtime_error(std::format(
                            "constrained_maze::bfs_maze(): prev missing for track=({}, {}, dir={}, index={}), mode={}, prev_size={}",
                            p->coord().row,
                            p->coord().col,
                            static_cast<int>(p->coord().dir),
                            p->coord().index,
                            mode,
                            prev.size()
                        ));
                    }
                    if (!it->second.has_value()) {
                        break;
                    }
                    neg.emplace_back(p, std::get<1>(*it->second));
                    p = std::get<0>(*it->second);
                }
                neg.emplace_back(p, std::nullopt);

                auto pos = std::Vector<std::Tuple<Track*, std::Option<hardware::COBConnector>>>{};
                pos.reserve(neg.size());
                std::transform(neg.rbegin(), neg.rend(), std::back_inserter(pos), [](const auto& x) { return x; });
                return build_deferred(pos);
            }

            for (auto& [next, connector] : view.adjacent_idle_tracks(mode, cur)) {
                if (prev.find(next) != prev.end() || occupied_tracks.contains(next)) {
                    continue;
                }

                float next_cost = 0;
                try {
                    next_cost = current_cost + recorder.expand_cost(cur, connector, reuse_type);
                }
                catch (const std::exception& err) {
                    throw std::runtime_error(std::format(
                        "constrained_maze::bfs_maze(): expand_cost failed at mode={}, cur_track=({}, {}, dir={}, index={}), reason={}",
                        mode,
                        cur->coord().row,
                        cur->coord().col,
                        static_cast<int>(cur->coord().dir),
                        cur->coord().index,
                        err.what()
                    ));
                }

                queue.push(std::Pair<hardware::Track*, float>(next, next_cost));
                prev.insert({next, std::Tuple<Track*, hardware::COBConnector>{cur, connector}});
            }
        }

        throw std::runtime_error(std::format(
            "constrained_maze::bfs_maze(): path not found, mode={}, begin_tracks={}, occupied_tracks={}, explored_tracks={}",
            mode,
            begin_tracks.size(),
            occupied_tracks.size(),
            prev.size()
        ));
    }

    auto maze_search_to_tracks(
        const OccupancyView& view,
        HardwareRecorder& recorder,
        bool reuse_type,
        int mode,
        const std::Vector<hardware::Track*>& begin_tracks,
        const std::HashSet<hardware::Track*>& end_tracks,
        const std::HashSet<hardware::Track*>& occupied_tracks
    ) -> DeferredRegularPath {
        try {
            return bfs_maze(
                view, recorder, reuse_type, mode, begin_tracks, occupied_tracks,
                [&](hardware::Track* t) { return end_tracks.contains(t); }
            );
        }
        catch (const std::exception& err) {
            throw std::runtime_error(std::format(
                "maze_search_to_tracks() failed: mode={}, begin_tracks={}, end_tracks={}, occupied_tracks={}, reason={}",
                mode,
                begin_tracks.size(),
                end_tracks.size(),
                occupied_tracks.size(),
                err.what()
            ));
        }
    }

    auto maze_search_to_cob(
        const OccupancyView& view,
        HardwareRecorder& recorder,
        bool reuse_type,
        int mode,
        const std::Vector<hardware::Track*>& begin_tracks,
        const hardware::COBCoord& target_cob,
        const std::HashSet<hardware::Track*>& occupied_tracks
    ) -> DeferredRegularPath {
        try {
            return bfs_maze(
                view, recorder, reuse_type, mode, begin_tracks, occupied_tracks,
                [&](hardware::Track* t) {
                    return reached_cob(t->coord(), target_cob) && view.is_idle_track(mode, t);
                }
            );
        }
        catch (const std::exception& err) {
            throw std::runtime_error(std::format(
                "maze_search_to_cob() failed: mode={}, begin_tracks={}, target_cob=({}, {}), occupied_tracks={}, reason={}",
                mode,
                begin_tracks.size(),
                target_cob.row,
                target_cob.col,
                occupied_tracks.size(),
                err.what()
            ));
        }
    }

}

