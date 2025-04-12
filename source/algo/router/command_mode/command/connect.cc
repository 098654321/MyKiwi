#include "./connect.hh"
#include <global/debug/debug.hh>


namespace kiwi::algo {

auto Connect::execute(hardware::Interposer* interposer, RouteEngine& engine) const -> void {
    debug::debug("connecting paths ...");
    auto nets = engine.all_nets();
    for (auto& net : nets) {
        auto& path_package = net->pathpackage();    
        path_package.connect_all();
    }
}

auto Connect::to_string() const -> const std::String {
    return "Connect";
}

}

