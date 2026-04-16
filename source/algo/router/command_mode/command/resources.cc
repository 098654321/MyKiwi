#include "./resources.hh"


namespace PR_tool::algo {

auto Resources::execute(hardware::Interposer* interposer, RouteEngine& engine) const -> void {
    auto nets = engine.nets();
    for (auto& net: nets){
        net->check_accessable_cobunit();
    }
    interposer->manage_cobunit_resources();
}

auto Resources::to_string() const -> const std::String {
    return "Resources";
}

}

