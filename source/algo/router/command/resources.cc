#include "./resources.hh"


namespace kiwi::algo {

auto Resources::execute(hardware::Interposer* interposer, RouteEngine& engine, const RouteStrategy& strategy) const -> void {
    auto& nets = const_cast<std::Vector<circuit::Net*>&>(engine.nets());
    for (auto& net: nets){
        net->check_accessable_cobunit();
    }
    interposer->manage_cobunit_resources();
}

auto Resources::to_string() const -> const std::String {
    return "Resources";
}

}

