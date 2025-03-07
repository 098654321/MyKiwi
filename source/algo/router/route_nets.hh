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




    // debug
    auto show_retry_expt(circuit::Net*, RouteEngine&, hardware::Interposer*) -> void;

    auto net_connection_state(
        circuit::Net*, hardware::Interposer*
    ) -> std::Tuple<std::Vector<const kiwi::hardware::Bump *>, std::Vector<const kiwi::hardware::Bump *>, std::Vector<const kiwi::hardware::Track *>>;

    auto show_bump_resources(
        const std::Tuple<std::Vector<const kiwi::hardware::Bump *>, std::Vector<const kiwi::hardware::Bump *>, std::Vector<const kiwi::hardware::Track *>>&,
        circuit::Net*, hardware::Interposer*, RouteEngine&
    ) -> void;


    auto necessary_tracks(const hardware::Track*) -> std::HashSet<std::usize>;
    auto search_bumps_connected_with_track(const hardware::Bump*, RouteEngine&, hardware::Interposer*, const std::HashSet<std::usize>&) -> std::HashSet<std::usize>;
    auto unoccupied_vert_to_track_mux(const std::HashSet<std::usize>&, const hardware::TOB*) -> std::HashSet<std::usize>;
    auto unoccupied_hori_to_vert_mux(const std::HashSet<std::usize>&, const hardware::TOB*) -> std::HashSet<std::usize>;
    auto unoccupied_bump_to_hori_mux(const std::HashSet<std::usize>&, const hardware::TOB*) -> std::HashSet<std::usize>;
}