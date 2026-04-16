#include "./coord.hh"

#include <std/integer.hh>

namespace PR_tool::hardware {

    Coord::Coord(std::i64 row, std::i64 col) :
        row{row}, col{col} 
    {
    }

    Coord::Coord() :
        Coord{0, 0}
    {
    }

    bool Coord::operator == (const Coord& other) const {
        return this->col == other.col && this->row == other.row;
    }

}

namespace std {

    std::size_t hash<PR_tool::hardware::Coord>::operator() (const PR_tool::hardware::Coord& c) const noexcept {
        auto h = hash<i64>{};
        return h(c.col) ^ h(c.row);
    }

}