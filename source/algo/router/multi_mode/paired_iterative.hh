#pragma once

#include "./occupancy_view.hh"
#include "./entry_exit.hh"
#include "./params.hh"
#include "./constrained_maze.hh"
#include <algo/router/single_mode/incremental/recorders/hardware_recorder.hh>
#include <circuit/net/net.hh>
#include <circuit/path/deferred_path.hh>
#include <std/utility.hh>

namespace kiwi::algo::multi_mode {

    struct PairedRouteResult {
        bool success{false};
        circuit::HistoryPathPackage net1_history{circuit::PathPackage{}};
        circuit::HistoryPathPackage net2_history{circuit::PathPackage{}};
        CobPairCandidate used_cob_pair{};
    };

    // Minimal paired routing scaffold:
    // - tries k candidates in parallel
    // - each candidate iterates routing the two nets until convergence
    // - no global hardware mutation; returns HistoryPathPackage for later commit
    auto route_paired_nets_iterative(
        hardware::Interposer* interposer,
        const OccupancyView& base_view,
        const algo::HardwareRecorder& base_recorder,
        const circuit::Net& net1, int mode1,
        const circuit::Net& net2, int mode2,
        const circuit::Region& overlap_region,
        const MultiModeParams& params
    ) -> PairedRouteResult;

}

