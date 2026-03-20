#pragma once

#include <std/collection.hh>
#include <std/integer.hh>

namespace kiwi::algo {

    // Max-weight assignment for a square matrix.
    // weights[i][j] are non-negative integers.
    // Returns match vector `assign`, where assign[i] = matched column index for row i.
    auto hungarian_max_weight(const std::Vector<std::Vector<std::i64>>& weights) -> std::Vector<std::usize>;

}

