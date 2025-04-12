#include "./incremental_route.hh"
#include <global/debug/debug.hh>


namespace kiwi::algo {

auto Incre_route::execute(hardware::Interposer* interposer, RouteEngine& engine) const -> void {
    debug::debug("routing in incremental mode ...");
    auto nets = engine.nets();              //TODO: 现在做的是所有 net 
    auto posi = engine.position();

    for (std::usize i = posi; i < nets.size(); ++i) {
        auto net = nets[i];
        net->modes().size() > 1 ? net->set_reuse_type(true) : net->set_reuse_type(false);

        // check existing path
        auto routed_nets = engine.routed_nets();
        net->search_related_nets(routed_nets);

        // route
        // TODO： 所有的 begin_tracks 一起开始搜索和一个一个搜索有什么区别
        net->incremental_route(interposer, engine.incre_route_strategy(), engine);
        engine.move_on();
    }
}

auto Incre_route::to_string() const -> const std::String {
    return "Incremental routing";
}

}

