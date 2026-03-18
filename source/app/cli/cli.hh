#pragma once

#include "std/string.hh"
#include "std/utility.hh"
#include "std/file.hh"
#include "std/integer.hh"
#include <vector>


namespace kiwi::hardware {
    class Interposer;
}


namespace kiwi::circuit {
    class BaseDie;
    class TopDieInstance;
}


namespace kiwi {

    auto cli_main(
        std::StringView config_path, std::Option<std::StringView> output_path,
        int mode, std::optional<int> compare, bool placement, bool multi_mode,
        std::Option<std::usize> mm_k_candidates,
        std::Option<double> mm_converge_threshold
    ) -> int;

    auto place(kiwi::hardware::Interposer*, kiwi::circuit::BaseDie*, std::vector<kiwi::circuit::TopDieInstance*>&) -> void;

    auto route_single_mode(
        kiwi::hardware::Interposer*, kiwi::circuit::BaseDie*,
        std::StringView,  const std::FilePath&,
        int mode, std::optional<int> compare
    ) -> void;
}