#pragma once

#include "std/string.hh"
#include "std/utility.hh"
#include "std/file.hh"
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
        int mode, std::optional<int> compare, bool try_all_modes, bool placement
    ) -> int;

    auto place(kiwi::hardware::Interposer*, kiwi::circuit::BaseDie*, std::vector<kiwi::circuit::TopDieInstance*>&) -> void;

    auto route(
        kiwi::hardware::Interposer*, kiwi::circuit::BaseDie*,
        std::StringView,  const std::FilePath&,
        int mode, std::optional<int> compare, bool try_all_modes
    ) -> void;
}