#pragma once 

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
        size_t operator()(const COBDirection& dir) const {
            return static_cast<size_t>(dir);
        }
    };
}

