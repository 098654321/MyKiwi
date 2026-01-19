#pragma once

#include "std/string.hh"
#include "std/utility.hh"
#include <vector>


namespace hardware {
    class Interposer;
}


namespace circuit {
    class BaseDie;
    class TopDieInstance;
}


namespace kiwi {

    auto cli_main(
        std::StringView config_path, std::Option<std::StringView> output_path, 
        int mode, std::optional<int> compare, bool try_all_modes, bool placement
    ) -> int;

    auto place(hardware::Interposer*, circuit::BaseDie*, std::vector<circuit::TopDieInstance*>&) -> void;
    auto route(
        hardware::Interposer*, circuit::BaseDie*,
        std::StringView,  const std::FilePath&,
        int mode, std::optional<int> compare, bool try_all_modes
    ) -> void;
}