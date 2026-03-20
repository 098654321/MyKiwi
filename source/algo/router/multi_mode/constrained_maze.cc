#include "./constrained_maze.hh"

#include <algo/router/routeerror.hh>
#include <hardware/track/track.hh>
#include <hardware/cob/cobcoord.hh>

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
        if (tc.row == cob.row + 1 && tc.col == cob.col) {
            return true;
        }
        if (tc.col == cob.col + 1 && tc.row == cob.row) {
            return true;
        }
        return false;
    }

    template <class EndPredicate>
    static auto bfs_maze(
        const OccupancyView& view,
        int mode,
        const std::Vector<hardware::Track*>& begin_tracks,
        const std::HashSet<hardware::Track*>& occupied_tracks,
        EndPredicate&& is_end
    ) -> DeferredRegularPath {
        using hardware::Track;

        auto queue = std::Queue<Track*>{};
        auto prev =
            std::HashMap<Track*, std::Option<std::Tuple<Track*, hardware::COBConnector>>>{};

        for (auto* t : begin_tracks) {
            queue.push(t);
            prev.insert({t, std::nullopt});
        }

        while (!queue.empty()) {
            auto* cur = queue.front();
            queue.pop();

            if (is_end(cur)) {
                // build negative then flip to positive (same as existing maze)
                auto neg = std::Vector<std::Tuple<Track*, std::Option<hardware::COBConnector>>>{};
                auto* p = cur;
                while (true) {
                    auto it = prev.find(p);
                    if (it == prev.end()) {
                        throw FinalError("constrained_maze: prev missing");
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
                queue.push(next);
                prev.insert({next, std::Tuple<Track*, hardware::COBConnector>{cur, connector}});
            }
        }

        throw RetryExpt("constrained_maze: path not found");
    }

    auto maze_search_to_tracks(
        const OccupancyView& view,
        int mode,
        const std::Vector<hardware::Track*>& begin_tracks,
        const std::HashSet<hardware::Track*>& end_tracks,
        const std::HashSet<hardware::Track*>& occupied_tracks
    ) -> DeferredRegularPath {
        return bfs_maze(
            view, mode, begin_tracks, occupied_tracks,
            [&](hardware::Track* t) { return end_tracks.contains(t); }
        );
    }

    auto maze_search_to_cob(
        const OccupancyView& view,
        int mode,
        const std::Vector<hardware::Track*>& begin_tracks,
        const hardware::COBCoord& target_cob,
        const std::HashSet<hardware::Track*>& occupied_tracks
    ) -> DeferredRegularPath {
        return bfs_maze(
            view, mode, begin_tracks, occupied_tracks,
            [&](hardware::Track* t) {
                return reached_cob(t->coord(), target_cob) && view.is_idle_track(mode, t);
            }
        );
    }

}

