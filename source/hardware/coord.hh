#pragma once 

#include <std/integer.hh>
#include <std/format.hh>
#include <serde/de.hh>

namespace PR_tool::hardware {

    struct Coord {
        Coord(std::i64 row, std::i64 col);
        Coord();
        
        bool operator == (const Coord& other) const;

        std::i64 row;
        std::i64 col;
    };

}

DESERIALIZE_STRUCT(PR_tool::hardware::Coord,
    DE_FILED(row)
    DE_FILED(col)
)

FORMAT_STRUCT(PR_tool::hardware::Coord, row, col)

template<>
struct std::hash<PR_tool::hardware::Coord> {
    std::size_t operator() (const PR_tool::hardware::Coord& c) const noexcept;
};
