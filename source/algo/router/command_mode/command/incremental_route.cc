#include "./incremental_route.hh"
#include <global/debug/debug.hh>
#include <algo/router/routeerror.hh>
#include <algorithm>


namespace kiwi::algo {

void check_tobconnector_consistency(std::Vector<circuit::Net*>& nets);
void check_address(std::Vector<circuit::Net*> nets);

auto Incre_route::execute(hardware::Interposer* interposer, RouteEngine& engine) const -> void {
    debug::info("routing in incremental mode ...");
    auto nets = engine.nets();              
    auto& recorder = engine.recorder();
    recorder.set_use_cost(true);

    nets = sort_incre(nets);

    auto res = iterate_routing(interposer, engine, nets, recorder);
    if (!res) {                 // fail in the first iteration
        if (!engine.path_exists_in_other_modes()) {
            engine.collect_data_when_fail(nets, true);
            throw FinalError("Incremental Routing failed");
        }
        else {
            debug::info("incremental routing failed when path exists in other modes, reinitialize and reroute");
            for (auto& net: nets) {
                net->clear_current_package();
            }
            engine.set_path_exists(false);
            nets = engine.nets();
            nets = sort_incre(nets);
            auto res = iterate_routing(interposer, engine, nets, recorder);
            if (!res) {
                engine.collect_data_when_fail(nets, true);
                throw FinalError("Incremental Routing failed");
            }
        }
    }

    // succeed: fail in the iteration > 2, but has a path in the former iteration
    if (engine.position() < nets.size() - 1) {
        debug::info("set result in last iteration as final result");
        set_history_as_current(nets, interposer);
    }
}


auto Incre_route::sort_incre(std::Vector<circuit::Net*>& nets) const -> std::Vector<circuit::Net*> {
    auto compare = [] (circuit::Net* n1, circuit::Net* n2) -> bool {
            return n1->modes().size() > n2->modes().size();
    };
    std::sort(nets.begin(), nets.end(), compare);
    
    auto i {0};
    for (auto& n: nets) {
        debug::info_fmt("net {}: {}", i++, n->name());
    }

    return nets;
}


// for debug
void check_tobconnector_consistency(std::Vector<circuit::Net*>& nets) {
    std::String except_mess{};

    for (auto& net: nets) {
        const auto& package = net->pathpackage();
    try {
        package.check_tobconenctor_consistency();
    }
    catch (const std::exception& e) {
        except_mess += (net->name() + ", check_tobconnector_consistency failed. " + std::string(e.what()) + "\n");
    }
    }

    if (except_mess.size() > 0) {
        except_mess = "Incre_route::iterate_routing(): " + except_mess;
        debug::debug(except_mess);
    }
}
//

void check_address(std::Vector<circuit::Net*> nets) {
    std::unordered_map<uintptr_t, std::String> address_to_net;
    std::unordered_map<std::String, std::String> net_share_reg;

    for (auto& net: nets) {
        const auto& package = net->pathpackage();
        for (const auto& [pbump, tobconnector, ptrack]: package._tob_to_track) {
            auto address = tobconnector.check_vert_to_track_reg_address();
            if (address_to_net.contains(address)) {
                net_share_reg.emplace(net->name(), address_to_net[address]);
            }
            else{
                address_to_net.emplace(address, net->name());
            }
        }
        for (const auto& [pbump, tobconnector, ptrack]: package._track_to_tob) {
            auto address = tobconnector.check_vert_to_track_reg_address();
            if (address_to_net.contains(address)) {
                net_share_reg.emplace(net->name(), address_to_net[address]);
            }
            else{
                address_to_net.emplace(address, net->name());
            }
        }
    }

    if (net_share_reg.size() > 0) {
        std::String except_mess{"check_address(): "};
        for (const auto& [net_name, share_net_name]: net_share_reg) {
            except_mess += (net_name + " share reg with " + share_net_name + "\n");
        }
        debug::debug(except_mess);
    }
}

auto Incre_route::iterate_routing(hardware::Interposer* interposer, RouteEngine& engine, std::Vector<circuit::Net*>& nets, HardwareRecorder& recorder) const -> bool {
    std::usize cycle{0}, min_cycle{10};
    while(cycle < min_cycle) {
        debug::info_fmt("cycle {} start", cycle);

        // init for cycle
        for (auto net: nets) {
            net->set_history_pathpackage();
            recorder.clear_history_records(net->pathpackage(), net->reuse_type().value());
            net->reset_pathpackage();
        }
        engine.reset_position();

        // routing for each net
        for (auto net: nets) {
            // check existing path
            auto routed_nets = engine.routed_nets();
            net->search_related_nets(routed_nets);
            
            // route
            auto res = net->incremental_route(interposer, engine.incre_route_strategy(), engine, false);    
            if (!res) {
                return cycle == 0 ? false : true;
            }
            engine.move_on();
            
            // update current cost
            recorder.update_recorders_current(net->pathpackage(), net->reuse_type().value());       
        }

        // update history cost 
        for (auto net:nets) {
            recorder.update_recorders_history(net->pathpackage(), net->reuse_type().value());   
        }

        // show path recorder status
        show_path_recorder_status(nets, recorder);

        debug::info_fmt("cycle {} done", cycle);
        engine.show_net_and_path();
        engine.show_data_in_cycle(cycle, nets);
        
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
    engine.init_route_data();

    for (auto& [m, net_v]: engine.nets_with_mode()) {
        std::erase_if(net_v, [&](auto& net) {
            return !net->modes().contains(engine.mode());
        });
    }
}

auto Incre_route::set_history_as_current(std::Vector<circuit::Net*>& nets, hardware::Interposer* interposer) const -> void {
    for (auto n: nets) {
        n->clear_current_package();
    }
    for (auto n: nets) {
        n->move_history_to_current(interposer);
    }
}

auto Incre_route::to_string() const -> const std::String {
    return "Incremental routing";
}

auto Incre_route::show_path_recorder_status(const std::Vector<circuit::Net*>& nets, const HardwareRecorder& recorder, bool show_all) const -> void {
try{
    std::unordered_map<std::string, std::vector<circuit::PathInOrder>> paths;
    for (auto& net: nets) {
        debug::info_fmt("net {} path_in_order", net->name());
        paths.insert({net->name(), net->path_in_order()});
    }
    recorder.show_path_recorder_status(paths, show_all);
}
catch(const std::exception& e) {
    debug::info_fmt("show_path_recorder_status(): {}", e.what());
}
}

}
