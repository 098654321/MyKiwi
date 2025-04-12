#include "./clear.hh"
#include <global/debug/debug.hh>


namespace kiwi::algo {

auto Clear::execute(hardware::Interposer* interposer, RouteEngine& engine) const -> void {
    debug::debug("Clearing existing paths ...");
    auto nets = engine.nets();
    for (auto& net : nets) {
        auto path_package = net->pathpackage();
        path_package.clear_all();
    }
}

auto Clear::to_string() const -> const std::String {
    return "Clear";
}

}

