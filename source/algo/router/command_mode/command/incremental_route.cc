#include "./incremental_route.hh"
#include <global/debug/debug.hh>
#include <algo/router/routeerror.hh>


namespace kiwi::algo {

auto Incre_route::execute(hardware::Interposer* interposer, RouteEngine& engine) const -> void {
    debug::debug("routing in incremental mode ...");
    auto nets = engine.nets();              //* 现在做的是所有 net 
    auto posi = engine.position();
    auto& recorder = engine.recorder();
    std::usize min_cycle = 10;
    std::usize max_cycle = 20;

    auto compare = [] (circuit::Net* n1, circuit::Net* n2) -> bool {
        return n1->modes().size() > n2->modes().size();
    };
    std::sort(nets.begin(), nets.end(), compare);

    std::usize cycle = 0;
    while(cycle < min_cycle || recorder.check_shared()) {
        debug::debug_fmt("cycle {}", cycle);

        for (std::usize i = posi; i < nets.size(); ++i) {
            auto net = nets[i];
            net->pathpackage().reset_all();
        }
        engine.reset_position();

        for (std::usize i = posi; i < nets.size(); ++i) {
            auto net = nets[i];
            net->modes().size() > 1 ? net->set_reuse_type(true) : net->set_reuse_type(false);
    
            // check existing path
            auto routed_nets = engine.routed_nets();
            net->search_related_nets(routed_nets);

            // route
            // TODO：所有的 begin_tracks 一起开始搜索和一个一个搜索有什么区别。以及末端如果用 end_track_set 存储那么也没有考虑到 mux 的 cost
            auto res = net->incremental_route(interposer, engine.incre_route_strategy(), engine, false);
            if (!res) {
                // allow sharing for tob mux
                //* cob 没有允许共享
                net->pathpackage().reset_all();
                res = net->incremental_route(interposer, engine.incre_route_strategy(), engine, true);
                if (!res) {
                    throw std::logic_error("incremental routing failed when allowing shared");
                }
            }
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

