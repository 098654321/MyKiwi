#pragma once

#include <std/file.hh>
#include <std/string.hh>
#include <std/collection.hh>
#include <std/memory.hh>
#include <circuit/net/net.hh>
#include <unordered_set>

#include "./params.hh"

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
        const kiwi::algo::multi_mode::MultiModeParams& params
    ) -> void;

    auto classify_nets(
        const auto&, const auto&
    ) -> std::Tuple<std::unordered_set<std::shared_ptr<circuit::Net>>, std::unordered_set<std::shared_ptr<circuit::Net>>, std::unordered_set<std::shared_ptr<circuit::Net>>>;
}

