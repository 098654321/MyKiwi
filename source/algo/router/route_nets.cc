#include "./route_nets.hh"
#include "./invoker.hh"
#include "./routeengine.hh"
#include <algo/router/routeerror.hh>
#include "debug/debug.hh"
#include <circuit/basedie.hh>
#include <hardware/interposer.hh>
#include <algorithm>


namespace kiwi::algo {

    auto route_nets(
        hardware::Interposer* interposer,
        circuit::BaseDie* basedie,
        const RouteStrategy& strateg
    ) -> std::usize {
        debug::info("Route nets");
        auto invoker = Invoker{};
        auto engine = RouteEngine{basedie->nets()};
        invoker.set_route_commands();
        
        // TODO: reroute_command
        while (!invoker.check_command())
        try {
            invoker.invoke(interposer, engine, strateg);
        } 
        catch (const RetryExpt& err) {
            assert (err.net() != nullptr);

            debug::info(err.what());
            show_retry_expt(err.net(), engine, interposer);

            // bool call = invoker.call_remediation(invoker.current_command());
            // if (!call) {
            //     debug::info("routing failed");
            // }
        }
        catch (const FinalError& err){
            debug::exception_in("route_nets()", err.what());
        }
        catch (const std::exception& err){
            throw std::runtime_error("route_nets() >> " + std::String(err.what()));
        }

        auto total_length = analyze_results(interposer, engine, strateg);
        return total_length;
    }

    // return total length of all nets
    auto analyze_results(
        hardware::Interposer* interposer,
        RouteEngine& engine,
        const RouteStrategy& strategy
    ) -> std::usize {
        std::usize total_length {0};
        const auto& nets = engine.nets();
        for (const auto& net: nets) {
            debug::debug(net->to_string());
            net->show_path();
            total_length += net->length();
        }

        debug::info_fmt("Total length of all nets: {}", total_length);
        return total_length;
    }

    auto show_retry_expt(circuit::Net* net, RouteEngine& engine, hardware::Interposer* interposer) -> void {
        //! cobconnector 没有把真正的硬件状态设为 giveout
        debug::debug("\n");
        debug::debug("Show details of retry exception");
        debug::debug(net->to_string());
        
        auto [routable_bumps, unroutable_bumps, unroutable_tracks] = net->connection_state();

        debug::debug("routable bumps of this net: ");
        if (!routable_bumps.empty()) {
            for (auto& bump: routable_bumps) {
                debug::debug(bump->coord().to_string());

                auto [do_not_care, tobconnector, connected_track] = net->pathpackage().find_bump(bump).value();
                debug::debug_fmt("connected with track {}", connected_track->coord().to_string());

                debug::debug("More available tracks:");
                auto available_tracks = interposer->available_tracks(
                    const_cast<hardware::Bump*>(bump), tobconnector.single_direction()
                );
                if (!available_tracks.empty()) {
                    for (auto& [track, _]: available_tracks) {
                        debug::debug(track->coord().to_string());
                    }
                }
                else {
                    debug::debug("Empty");
                }
            }
            debug::debug("\n");
        }
        else {
            debug::debug("Empty\n");
        }

        debug::debug("unroutable bumps of this net: ");
        if (!unroutable_bumps.empty()) {
            for (auto& bump: unroutable_bumps) {
                debug::debug(bump->coord().to_string());

                debug::debug("More available tracks for TOB_to_track_direction:");
                auto available_tracks_bump_to_track = interposer->available_tracks_bump_to_track(
                    const_cast<hardware::Bump*>(bump)
                );
                if (!available_tracks_bump_to_track.empty()) {
                    for (auto& [track, _]: available_tracks_bump_to_track) {
                        debug::debug(track->coord().to_string());
                    }
                }
                else {
                    debug::debug("Empty");
                }

                debug::debug("More available tracks for track_to_TOB_direction:");
                auto available_tracks_track_to_bump = interposer->available_tracks_track_to_bump(
                    const_cast<hardware::Bump*>(bump)
                );
                if (!available_tracks_track_to_bump.empty()) {
                    for (auto& [track, _]: available_tracks_track_to_bump) {
                        debug::debug(track->coord().to_string());
                    }
                }
                else {
                    debug::debug("Empty");
                }
            }
            debug::debug("\n");
        }
        else {
            debug::debug("Empty\n");
        }

        debug::debug("unroutable tracks of this net");
        for (auto& track: unroutable_tracks) {
            debug::debug(track->coord().to_string());
        };

        debug::debug("\n");
    }

}