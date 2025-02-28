#include "./route_nets.hh"
#include "./invoker.hh"
#include "./routeengine.hh"
#include <algo/router/routeerror.hh>
#include "debug/debug.hh"
#include <circuit/basedie.hh>
#include <hardware/interposer.hh>

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
        
        // TODO: analyze_net(), reroute_command
        while (!invoker.check_command())
        try {
            invoker.invoke(interposer, engine, strateg);
        } 
        catch (const RetryExpt& err) {
            debug::info(err.what());
            bool call = invoker.call_remediation(invoker.current_command());
            if (!call) {
                debug::info("routing failed");
            }
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
            net->show();
            total_length += net->length();
        }

        debug::info_fmt("Total length of all nets: {}", total_length);
        return total_length;
    }

}