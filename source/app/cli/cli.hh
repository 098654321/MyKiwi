#pragma once

#include "std/string.hh"
#include "std/utility.hh"

namespace kiwi {

    auto cli_main(std::StringView config_path, std::Option<std::StringView> output_path, int mode, std::optional<int> compare, bool try_all_modes) -> int;

}