
#pragma once

#include "../coord.hh"
#include "serde/de.hh"

#include <std/integer.hh>
#include <std/string.hh>
#include <std/format.hh>

namespace PR_tool::hardware {

    enum class TrackDirection {
        Horizontal,
        Vertical,
    };

    struct TrackCoord : public Coord {
        TrackCoord(std::i64 r, std::i64 c, TrackDirection d, std::usize i);
        TrackCoord();
                
        bool operator == (const TrackCoord& other) const;
        bool operator < (const TrackCoord& other) const;
        auto to_string() const -> std::String;

        TrackDirection dir;
        std::usize index;
    };

}

FORMAT_ENUM(PR_tool::hardware::TrackDirection,
    PR_tool::hardware::TrackDirection::Horizontal,
    PR_tool::hardware::TrackDirection::Vertical
)

FORMAT_STRUCT(PR_tool::hardware::TrackCoord, row, col, dir, index)

DESERIALIZE_ENUM(PR_tool::hardware::TrackDirection,
    DE_VALUE_AS(PR_tool::hardware::TrackDirection::Horizontal, "hori")
    DE_VALUE_AS(PR_tool::hardware::TrackDirection::Vertical, "vert")
)

DESERIALIZE_STRUCT(PR_tool::hardware::TrackCoord, 
    DE_FILED(row)
    DE_FILED(col)
    DE_FILED(dir)
    DE_FILED(index)
)

template <>
struct std::hash<PR_tool::hardware::TrackDirection> {
    std::size_t operator() (const PR_tool::hardware::TrackDirection& dir) const noexcept;
};

template <>
struct std::hash<PR_tool::hardware::TrackCoord> {
    std::size_t operator() (const PR_tool::hardware::TrackCoord& coord) const noexcept;
};
