#pragma once

#include "./region.hh"
#include <std/integer.hh>

namespace kiwi::circuit {

    class Net;

    struct BoundingBox {
        Region region{};
        int mode{0};
        Net* net{nullptr};

        // reserved for matching stage
        Net* matched_net{nullptr};
        std::Option<Region> overlap_region{std::nullopt};
    };

}

