#pragma once 

#include <std/integer.hh>

namespace kiwi::hardware {

    enum class COBDirection {
        Left,
        Right,
        Down,
        Up
    };

}


namespace kiwi::hardware {
    struct COBDirectionHash {
        std::usize operator()(const COBDirection& dir) const {
            return static_cast<std::usize>(dir);
        }
    };
}

