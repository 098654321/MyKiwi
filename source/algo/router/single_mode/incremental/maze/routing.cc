#include "./routing.hh"
#include <debug/debug.hh>
#include <circuit/net/nets.hh>
#include <ranges>
#include <algo/router/common/maze/path_length.hh>
#include <algo/router/single_mode/routeengine.hh>
#include <algo/router/routeerror.hh>
#include <type_traits>


namespace kiwi::algo {

auto IncreRouting::route_bump_to_bump_net(
    hardware::Interposer* interposer, circuit::BumpToBumpNet* net, RouteEngine& engine
) const -> bool {
    circuit::PathPackage path_package {};
try {
    debug::info(std::format("Incremental routing for net: {}", net->name()));

    // ports
    auto begin_bump = net->begin_bump();
    auto end_bump = net->end_bump();
    debug::check(begin_bump->tob() != end_bump->tob(), "Route bump in the same tob");

    // begin/end nodes
    auto begins_map = searching_points<hardware::Bump>(begin_bump, net, interposer);
    auto ends_map = searching_points<hardware::Bump>(end_bump, net, interposer);

    // route
    auto reuse_type = net->reuse_type();
    if (!reuse_type.has_value()) {
        throw std::logic_error("net reuse type should not be nullopt when routing");
    };
    path_package._regular_path = this->route_path(
        interposer, 
        map_to_vec<hardware::Bump>(begin_bump, begins_map, engine.recorder(), reuse_type.value()), map_to_set(ends_map),
        std::HashSet<hardware::Track*>{}, engine.recorder(), reuse_type.value()
    );

    // set
    auto head = std::get<0>(path_package._regular_path.front());
    auto tail = std::get<0>(path_package._regular_path.back());
    set_tobconnector(begins_map, head, begin_bump, path_package, true);
    set_tobconnector(ends_map, tail, end_bump, path_package, false);
    path_package._length += path_length(path_package._regular_path);
    net->set_pathpackage(path_package);

    return true;
}
catch (RetryExpt& err){
    debug::info_fmt("IncreRouting::route_bump_to_bump_net: {}", err.what());
    path_package.reset_all();
    return false;
}
}

auto IncreRouting::route_track_to_bump_net(
    hardware::Interposer* interposer, circuit::TrackToBumpNet* net, RouteEngine& engine
) const -> bool {
    circuit::PathPackage path_package {};
try {
    debug::info(std::format("Incremental routing for net: {}", net->name()));

    auto begin_track = net->begin_track();
    auto end_bump = net->end_bump();

    auto begins_map = searching_points<hardware::Track>(begin_track, net, interposer);
    auto ends_map = searching_points<hardware::Bump>(end_bump, net, interposer);

    auto reuse_type = net->reuse_type();
    if (!reuse_type.has_value()) {
        throw std::logic_error("net reuse type should not be nullopt when routing");
    };
    path_package._regular_path = this->route_path(
        interposer, 
        map_to_vec<hardware::Track>(begin_track, begins_map, engine.recorder(), reuse_type.value()), map_to_set(ends_map),
        std::HashSet<hardware::Track*>{}, engine.recorder(), reuse_type.value()
    );

    auto tail = std::get<0>(path_package._regular_path.back());
    set_tobconnector(ends_map, tail, end_bump, path_package, false);
    path_package._length += path_length(path_package._regular_path);
    net->set_pathpackage(path_package);

    return true;
}
catch(RetryExpt& err) {
    debug::info_fmt("IncreRouting::route_track_to_bump_net: {}", err.what());
    path_package.reset_all();
    return false;
}
}

auto IncreRouting::route_bump_to_track_net(
    hardware::Interposer* interposer, circuit::BumpToTrackNet* net, RouteEngine& engine
) const -> bool {
    circuit::PathPackage path_package {};
try {
    debug::info(std::format("Incremental routing for net: {}", net->name()));

    auto begin_bump = net->begin_bump();
    auto end_track = net->end_track();

    auto begins_map = searching_points<hardware::Bump>(begin_bump, net, interposer);
    auto ends_map = searching_points<hardware::Track>(end_track, net, interposer);

    auto reuse_type = net->reuse_type();
    if (!reuse_type.has_value()) {
        throw std::logic_error("net reuse type should not be nullopt when routing");
    };
    path_package._regular_path = this->route_path(
        interposer, 
        map_to_vec<hardware::Bump>(begin_bump, begins_map, engine.recorder(), reuse_type.value()), map_to_set(ends_map),
        std::HashSet<hardware::Track*>{}, engine.recorder(), reuse_type.value()
    );

    auto head = std::get<0>(path_package._regular_path.front());
    set_tobconnector(begins_map, head, begin_bump, path_package, true);
    path_package._length += path_length(path_package._regular_path);
    net->set_pathpackage(path_package);

    return true;
}
catch (RetryExpt& err) {
    debug::info_fmt("IncreRouting::route_bump_to_track_net: {}", err.what());
    path_package.reset_all();
    return false;
}
}

auto IncreRouting::route_bump_to_bumps_net(
    hardware::Interposer* interposer, circuit::BumpToBumpsNet* net, RouteEngine& engine
) const -> bool {
    circuit::PathPackage path_package {};
try {
    debug::info(std::format("Incremental routing for net: {}", net->name()));

    auto begin_bump = net->begin_bump();
    auto& end_bumps = net->end_bumps();

    algo::routed_path total_regular_path {};
    for (auto end_bump: end_bumps) {
        auto current_begin_map = searching_points<hardware::Bump>(begin_bump, net, interposer);
        for (auto& [track, connector]: total_regular_path) {
            current_begin_map.emplace(track, std::nullopt);
        }
        auto current_begin_tracks = map_to_vec<hardware::Bump>(begin_bump, current_begin_map, engine.recorder(), net->reuse_type().value());

        auto ends_map = searching_points<hardware::Bump>(end_bump, net, interposer);
        auto current_end_tracks = map_to_set(ends_map);

        auto regular_path = this->route_path(
            interposer, current_begin_tracks, current_end_tracks, std::HashSet<hardware::Track*>{},
            engine.recorder(), net->reuse_type().value()
        );
        // remove repeated tracks
        regular_path.erase(std::unique(regular_path.begin(), regular_path.end(), [](const auto& a, const auto& b) {
            return std::get<0>(a) == std::get<0>(b);
        }), regular_path.end());
        total_regular_path.insert(total_regular_path.end(), regular_path.begin(), regular_path.end());

        auto head = std::get<0>(regular_path.front());
        auto tail = std::get<0>(regular_path.back());
        set_tobconnector(current_begin_map, head, begin_bump, path_package, true);
        set_tobconnector(ends_map, tail, end_bump, path_package, false);
        path_package._length += path_length(regular_path) - 1;
    }
    path_package._regular_path = total_regular_path;
    path_package._length += 1;
    net->set_pathpackage(path_package);

    return true;
}
catch (RetryExpt& err) {
    debug::info_fmt("IncreRouting::route_bump_to_bumps_net: {}", err.what());
    path_package.reset_all();
    return false;
}
}

auto IncreRouting::route_track_to_bumps_net(
    hardware::Interposer* interposer, circuit::TrackToBumpsNet* net, RouteEngine& engine
) const -> bool {
    circuit::PathPackage path_package {};
try {
    debug::info(std::format("Incremental routing for net: {}", net->name()));

    auto begin_track = net->begin_track();
    auto& end_bumps = net->end_bumps();

    algo::routed_path total_regular_path {};
    for (auto& end_bump: end_bumps) {
        auto current_begin_map = searching_points<hardware::Track>(begin_track, net, interposer);
        for (auto& [track, connector]: total_regular_path) {
            current_begin_map.emplace(track, std::nullopt);
        }
        auto current_begin_tracks = map_to_vec<hardware::Track>(begin_track, current_begin_map, engine.recorder(), net->reuse_type().value());

        auto end_map = searching_points<hardware::Bump>(end_bump, net, interposer);
        auto current_end_tracks = map_to_set(end_map);

        auto regular_path = this->route_path(
            interposer, current_begin_tracks, current_end_tracks, std::HashSet<hardware::Track*>{},
            engine.recorder(), net->reuse_type().value()
        );
        // remove repeated tracks
        regular_path.erase(std::unique(regular_path.begin(), regular_path.end(), [](const auto& a, const auto& b) {
            return std::get<0>(a) == std::get<0>(b);
        }), regular_path.end());
        total_regular_path.insert(total_regular_path.end(), regular_path.begin(), regular_path.end());

        auto tail = std::get<0>(regular_path.back());
        set_tobconnector(end_map, tail, end_bump, path_package, false);
        path_package._length += path_length(regular_path) - 1;
    }
    path_package._regular_path = total_regular_path;
    path_package._length += 1;
    net->set_pathpackage(path_package);

    return true;
}
catch (RetryExpt& err) {
    debug::info_fmt("IncreRouting::route_track_to_bumps_net: {}", net->name());
    path_package.reset_all();
    return false;
}
}

auto IncreRouting::route_bump_to_tracks_net(
    hardware::Interposer* interposer, circuit::BumpToTracksNet* net, RouteEngine& engine
) const -> bool {
    circuit::PathPackage path_package {};
try {
    debug::info(std::format("Incremental routing for net: {}", net->name()));

    auto begin_bump = net->begin_bump();
    auto& end_tracks = net->end_tracks();

    routed_path total_regular_path {};
    for (auto& end_track: end_tracks) {
        auto current_begin_map = searching_points<hardware::Bump>(begin_bump, net, interposer);
        for (auto& [track, connector]: total_regular_path) {
            current_begin_map.emplace(track, std::nullopt);
        }
        auto current_begin_tracks = map_to_vec<hardware::Bump>(begin_bump, current_begin_map, engine.recorder(), net->reuse_type().value());

        auto end_map = searching_points<hardware::Track>(end_track, net, interposer);
        auto current_end_tracks = map_to_set(end_map);

        auto regular_path = this->route_path(
            interposer, current_begin_tracks, current_end_tracks, std::HashSet<hardware::Track*>{},
            engine.recorder(), net->reuse_type().value()
        );
        // remove repeated tracks
        regular_path.erase(std::unique(regular_path.begin(), regular_path.end(), [](const auto& a, const auto& b) {
            return std::get<0>(a) == std::get<0>(b);
        }), regular_path.end());
        total_regular_path.insert(total_regular_path.end(), regular_path.begin(), regular_path.end());

        auto head = std::get<0>(regular_path.front());
        set_tobconnector(current_begin_map, head, begin_bump, path_package, true);
        path_package._length += path_length(regular_path) - 1;
    }

    path_package._regular_path = total_regular_path;
    path_package._length += 1;
    net->set_pathpackage(path_package);
    
    return true;
}
catch (RetryExpt& err) {
    debug::info_fmt("IncreRouting::route_bump_to_tracks_net: {}", net->name());
    path_package.reset_all();
    return false;
}
}

auto IncreRouting::route_tracks_to_bumps_net(
    hardware::Interposer* interposer, circuit::TracksToBumpsNet* net, RouteEngine& engine
) const -> bool {
    circuit::PathPackage path_package {};
try {
    debug::info(std::format("Incremental routing for net: {}", net->name()));

    auto begin_tracks = net->begin_tracks();
    auto& end_bumps = net->end_bumps();

    routed_path total_regular_path {};
    for (auto& end_bump: end_bumps) {
        std::sort(begin_tracks.begin(), begin_tracks.end(), [&](auto& t1, auto& t2){
            return engine.recorder().track_cost(t1, net->reuse_type().value()) < engine.recorder().track_cost(t2, net->reuse_type().value());
        });
        auto ends_map = searching_points<hardware::Bump>(end_bump, net, interposer);

        auto regular_path = this->route_path(
            interposer, begin_tracks, map_to_set(ends_map), std::HashSet<hardware::Track*>{},
            engine.recorder(), net->reuse_type().value()
        );
        // remove repeated tracks
        regular_path.erase(std::unique(regular_path.begin(), regular_path.end(), [](const auto& a, const auto& b) {
            return std::get<0>(a) == std::get<0>(b);
        }), regular_path.end());
        total_regular_path.insert(total_regular_path.end(), regular_path.begin(), regular_path.end());

        auto tail = std::get<0>(regular_path.back());
        set_tobconnector(ends_map, tail, end_bump, path_package, false);
        path_package._length += path_length(regular_path);

        for (auto& [t, connector]: regular_path) {
            begin_tracks.emplace_back(t);
        }
    }
    path_package._regular_path = total_regular_path;
    net->set_pathpackage(path_package);

    return true;
}
catch (RetryExpt& err) {
    debug::info_fmt("IncreRouting::route_tracks_to_bumps_net: {}", net->name());
    path_package.reset_all();
    return false;
}
}


// for debug
auto check_preroute_net_tobconnector_consistency(
    const std::Vector<circuit::Net*>& nets, std::String info
) -> void {
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
        except_mess = info + except_mess;
        debug::debug(except_mess);
    }
}
//


auto IncreRouting::route_sync_net(
    hardware::Interposer* interposer, circuit::SyncNet* sync_net, RouteEngine& engine
) const -> bool {
try {
    debug::info(std::format("Incremental routing for net: {}", sync_net->name()));

    std::HashSet<hardware::Track*> occupied_tracks_vec {}; 
    std::usize max_length {0};
    for (auto& net: sync_net->bttnets()) {
        occupied_tracks_vec.emplace(net->end_track());
    }
    for (auto& net: sync_net->ttbnets()) {
        occupied_tracks_vec.emplace(net->begin_track());
    }

    if (sync_net->btbnets().size() > 0){
        auto current_len = sync_preroute_bump_to_bump(
            interposer, sync_net->btbnets(), occupied_tracks_vec, engine
        );
        max_length = current_len > max_length ? current_len : max_length;
    }
    if (sync_net->ttbnets().size() > 0){
        auto current_len = sync_preroute_track_to_bump(
            interposer, sync_net->ttbnets(), occupied_tracks_vec, engine
        );
        max_length = current_len > max_length ? current_len : max_length;
    }
    if (sync_net->bttnets().size() > 0){
        auto current_len = sync_preroute_bump_to_track(
            interposer, sync_net->bttnets(), occupied_tracks_vec, engine
        );
        max_length = current_len > max_length ? current_len : max_length;
    }

    debug::info("Prerouting done");

    auto btb_nets = std::Vector<circuit::Net*> {};
    auto ttb_nets = std::Vector<circuit::Net*> {};
    auto btt_nets = std::Vector<circuit::Net*> {};
    for (auto& net: sync_net->btbnets()) {
        btb_nets.emplace_back(net.get());
    }
    for (auto& net: sync_net->ttbnets()) {
        ttb_nets.emplace_back(net.get());
    }
    for (auto& net: sync_net->bttnets()) {
        btt_nets.emplace_back(net.get());
    }

    while (true){
        auto [success, ml] = sync_incremental_reroute(
            interposer, btb_nets, max_length, engine
        );
        if (success){
            auto [success, ml] = sync_incremental_reroute(
                interposer, ttb_nets, max_length, engine
            );
            if (success){
                auto [success, ml] = sync_incremental_reroute(
                    interposer, btt_nets, max_length, engine
                );
                max_length = ml;
                if (success){
                    break;      // break only when all three nets are successfully routed
                }
                else{
                    continue;
                }
            }
            else{
                max_length = ml;
                continue;
            }
        }
        else{
            max_length = ml;
            continue;
        }
    }
    std::usize total_nets {sync_net->btbnets().size() + sync_net->bttnets().size() + sync_net->ttbnets().size()};
    std::usize total_length = total_nets * max_length;

    // check
    std::usize sum {0};
    for (auto& net: sync_net->ttbnets()) {
        sum += net->pathpackage()._length;
    }
    for (auto& net: sync_net->btbnets()) {
        sum += net->pathpackage()._length;
    }
    for (auto& net: sync_net->bttnets()) {
        sum += net->pathpackage()._length;
    }
    assert(sum == total_length);

    sync_net->collect_package();

    return true;
}
catch (RetryExpt& err) {
    debug::info_fmt("IncreRouting::route_sync_net: {}", err.what());
    auto reset_reg_state = [](auto& nets) {
        for (auto& net: nets) {
            net->pathpackage().reset_all();
        }
    };
    reset_reg_state(sync_net->btbnets());
    reset_reg_state(sync_net->ttbnets());
    reset_reg_state(sync_net->bttnets());

    return false;
}
}

auto IncreRouting::sync_preroute_bump_to_bump(
    hardware::Interposer* interposer,
    std::Vector<std::Rc<circuit::BumpToBumpNet>>& sync_net,
    std::HashSet<hardware::Track*>& occupied_tracks_vec,
    RouteEngine& engine
) const -> std::usize {
    if (sync_net.size() <= 0){
        throw FinalError("IncreRouting::sync_preroute_bump_to_bump(): empty sync_net");
    }

    for (auto& net: sync_net) {
        auto begin_bump = net->begin_bump();
        auto end_bump  =net->end_bump();
        debug::check(begin_bump->tob() != end_bump->tob(), "IncreRouting::preroute(): Route bump in the same tob");

        auto begin_map = interposer->available_tracks_bump_to_track(begin_bump);
        auto begin_track_vec = map_to_vec(begin_bump, begin_map, engine.recorder(), net->reuse_type().value());
        auto end_map = interposer->available_tracks_track_to_bump(end_bump);
        auto end_track_set = map_to_set(end_map);

        circuit::PathPackage path_package{};
        path_package._regular_path = this->route_path(
            interposer, begin_track_vec, end_track_set, occupied_tracks_vec,
            engine.recorder(), net->reuse_type().value()
        );

        auto head = std::get<0>(path_package._regular_path.front());
        if (!begin_map.contains(head)) {
            throw FinalError("IncreRouting::sync_preroute_bump_to_bump(): head of path not in begin_map");
        }
        auto begin_tobconnector = begin_map.find(head)->second;
        begin_tobconnector.give_out();
        path_package._tob_to_track.emplace_back(
            std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*>(begin_bump, begin_tobconnector, head)
        );

        auto tail = std::get<0>(path_package._regular_path.back());
        if (!end_map.contains(tail)) {
            throw FinalError("IncreRouting::sync_preroute_bump_to_bump(): tail of path not in end_map");
        }
        auto end_tobconnector = end_map.find(tail)->second;
        end_tobconnector.give_out();
        path_package._track_to_tob.emplace_back(
            std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*>(end_bump, end_tobconnector, tail)
        );

        path_package._length += path_length(path_package._regular_path) + 2;
        net->set_pathpackage(path_package);
    }

    auto max_length = std::usize{0};
    for (auto &net: sync_net) {
        max_length = std::max(max_length, net->pathpackage()._length);
    }

    return max_length;
}

auto IncreRouting::sync_preroute_bump_to_track(
    hardware::Interposer* interposer,
    std::Vector<std::Rc<circuit::BumpToTrackNet>>& sync_net,
    std::HashSet<hardware::Track*>& occupied_tracks_vec,
    RouteEngine& engine
) const -> std::usize {
    if (sync_net.size() <= 0){
        throw FinalError("IncreRouting::sync_preroute_bump_to_track(): empty sync_net");
    }

    for (auto& net: sync_net) {
        auto begin_bump = net->begin_bump();
        auto end_track = net->end_track();

        auto begin_map = interposer->available_tracks_bump_to_track(begin_bump);
        auto begin_track_vec = map_to_vec(begin_bump, begin_map, engine.recorder(), net->reuse_type().value());
        auto end_set = std::HashSet<hardware::Track*>{end_track};

        std::erase_if(occupied_tracks_vec, [&](const auto& track) {
            return track->coord() == end_track->coord();
        });

        circuit::PathPackage path_package{};
        path_package._regular_path = this->route_path(
            interposer, begin_track_vec, end_set, occupied_tracks_vec,
            engine.recorder(), net->reuse_type().value()
        );

        auto head = std::get<0>(path_package._regular_path.front());
        if (!begin_map.contains(head)) {
            throw FinalError("IncreRouting::sync_preroute_bump_to_track(): head of path not in begin_map");
        }
        auto begin_tobconnector = begin_map.find(head)->second;
        begin_tobconnector.give_out();
        path_package._tob_to_track.emplace_back(
            std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*>(begin_bump, begin_tobconnector, head)
        );

        path_package._length += path_length(path_package._regular_path) + 1;
        net->set_pathpackage(path_package);
    }

    auto max_length = std::usize{0};
    for (auto &net: sync_net) {
        max_length = std::max(max_length, net->pathpackage()._length);
    }

    return max_length;
}

auto IncreRouting::sync_preroute_track_to_bump(
    hardware::Interposer* interposer,
    std::Vector<std::Rc<circuit::TrackToBumpNet>>& sync_net,
    std::HashSet<hardware::Track*>& occupied_tracks_vec,
    RouteEngine& engine
) const -> std::usize {
    if (sync_net.size() <= 0){
        throw FinalError("IncreRouting::sync_preroute_track_to_bump(): empty sync_net");
    }

    for (auto& net: sync_net) {
        auto begin_track = net->begin_track();
        auto end_bump = net->end_bump();

        auto begin_track_vec = std::Vector<hardware::Track*>{begin_track};
        auto end_map = interposer->available_tracks_track_to_bump(end_bump);
        auto end_set = map_to_set(end_map);

        std::erase_if(occupied_tracks_vec, [&](const auto& track) {
            return track->coord() == begin_track->coord();
        });

        circuit::PathPackage path_package{};
        path_package._regular_path = this->route_path(
            interposer, begin_track_vec, end_set, occupied_tracks_vec,
            engine.recorder(), net->reuse_type().value()
        );

        auto tail = std::get<0>(path_package._regular_path.back());
        if (!end_map.contains(tail)) {
            throw FinalError("IncreRouting::sync_preroute_track_to_bump(): tail of path not in end_map");
        }
        auto end_tobconnector = end_map.find(tail)->second;
        end_tobconnector.give_out();
        path_package._track_to_tob.emplace_back(
            std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*>(end_bump, end_tobconnector, tail)
        );

        path_package._length += path_length(path_package._regular_path) + 1;
        net->set_pathpackage(path_package);
    }
    auto max_length = std::usize{0};
    for (auto &net: sync_net) {
        max_length = std::max(max_length, net->pathpackage()._length);
    }

    return max_length;
}

auto IncreRouting::sync_incremental_reroute(
    hardware::Interposer* interposer,
    std::Vector<circuit::Net*>& nets,
    std::usize max_length,
    RouteEngine& engine
) const -> std::tuple<bool, std::usize> {
    debug::info_fmt("sync_incremental_reroute: max_length {}, nets {}", max_length, nets.size());

    std::Vector<circuit::Net*> nets_to_be_rerouted {};
    std::String reroute_nets{};
    for (auto& net: nets) {
        if (net->pathpackage()._length < max_length) {
            nets_to_be_rerouted.push_back(net);
            reroute_nets += net->name() + "\n";
        }
    }

    if (nets_to_be_rerouted.size() > 0) {
        debug::info(std::format("sync_incremental_reroute(): reroute nets {}", reroute_nets));
        for (auto& net: nets_to_be_rerouted) {
            debug::info_fmt("sync_incremental_reroute: reroute net {} length {}", net->name(), net->pathpackage()._length);
        }
    }

    if (nets_to_be_rerouted.size() > 0) {
        auto [success, ml] = _rerouter->bus_reroute(interposer, nets_to_be_rerouted, max_length);
        
        if (success){   // routing done with ml == max_length
            if (max_length != ml){
                throw FinalError("MazeRouteStrategy::sync_reroute(): max_length != ml when succeed");
            }
            debug::info_fmt("sync_incremental_reroute: success max_length {}", max_length);
            return std::tuple<std::usize, std::usize>{true, max_length};
        }
        else{           // have longer path OR routing failed
            debug::info_fmt("sync_incremental_reroute: failed new_max_length {}", ml);
            return std::tuple<std::usize, std::usize>{false, ml};
        }
    }
    else{
        return std::tuple<std::usize, std::usize>{true, max_length};
    }
}

auto IncreRouting::route_path(
    hardware::Interposer* interposer, 
    const std::Vector<hardware::Track*>& begin_tracks,
    const std::HashSet<hardware::Track*>& end_tracks,
    const std::HashSet<hardware::Track*>& occupied_tracks,
    HardwareRecorder& recorder,
    bool reuse_type
) const -> algo::routed_path {
    auto path_info = maze_search(interposer, begin_tracks, end_tracks, occupied_tracks, recorder, reuse_type);  // negative sequence

    auto path = algo::routed_path {};   // positive sequence
    std::transform(path_info.rbegin(), path_info.rend(), std::back_inserter(path), [](const auto& p){
        return p;
    });

    for (auto& [track, cobconnector]: path) {
        if (cobconnector.has_value()) {
            cobconnector.value().suspend();
        }
    }

    return path;
}

auto IncreRouting::maze_search(
    hardware::Interposer* interposer, 
    const std::Vector<hardware::Track*>& begin_tracks,
    const std::HashSet<hardware::Track*>& end_tracks,
    const std::HashSet<hardware::Track*>& occupied_tracks,
    HardwareRecorder& recorder,
    bool reuse_type
) const -> algo::routed_path {
    using namespace hardware;

    auto queue = std::MinHeap<std::Pair<Track*, float>, CompareTrack>{};
    auto prev_track_infos = 
        std::HashMap<Track*, std::Option<std::Tuple<Track*, COBConnector>>>{};

    for (auto& t: begin_tracks) {
        queue.push(std::Pair<Track*, float>(t, 0));
        prev_track_infos.insert({t, std::nullopt});
    }
    
    while (!queue.empty()) {
        auto [track, current_cost] = queue.top();
        queue.pop();

        if (check_found(end_tracks, track)) {
            auto path = std::Vector<std::Tuple<Track*, std::Option<COBConnector>>>{};
            auto cur_track = track;
            while (true) {
                auto prev_track_info = prev_track_infos.find(cur_track);
                if(prev_track_info == prev_track_infos.end()){
                    throw FinalError("MazeRouteStrategy::maze_search(): cannot find previous track");
                }
                // Reach start track
                if (!prev_track_info->second.has_value()) {
                    break;
                }

                path.emplace_back(cur_track, std::get<1>(*prev_track_info->second));
                cur_track = std::get<0>(*prev_track_info->second);
            }
            path.emplace_back(cur_track, std::nullopt);

            return {path};
        }

        for (auto& [next_track, connector] : interposer->adjacent_idle_tracks(track)) {
            if (prev_track_infos.find(next_track) != prev_track_infos.end() || occupied_tracks.contains(next_track)) {
                continue;
            }

            queue.push(std::Pair<hardware::Track*, float>(next_track, current_cost + recorder.expand_cost(track, connector, reuse_type)));             
            prev_track_infos.insert({
                next_track, 
                std::Tuple<Track*, COBConnector>{track, connector}
            });
        }
    }

    throw RetryExpt("MazeRouteStrategy::maze_search(): path not found");
}


template <class Node>
auto IncreRouting::searching_points(
    Node* node, circuit::Net* net, hardware::Interposer* interposer
) const -> std::HashMap<hardware::Track*, std::Option<hardware::TOBConnector>> {
    static_assert(
        std::is_same<Node, hardware::Bump>::value || std::is_same<Node, hardware::Track>::value,\
        "IncreRouting::searching_points(): Node must be hardware::Bump or hardware::Track"
    );

    auto map = std::HashMap<hardware::Track*, std::Option<hardware::TOBConnector>>{};
    if constexpr(std::is_same<Node, hardware::Bump>::value) {
        auto track_and_connector = interposer->available_tracks_bump_to_track(node);
        for (auto& [t, tobconnector]: track_and_connector) {
            map.emplace(t, tobconnector);
        }
    }
    else if constexpr(std::is_same<Node, hardware::Track>::value) {
        map.emplace(node, std::nullopt);
    }
    auto points = existing_path_vec<Node>(node, net);
    for (auto t: points) {
        map.emplace(t, std::nullopt);
    }
    if (map.empty()) {
        throw RetryExpt("IncreRouting::searching_points(): no available tracks");
    }

    return map;
}

auto IncreRouting::set_tobconnector(
    std::HashMap<hardware::Track*, std::Option<hardware::TOBConnector>>& map, hardware::Track* track,
    hardware::Bump* bump, circuit::PathPackage& path_package, bool head
) const -> void {
    auto iter = map.find(track);
    if (iter == map.end()) {
        throw FinalError("IncreRouting::set)tobconnector(): cannot find track in tracks_map");
    }

    if (iter->second.has_value()) {
        iter->second.value().give_out();
        if (head) {
            path_package._tob_to_track.emplace_back(
                std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*>(bump, iter->second.value(), track)
            );
        }
        else {
            path_package._track_to_tob.emplace_back(
                std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*>(bump, iter->second.value(), track)
            );
        }
        
        path_package._length += 1;
    }
    else {
        debug::debug(std::format("set_tobconnector: track {} connect to an existing net_path", track->coord()));
    }
}

template <class Node>
auto IncreRouting::map_to_vec(
    Node* node, const std::HashMap<hardware::Track*, std::Option<hardware::TOBConnector>>& map, HardwareRecorder& recorder, bool reuse_type
) const -> std::Vector<hardware::Track*> {
    static_assert(
        std::is_same<Node, hardware::Bump>::value || std::is_same<Node, hardware::Track>::value,\
        "IncreRouting::map_to_vec(): Node must be hardware::Bump or hardware::Track"
    );

    auto vec = std::Vector<hardware::Track*>{};
    for (auto& [t, tobconnector]: map) {
        vec.emplace_back(t);
    }

    std::sort(vec.begin(), vec.end(), [&](hardware::Track* track1, hardware::Track* track2) {
        if constexpr(std::is_same<Node, hardware::Bump>::value) {
            return recorder.bump_to_track_cost(node->coord(), node->index(), track1, reuse_type) < recorder.bump_to_track_cost(node->coord(), node->index(), track2, reuse_type);
        }
        else {
            return recorder.track_cost(track1, reuse_type) < recorder.track_cost(track2, reuse_type);
        }
    });
    return vec;
}

auto IncreRouting::map_to_set(const std::HashMap<hardware::Track*, std::Option<hardware::TOBConnector>>& map) const -> std::HashSet<hardware::Track*> {
    auto set = std::HashSet<hardware::Track*>{};
    for (auto& [t, _]: map) {
        set.emplace(t);
    }
    return set;
}

template <class Node>
auto IncreRouting::map_to_vec(
    Node* node, const std::HashMap<hardware::Track*, hardware::TOBConnector>& map, HardwareRecorder& recorder, bool reuse_type
) const -> std::Vector<hardware::Track*> {
    static_assert(
        std::is_same<Node, hardware::Bump>::value || std::is_same<Node, hardware::Track>::value,\
        "IncreRouting::map_to_vec(): Node must be hardware::Bump or hardware::Track"
    );

    auto vec = std::Vector<hardware::Track*>{};
    for (auto& [t, _]: map) {
        vec.emplace_back(t);
    }
    std::sort(vec.begin(), vec.end(), [&](hardware::Track* track1, hardware::Track* track2) {
        if constexpr(std::is_same<Node, hardware::Bump>::value) {
            return recorder.bump_to_track_cost(node->coord(), node->index(), track1, reuse_type) < recorder.bump_to_track_cost(node->coord(), node->index(), track2, reuse_type);
        }
        else {
            return recorder.track_cost(track1, reuse_type) < recorder.track_cost(track2, reuse_type);
        }
    });
    return vec;
}

auto IncreRouting::map_to_set(const std::HashMap<hardware::Track*, hardware::TOBConnector>& map) const -> std::HashSet<hardware::Track*> {
    auto set = std::HashSet<hardware::Track*>{};
    for (auto& [t, _]: map) {
        set.emplace(t);
    }
    return set;
}

}

