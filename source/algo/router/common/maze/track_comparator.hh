#pragma once

#include <std/utility.hh>
#include <hardware/track/track.hh>
#include <std/collection.hh>



namespace kiwi::algo {

    struct CompareTrack {
        bool operator()(std::Pair<hardware::Track*, float> e1, std::Pair<hardware::Track*, float> e2) const {
            return e1.second > e2.second;
        }
    };

}
