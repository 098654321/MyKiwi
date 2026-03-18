#pragma once

#include <std/file.hh>
#include <std/string.hh>

#include "./params.hh"

namespace kiwi::hardware {
    class Interposer;
}

namespace kiwi::circuit {
    class BaseDie;
}

namespace kiwi::algo {

    auto route_multi_mode(
        hardware::Interposer* interposer,
        circuit::BaseDie* basedie,
        std::StringView config_path,
        const std::FilePath& output_path,
        const kiwi::algo::multi_mode::MultiModeParams& params
    ) -> void;

}

