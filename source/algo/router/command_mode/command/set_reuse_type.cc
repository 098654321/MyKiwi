#include "./set_reuse_type.hh"
#include <global/debug/debug.hh>


namespace kiwi::algo {


auto Set_reuse_type::execute(hardware::Interposer* interposer, RouteEngine& engine) const -> void {
    debug::info("Setting reuse type ...");
    
    auto nets = engine.all_nets();
    for (auto& net: nets) {
        net->modes().size() > 1 ? net->set_reuse_type(true) : net->set_reuse_type(false);
    }
}

auto Set_reuse_type::to_string() const -> const std::String {
    return "Set_reuse_type";
}


}
