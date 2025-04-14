#include "./incremental_route.hh"
#include <global/debug/debug.hh>
#include <algo/router/routeerror.hh>


namespace kiwi::algo {

auto Incre_route::execute(hardware::Interposer* interposer, RouteEngine& engine) const -> void {
    debug::debug("routing in incremental mode ...");
    auto nets = engine.nets();              //TODO: 现在做的是所有 net 
    auto posi = engine.position();
    auto& recorder = engine.recorder();
    std::usize min_cycle = 10;
    std::usize max_cycle = 20;

    std::usize cycle = 0;
    while(cycle < min_cycle || recorder.check_shared()) {
        for (std::usize i = posi; i < nets.size(); ++i) {
            auto net = nets[i];
            net->modes().size() > 1 ? net->set_reuse_type(true) : net->set_reuse_type(false);
    
            // check existing path
            auto routed_nets = engine.routed_nets();
            net->search_related_nets(routed_nets);
    
            // route
            // TODO：所有的 begin_tracks 一起开始搜索和一个一个搜索有什么区别
            net->incremental_route(interposer, engine.incre_route_strategy(), engine);
            // TODO: 如果布线失败，设置允许共享，然后重新布线。 mux 的硬件状态需要增加一个 Shared
            engine.move_on();

            recorder.clear_shared(net->history_pathpackage());
            recorder.update_recorders(net->pathpackage(), net->reuse_type().value());
        }

        cycle++;
        if (cycle >= max_cycle) {
            throw FinalError("incremental routing failed");
            break;
        }
    }
}

auto Incre_route::to_string() const -> const std::String {
    return "Incremental routing";
}

}

