#include "./incremental_route.hh"
#include <global/debug/debug.hh>
#include <algo/router/routeerror.hh>
#include <algorithm>


namespace kiwi::algo {

auto Incre_route::execute(hardware::Interposer* interposer, RouteEngine& engine) const -> void {
    debug::debug("routing in incremental mode ...");
    auto nets = engine.nets();              //* 现在做的是所有 net 
    auto& recorder = engine.recorder();

    auto compare = [] (circuit::Net* n1, circuit::Net* n2) -> bool {
        return n1->modes().size() > n2->modes().size();
    };
    std::sort(nets.begin(), nets.end(), compare);

    auto res = iterate_routing(interposer, engine, nets, recorder);
    if (!res) {
        reset(engine, nets);
        res = iterate_routing(interposer, engine, nets, recorder);
        if (!res) {
            throw FinalError("Incremental Routing failed");
        }
    }

    // succeed
    if (engine.position() < nets.size() - 1) {
        set_history_as_current(nets);    
    }
}

auto Incre_route::iterate_routing(hardware::Interposer* interposer, RouteEngine& engine, std::Vector<circuit::Net*>& nets, HardwareRecorder& recorder) const -> bool {
    std::usize cycle{0}, min_cycle{10};
    while(cycle < min_cycle) {
        debug::debug_fmt("cycle {}", cycle);

        for (auto net: nets) {
            net->pathpackage().reset_all();
        }
        engine.reset_position();

        for (auto net: nets) {
            net->modes().size() > 1 ? net->set_reuse_type(true) : net->set_reuse_type(false);
    
            // check existing path
            auto routed_nets = engine.routed_nets();
            net->search_related_nets(routed_nets);

            // route
            auto res = net->incremental_route(interposer, engine.incre_route_strategy(), engine, false);
            if (!res) {
                return cycle == 0 ? false : true;
            }
            engine.move_on();
            
            recorder.update_recorders(net->pathpackage(), net->reuse_type().value());
        }

        cycle++;
    }
    return true;
}

auto Incre_route::reset(RouteEngine& engine, std::Vector<circuit::Net*>& nets) const -> void {
    auto iter = std::remove_if(nets.begin(), nets.end(), [&](auto& net) {
        auto modes = net->modes();
        if (!modes.contains(engine.mode())) {
            return true;
        }
        return false;
    });
    nets.erase(iter, nets.end());
    for (auto n: nets) {
        n->clear_path();
    }
    engine.reset_position();
    engine.recorder().re_initialize();
}

auto Incre_route::set_history_as_current(std::Vector<circuit::Net*>& nets) const -> void {
    for (auto n: nets) {
        n->set_pathpackage(n->history_pathpackage());
        n->pathpackage().occupy_all();
    }
}

auto Incre_route::to_string() const -> const std::String {
    return "Incremental routing";
}

}

