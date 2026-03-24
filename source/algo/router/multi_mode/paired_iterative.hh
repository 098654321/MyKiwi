#pragma once

#include "./occupancy_view.hh"
#include "./entry_exit.hh"
#include "./params.hh"
#include "./constrained_maze.hh"
#include <algo/router/single_mode/incremental/recorders/hardware_recorder.hh>
#include <circuit/net/net.hh>
#include <circuit/path/deferred_path.hh>
#include <std/utility.hh>

namespace kiwi::algo {

    struct NetRouteHistoryResult {
        // For regular nets this contains one element.
        // For SyncNet this contains one element per subnet.
        std::Vector<circuit::HistoryPathPackage> subnet_histories{};
        // Aggregate package used by recorder convergence and final commit.
        circuit::HistoryPathPackage aggregate_history{circuit::PathPackage{}};
    };

    struct PairedRouteResult {
        bool success{false};
        NetRouteHistoryResult net1_result{};
        NetRouteHistoryResult net2_result{};
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
        circuit::Net* net1, int mode1,
        circuit::Net* net2, int mode2,
        const circuit::Region& overlap_region,
        const MultiModeParams& params
    ) -> PairedRouteResult;

}

