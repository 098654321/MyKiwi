#include "./connect.hh"
#include <global/debug/debug.hh>


namespace kiwi::algo {

auto Connect::execute(hardware::Interposer* interposer, RouteEngine& engine) const -> void {
    debug::info("Connecting paths ...");
    auto nets = engine.all_nets();
    for (auto& net : nets) {
        if (net->modes().contains(engine.mode())) {
            debug::debug_fmt("{} is connecting paths ...", net->name());

            auto& path_package = net->pathpackage();    
            path_package.connect_all();
        }
        else {
            net->pathpackage().reset_all();
        }
    }
}

auto Connect::to_string() const -> const std::String {
    return "Connect";
}

}

