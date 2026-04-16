#include "./allocate.hh"
#include <global/debug/debug.hh>


namespace PR_tool::algo {

auto Allocate::execute(hardware::Interposer* interposer, RouteEngine& engine) const -> void {
    debug::info("allocating tob resources for bumps ...");

    engine.allocatestrategy().allocate(interposer, engine.nets());
    
}

auto Allocate::to_string() const -> const std::String {
    return "Allocate";
}

}

