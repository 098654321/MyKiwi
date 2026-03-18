#pragma once

#include <std/integer.hh>

namespace kiwi::circuit {

    // Closed interval region in interposer coordinate system.
    struct Region {
        std::i64 row_min{0};
        std::i64 row_max{0};
        std::i64 col_min{0};
        std::i64 col_max{0};

        auto normalize() -> void {
            if (row_min > row_max) {
                auto tmp = row_min;
                row_min = row_max;
                row_max = tmp;
            }
            if (col_min > col_max) {
                auto tmp = col_min;
                col_min = col_max;
                col_max = tmp;
            }
        }
    };

}

