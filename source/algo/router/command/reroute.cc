#include "./reroute.hh"
#include <global/debug/debug.hh>


namespace kiwi::algo {

auto Reroute::execute(hardware::Interposer* interposer, RouteEngine& engine, const RouteStrategy& strategy) const -> void {
    debug::debug("Rerouting ...");
    debug::unimplement("Rerouting module is not implemented yet");
}

auto Reroute::to_string() const -> const std::String {
    return "Reroute";
}

}

