#pragma once

#include <circuit/net/nets.hh>
#include "./routestrategy.hh"
#include "./allocatestrategy.hh"
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
    RouteEngine(
        const std::HashMap<int, std::Vector<std::Rc<circuit::Net>>>& nets, const RouteStrategy& str, const AllocateStrategy& as, int m
    );
    ~RouteEngine() = default;

    auto routed_nets() const -> std::Vector<circuit::Net*>;
    auto move_on() -> void { this->_posi += 1; }

public:
    auto nets() const -> const std::Vector<circuit::Net*>& { return this->_nets.at(this->_mode);}
    auto nets() -> std::Vector<circuit::Net*>& {return this->_nets.at(this->_mode);}

    auto position() const -> std::usize {return this->_posi;}
    auto routestrategy() const -> const RouteStrategy& {return this->_routestrategy;}
    auto allocatestrategy() const -> const AllocateStrategy& {return this->_allocator;}

private:
    std::HashMap<int, std::Vector<circuit::Net*>> _nets;
    std::usize _posi;   // point to the net to be routed
    int _mode;

    const RouteStrategy& _routestrategy;
    const AllocateStrategy& _allocator;
};

}

