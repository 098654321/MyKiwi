#pragma once

#include "./occupancy_view.hh"
#include <circuit/path/pathpackage.hh>
#include <std/collection.hh>

namespace kiwi::algo {

    using DeferredRegularPath = std::Vector<std::Tuple<hardware::TrackCoord, std::Option<circuit::COBConnectorInfo>>>;

    // Maze search (BFS) on Track graph using OccupancyView, ending at any track in end_tracks.
    auto maze_search_to_tracks(
        const OccupancyView& view,
        int mode,
        const std::Vector<hardware::Track*>& begin_tracks,
        const std::HashSet<hardware::Track*>& end_tracks,
        const std::HashSet<hardware::Track*>& occupied_tracks
    ) -> DeferredRegularPath;

    // Maze search (BFS) ending when reaching a track adjacent to target_cob (per multi_mode.md rule).
    auto maze_search_to_cob(
        const OccupancyView& view,
        int mode,
        const std::Vector<hardware::Track*>& begin_tracks,
        const hardware::COBCoord& target_cob,
        const std::HashSet<hardware::Track*>& occupied_tracks
    ) -> DeferredRegularPath;

}

