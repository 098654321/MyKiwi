#include "./init_recorder.hh"
#include <global/debug/debug.hh>


namespace kiwi::algo {

Init_recorder::Init_recorder(bool try_all_modes, bool path_exists_in_other_modes):
    _try_all_modes{try_all_modes},
    _path_exists_in_other_modes{path_exists_in_other_modes} {}


auto Init_recorder::execute(hardware::Interposer* interposer, RouteEngine& engine) const -> void {
    debug::info("Initializing path recorder ...");
    
    if (!_path_exists_in_other_modes) {
        debug::info("No path exists in other modes, skip initializing path recorder.");
        return;
    }

    auto nets = engine.nets_loaded_with_path();
    if (engine.try_all_modes()) {
        debug::info("Removing all existing path ...");
        for (auto& net: nets) {
            net->pathpackage().clear_all();
        }
    }
    else {
        // update cost
        debug::info("Updating cost ...");
        auto& recorder = engine.recorder();
        for (auto& net: nets) {
            recorder.update_recorders_current(net->pathpackage(), net->reuse_type().value());
        }
        for (auto& net: nets) {
            recorder.update_recorders_history(net->pathpackage(), net->reuse_type().value());
        }

        // set used resources
        debug::info("Setting used resources ...");
        for (auto& net: nets){
            if (net->modes().size() > 1) {
                net->pathpackage().occupy_all();
            }
            else {
                net->pathpackage().clear_all();
            }        
        }
    }

    debug::info("Existing Paths are initialized.");
}

auto Init_recorder::to_string() const -> const std::String {
    return "Init_recorder";
}

}
