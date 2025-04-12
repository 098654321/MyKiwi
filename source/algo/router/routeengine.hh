#pragma once

#include <circuit/net/nets.hh>
#include "./common/routestrategy.hh"
#include "./common/allocatestrategy.hh"
#include "./incremental/maze/routing.hh"
#include "./incremental/recorders/hardware_recorder.hh"
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
        const std::HashMap<int, std::Vector<std::Rc<circuit::Net>>>& nets, const RouteStrategy& str, const AllocateStrategy& as, int m,
        bool incremental, bool path_exists, hardware::Interposer* interposer
    );
    ~RouteEngine() = default;

    auto routed_nets() const -> std::Vector<circuit::Net*>;
    auto move_on() -> void { this->_posi += 1; }

public:
    auto nets() const -> std::Vector<circuit::Net*>;
    auto all_nets() const -> std::Vector<circuit::Net*>;
    auto reusable_nets() const -> std::Set<circuit::Net*>;
    auto non_reusable_nets() const -> std::Set<circuit::Net*>;

    auto mode() const -> int {return this->_mode;}
    auto incremental() const -> bool {return this->_incremental;}   
    auto position() const -> std::usize {return this->_posi;}
    auto routestrategy() const -> const RouteStrategy& {return this->_routestrategy;}
    auto allocatestrategy() const -> const AllocateStrategy& {return this->_allocator;}
    auto incre_route_strategy() const -> const IncreRouting& {return this->_incre_strategy;}
    auto recorder() -> HardwareRecorder& {return this->_recorder;}

private:
    std::HashMap<int, std::Vector<circuit::Net*>> _nets;
    std::usize _posi;   // point to the net to be routed
    int _mode;

    const RouteStrategy& _routestrategy;
    const AllocateStrategy& _allocator;
    const IncreRouting _incre_strategy;
    HardwareRecorder _recorder;
    bool _incremental;
    bool _path_exists;
};

}

