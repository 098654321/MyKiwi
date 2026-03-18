#pragma once

#include <std/integer.hh>

namespace kiwi::algo::multi_mode {

    struct MultiModeParams {
        std::usize k_candidates{4};
        double converge_threshold{1e-3};
    };

}

