#pragma once

#include <debug/exception.hh>
#include <circuit/net/net.hh>
#include <std/algorithm.hh>
#include <std/memory.hh>
#include "./routeengine.hh"
#include <std/collection.hh>

namespace kiwi::hardware {
    class Interposer;
}

namespace kiwi::circuit {
    class BaseDie;
}

namespace kiwi::algo {

    class RouteStrategy;
    
    auto route_nets(
        hardware::Interposer* interposer,
        circuit::BaseDie* basedie,
        const RouteStrategy& strategy
    ) -> std::usize;

    auto analyze_results(
        hardware::Interposer* interposer,
        RouteEngine& engine,
        const RouteStrategy& strategy
    ) -> std::usize;

}