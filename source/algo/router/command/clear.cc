#include "./clear.hh"
#include <global/debug/debug.hh>


namespace kiwi::algo {

auto Clear::execute(hardware::Interposer* interposer, RouteEngine& engine, const RouteStrategy& strategy) const -> void {
    debug::debug("Clearing existing paths ...");
    auto& nets = const_cast<std::Vector<circuit::Net*>&>(engine.nets());
    for (auto& net : nets) {
        auto path_package = net->pathpackage();
        path_package._regular_path.clear();
        path_package._tob_to_track.clear();
        path_package._track_to_tob.clear();
    }
}

auto Clear::to_string() const -> const std::String {
    return "Clear";
}

}

