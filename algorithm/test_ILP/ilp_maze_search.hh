#pragma once

#include "cob_mcf_router.hh"

#include <hardware/cob/cobcoord.hh>
#include <hardware/cob/cobconnector.hh>
#include <hardware/interposer.hh>
#include <hardware/track/track.hh>
#include <std/collection.hh>
#include <std/utility.hh>

namespace PR_tool {

using MazePath = std::Vector<std::Tuple<hardware::Track*, std::Option<hardware::COBConnector>>>;

auto collect_accessible_cobs(
    const std::Vector<int>& mcf_nodes,
    CobMcfGridDims cob_grid
) -> std::HashSet<hardware::COBCoord>;

auto track_in_accessible_map(
    const hardware::Track* track,
    const std::HashSet<hardware::COBCoord>& accessible_cobs
) -> bool;

auto maze_search_accessible(
    hardware::Interposer* interposer,
    const std::Vector<hardware::Track*>& begin_tracks,
    const std::HashSet<hardware::Track*>& end_tracks,
    const std::HashSet<hardware::Track*>& occupied_tracks,
    const std::HashSet<hardware::COBCoord>& accessible_cobs
) -> MazePath;

} // namespace PR_tool
