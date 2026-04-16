#pragma once

#include "../coord.hh"
#include "serde/de.hh"

#include <std/integer.hh>
#include <std/string.hh>
#include <std/format.hh>

namespace PR_tool::hardware {

    struct BumpCoord : public Coord {
        BumpCoord(const Coord& c, std::usize i);
        BumpCoord(std::i64 r, std::i64 c, std::usize i);
        BumpCoord();

                
        bool operator == (const BumpCoord& other) const;
        bool operator < (const BumpCoord& other) const;
        auto to_string() const -> std::String;

        std::usize index;
    };

}

FORMAT_STRUCT(PR_tool::hardware::BumpCoord, row, col, index)

DESERIALIZE_STRUCT(PR_tool::hardware::BumpCoord, 
    DE_FILED(row)
    DE_FILED(col)
    DE_FILED(index)
)

template <>
struct std::hash<PR_tool::hardware::BumpCoord> {
    std::size_t operator() (const PR_tool::hardware::BumpCoord& coord) const noexcept;
};