#include "./route.hh"
#include <global/debug/debug.hh>
#include "./clear.hh"


namespace kiwi::algo {

Route::Route() {
    this->_remediation.emplace_back(std::make_shared<Clear>());
}

auto Route::execute(hardware::Interposer* interposer, RouteEngine& engine) const -> void {
    debug::debug("routing ...");

    auto& nets = const_cast<std::Vector<circuit::Net*>&>(engine.nets());
    auto posi = engine.position();
    for (std::usize i = posi; i < nets.size(); ++i) {
        auto net = nets[i];

        // check existing path
        auto routed_nets = engine.routed_nets();
        net->search_related_nets(routed_nets);

        // route
        net->route(interposer, engine.routestrategy());
        engine.move_on();
    }
}

auto Route::to_string() const -> const std::String {
    return "Route";
}

}

