#pragma once

#include <std/file.hh>
#include <std/string.hh>
#include <std/collection.hh>
#include <std/memory.hh>
#include <circuit/net/net.hh>
#include <algo/router/single_mode/incremental/recorders/hardware_recorder.hh>
#include <unordered_set>
#include "./params.hh"
#include "algo/router/multi_mode/occupancy_view.hh"

namespace kiwi::hardware {
    class Interposer;
}

namespace kiwi::circuit {
    class BaseDie;
    class Net;
}

namespace kiwi::algo {

    class MazeRouteStrategy;

    using NetsMapConstIter = std::HashMap<int, std::Vector<std::Rc<circuit::Net>>>::const_iterator;

    struct PairedNetGroup {
        circuit::Net* net1{nullptr};
        circuit::Net* net2{nullptr};
        circuit::Region overlap{};
        double priority_sum{0.0};
    };

    auto route_shared_nets(
        OccupancyView& view,
        HardwareRecorder& recorder,
        hardware::Interposer* interposer,
        const std::vector<std::shared_ptr<circuit::Net>>& shared
    ) -> void;

    auto route_mode_only_nets(
        OccupancyView& view,
        HardwareRecorder& recorder,
        hardware::Interposer* interposer,
        const NetsMapConstIter& it1,
        const NetsMapConstIter& it2,
        const MultiModeParams& params
    ) -> void;

    auto set_resources(
        const NetsMapConstIter& it1,
        const NetsMapConstIter& it2
    ) -> void;

    auto classify_nets(
        const NetsMapConstIter& mode1_it,
        const NetsMapConstIter& mode2_it
    ) -> std::Tuple<std::vector<std::shared_ptr<circuit::Net>>, std::vector<std::shared_ptr<circuit::Net>>, std::vector<std::shared_ptr<circuit::Net>>>;

    auto sort_shared_nets(
        const std::vector<std::shared_ptr<circuit::Net>>& shared_nets
    ) -> std::vector<std::shared_ptr<circuit::Net>>;

    auto route_shared(
        OccupancyView& view,
        HardwareRecorder& recorder,
        hardware::Interposer* interposer,
        const std::vector<std::shared_ptr<circuit::Net>>& shared_nets
    ) -> std::tuple<float, float>;

    auto compute_bounding_boxes(
        const NetsMapConstIter& mode1_it,
        const NetsMapConstIter& mode2_it
    ) -> std::tuple<std::Vector<circuit::BoundingBox>, std::Vector<circuit::BoundingBox>, std::Vector<circuit::Net*>, std::Vector<circuit::Net*>>;

    auto overlap(const circuit::Region& a, const circuit::Region& b) -> std::Option<circuit::Region>;

    auto compute_overlap_weights(
        const std::Vector<circuit::BoundingBox>& boxes1,
        const std::Vector<circuit::BoundingBox>& boxes2,
        const std::usize n
    ) -> std::Vector<std::Vector<std::i64>>;

    auto match_nets(
        std::Vector<circuit::BoundingBox>& boxes1,
        std::Vector<circuit::BoundingBox>& boxes2,
        const std::Vector<std::usize>& assign,
        const std::Vector<std::Vector<std::i64>>& weights
    ) -> std::tuple<std::Vector<PairedNetGroup>, std::Vector<circuit::Net*>, std::Vector<circuit::Net*>>;

    auto route_paired(
        hardware::Interposer* interposer,
        OccupancyView& view,
        HardwareRecorder& recorder,
        const std::Vector<PairedNetGroup>& pairs,
        const MultiModeParams& params
    ) -> std::tuple<std::usize, std::Vector<circuit::Net*>>;

    auto collect_remaining_nets(
        const std::Vector<circuit::Net*>& no_bbox,
        const std::Vector<circuit::Net*>& unpaired
    ) -> std::Vector<circuit::Net*>;

    auto route_remaining_nets(
        hardware::Interposer* interposer,
        OccupancyView& view,
        HardwareRecorder& recorder,
        int mode,
        const std::Vector<circuit::Net*>& remaining,
        const algo::MazeRouteStrategy& maze
    ) -> void;

}

