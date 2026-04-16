#pragma once 

#include <std/integer.hh>

namespace PR_tool::hardware {

    enum class COBDirection {
        Left,
        Right,
        Down,
        Up
    };

}


namespace PR_tool::hardware {
    struct COBDirectionHash {
        std::usize operator()(const COBDirection& dir) const {
            return static_cast<std::usize>(dir);
        }
    };
}

