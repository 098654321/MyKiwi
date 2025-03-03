#pragma once

#include <circuit/net/nets.hh>
#include <std/collection.hh>
#include <std/memory.hh>
#include <hardware/bump/bump.hh>
#include <hardware/track/track.hh>
#include <functional>


namespace kiwi::circuit {
    class Net;
}

namespace kiwi::hardware {
    class Bump;
    class Track;
}


namespace kiwi::algo {
 
class RouteEngine {
public:
    RouteEngine(const std::Vector<std::Box<circuit::Net>>& nets);
    ~RouteEngine() = default;

    auto routed_nets() const -> std::Vector<circuit::Net*>;
    auto move_on() -> void { this->_posi += 1; }

public:
    auto nets() const -> const std::Vector<circuit::Net*>& { return this->_nets; }
    auto position() const -> std::usize {return this->_posi;}

private:
    std::Vector<circuit::Net*> _nets;
    std::usize _posi;   // point to the net to be routed
};

}

