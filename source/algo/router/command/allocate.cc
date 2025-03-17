#include "./allocate.hh"
#include <global/debug/debug.hh>


namespace kiwi::algo {

auto Allocate::execute(hardware::Interposer* interposer, RouteEngine& engine) const -> void {
    debug::debug("allocating tob resources for bumps ...");

    engine.allocatestrategy().allocate(interposer, engine.nets());
    
}

auto Allocate::to_string() const -> const std::String {
    return "Allocate";
}

}

