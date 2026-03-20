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

    auto route_multi_mode(
        hardware::Interposer* interposer,
        circuit::BaseDie* basedie,
        std::StringView config_path,
        const std::FilePath& output_path,
        const kiwi::algo::MultiModeParams& params
    ) -> void;

    auto classify_nets(
        const auto&, const auto&
    ) -> std::Tuple<std::vector<std::shared_ptr<circuit::Net>>, std::vector<std::shared_ptr<circuit::Net>>, std::vector<std::shared_ptr<circuit::Net>>>;

    auto sort_shared_nets(
        const std::vector<std::shared_ptr<circuit::Net>>& shared_nets
    ) -> std::vector<std::shared_ptr<circuit::Net>>;

    auto route_shared_nets(
        OccupancyView& view,
        HardwareRecorder& recorder,
        hardware::Interposer* interposer,
        const std::vector<std::shared_ptr<circuit::Net>>& shared_nets
    ) -> std::tuple<float, float>;
}

