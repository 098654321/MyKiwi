#include "./hopcroft_karp.hh"
#include "hardware/cob/cobunit.hh"
#include <std/collection.hh>
#include <algo/router/routeerror.hh>
#include <format>
#include <random>
#include <ctime>
#include <climits>


namespace kiwi::algo {

auto HK::allocate(hardware::Interposer* interposer, std::Vector<circuit::Net*> nets) const -> void {
    // collect and divide map<bumps, available_tracks> to tobs
    auto resources = resources_map(nets);
    // collect related bumps
    auto bump_link = related_bumps_map(nets);
    // collect register direction
    auto bump_direction = bumps_direction_map(nets);

    while(!resources.empty()) {
        auto tob_map = randomly_pick(resources);
        auto [tobcoord, bump_map] = tob_map;

        HKSolver solver{bump_map.size()};
        init_solver_adj(solver, bump_map);
        auto max_matches = solver.max_matching();
        if (max_matches < bump_map.size()) {
            throw FinalError(
                std::format("Allocating tracks for bumps failed on tob ({}, {})", tobcoord.row, tobcoord.col)
            );
        }
        else {
            auto match_result = solver.matches_left();
            auto bumps = set_hardware(
                std::Pair<hardware::TOBCoord, std::HashMap<std::usize, int>>(tobcoord, match_result),
                bump_direction.at(tobcoord),
                interposer, nets
            );
            update_related_bumps(resources, bump_link, bumps);
        }
    }
    // 检查 net 的 package 的 bump 里面的 bump 数量和 net 的数量是否一致，以及 tob 的 mux 是否有重复
    // 查一下原来 tob 分配 connector 的方法，bump 会不会连到对面的 bank 上
}

auto HK::resources_map(
    std::Vector<circuit::Net*>& nets
) const -> std::HashMap<hardware::TOBCoord, std::HashMap<std::usize, std::HashSet<std::usize>>> {
    std::HashMap<hardware::TOBCoord, std::HashMap<std::usize, std::HashSet<std::usize>>> map {};
    
    for (auto& net: nets) {
        auto net_map = net->accessable_cobunit();

        // for all bumps in net
        for (auto& p: net_map) {
            // get bump and available cobunits
            auto& [b, cobunits] = p;
            std::HashSet<std::usize> indexes {};
            
            for (auto unit: cobunits){
                for (std::usize i = 0; i < 8; ++i) {
                    indexes.emplace(
                        (unit/8)*64 + i*8 + unit
                    );
                }
            }

            auto tobcoord = b->tob()->coord();
            auto bump_index = b->coord().index;
            if (map.contains(tobcoord)) {
                map.at(tobcoord).emplace(bump_index, indexes);
            }
            else{
                auto bump_map = std::HashMap<std::usize, std::HashSet<std::usize>>{{bump_index, indexes}};
                map.emplace(tobcoord, bump_map);
            }
        }
    }

    return map;
}

auto HK::related_bumps_map(
    std::Vector<circuit::Net*>& nets
) const -> std::Vector<std::HashSet<hardware::Bump*>> {
    std::Vector<std::HashSet<hardware::Bump*>> sets{};

    for (auto& net: nets) {
        auto bump_map = net->nodes_map();

        auto set = std::HashSet<hardware::Bump*>{};
        for (auto& [begin, ends]: bump_map) {
            set.emplace(begin);
            for (auto& end: ends) {
                set.emplace(end);
            }
            sets.emplace_back(set);
        }
    }
    return sets;
}

auto HK::bumps_direction_map(
    std::Vector<circuit::Net*>& nets
) const -> std::HashMap<hardware::TOBCoord, std::HashMap<std::usize, hardware::TOBBumpDirection>> {
    std::HashMap<hardware::TOBCoord, std::HashMap<std::usize, hardware::TOBBumpDirection>> map{};

    for (auto& net: nets) {
        auto net_map = net->nodes_direction();

        for (auto& [b, direction]: net_map) {
            auto& tobcoord = b->tob()->coord();
            auto bump_index = b->coord().index;
            if (map.contains(tobcoord)) {
                map.at(tobcoord).emplace(bump_index, direction);
            }
            else {
                auto b_map = std::HashMap<std::usize, hardware::TOBBumpDirection> {
                    {bump_index, direction}
                };
                map.emplace(tobcoord, b_map);
            }
        }
    }
    return map;
}

// randomly pop a tob
auto HK::randomly_pick(
    std::HashMap<hardware::TOBCoord, std::HashMap<std::usize, std::HashSet<std::usize>>>& map
) const -> std::Pair<hardware::TOBCoord, std::HashMap<std::usize, std::HashSet<std::usize>>> {
    if (map.empty()) {
        return std::Pair<hardware::TOBCoord, std::HashMap<std::usize, std::HashSet<std::usize>>> {};
    }

    static std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));  
    std::uniform_int_distribution<size_t> dist(0, map.size() - 1);
    size_t randomIndex = dist(rng);
    auto it = map.begin();
    std::advance(it, randomIndex);
    auto result = *it;
    map.erase(it);
    
    return result;
}

auto HK::init_solver_adj(HKSolver& solver, const std::HashMap<std::usize, std::HashSet<std::usize>>& bump_map) const -> void {
    for (auto& [bump_index, tracks]: bump_map) {
        solver.add_link(bump_index, tracks);
    }
}

auto HK::set_hardware( 
    const std::Pair<hardware::TOBCoord, std::HashMap<std::usize, int>>& matches, const std::HashMap<std::usize, hardware::TOBBumpDirection>& direcs,
    hardware::Interposer* interposer, std::Vector<circuit::Net*>& nets
) const -> std::HashSet<hardware::Bump*> {
try{
    auto& [tobcoord, bump_matches] = matches;
    auto tob = interposer->get_tob(tobcoord);
    assert(tob.has_value());

    // store allocated tracks in bumps
    auto& tob_bumps = (*tob)->bumps();
    for (auto& [b_index, t_index]: bump_matches) {
        assert(t_index >= 0 && tob_bumps.contains(b_index));

        auto& bump = tob_bumps.at(b_index);

        auto& bump_coord = bump->coord();
        auto track_coord = hardware::TrackCoord(bump_coord.row, bump_coord.col, hardware::TrackDirection::Vertical, t_index);
        auto track = interposer->get_track(track_coord);
        assert(track.has_value());

        bump->set_allocated_track(track.value());
    }

    // create TOBConnector and store it in net->package
    for (auto& [b_index, t_index]: bump_matches) {
        auto& bump = tob_bumps.at(b_index);
        auto& bump_coord = bump->coord();
        auto track_coord = hardware::TrackCoord(bump_coord.row, bump_coord.col, hardware::TrackDirection::Vertical, t_index);
        auto track = interposer->get_track(track_coord);
        auto direction = direcs.at(b_index);

        for (auto& net: nets) {
            if (net->check_relativity(bump.get()) != nullptr) {
                auto& package = net->pathpackage();
                auto& t = (direction == hardware::TOBBumpDirection::BumpToTOB ? package._tob_to_track : package._track_to_tob);

                auto tobconnector = (*tob)->bump_track_connectors_chain(b_index, t_index, direction);
                t.emplace_back(
                    std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*>(bump.get(), tobconnector, track.value())
                );
                break;  // 在不同 net 当中重复出现的 bump 可以不同设置，因为布线之前会所有一下已有的路径
            }
        }
    }

    // collect matched bumps
    std::HashSet<hardware::Bump*> bumps{};
    for(auto& [b_index, _]: bump_matches) {
        bumps.emplace(tob_bumps.at(b_index).get());
    }
    return bumps;
}
catch(RetryExpt& re) {
    std::String msg = std::format("set hardware(): {}", re.what());
    throw RetryExpt{msg};
}
catch(std::exception& e) {
    debug::debug_fmt("Unexpected exception in set hardware() >> {}", e.what());
    std::exit(EXIT_FAILURE);
}
}

auto HK::update_related_bumps(
    std::HashMap<hardware::TOBCoord, std::HashMap<std::usize, std::HashSet<std::usize>>>& resources,
    const std::Vector<std::HashSet<hardware::Bump*>>& links,
    const std::HashSet<hardware::Bump*>& bumps
) const -> void {
    for (auto& b: bumps) {
        auto allocated_t_index = b->allocated_track()->coord().index;
        auto cobunit = (allocated_t_index/64)*8 + allocated_t_index%8;
        auto new_available_tracks = std::HashSet<std::usize>{};
        for (std::usize i = 0; i < 8; ++i) {
            new_available_tracks.emplace((cobunit/8)*64 + i*8 + cobunit);
        }

        for (auto& set: links) {
            if (set.contains(b)) {
                for (auto& s: set) {
                    // update bump
                    s->intersect_access_unit(std::HashSet<std::usize>{cobunit});

                    // update reources map
                    auto tob = s->tob();
                    if (resources.contains(tob->coord())) {
                        auto& tob_map = resources.at(tob->coord());
                        tob_map.at(s->coord().index) = new_available_tracks;
                    }
                }
            }
        }
    }
}



auto HKSolver::add_link(std::usize left, const std::HashSet<std::usize>& rights) -> void {
    this->_adj_left.emplace(left, rights);
}

auto HKSolver::add_link(std::usize left, std::usize right) -> void {
    if (this->_adj_left.contains(left)) {
        this->_adj_left.at(left).emplace(right);
    }
    else {
        this->_adj_left.emplace(
            left, std::HashSet<std::usize>{right}
        );
    }
} 

auto HKSolver::bfs() -> bool {
    std::Queue<std::usize> q;

    for (auto& [u, _]: this->_adj_left) {
        if (this->_map_l.at(u) == -1) {
            this->_distance.at(u) = 0;
            q.push(u);
        } else {
            this->_distance.at(u) = INT_MAX;
        }
    }

    bool found = false;
    while (!q.empty()) {
        auto u = q.front();
        q.pop();

        for (auto v : this->_adj_left.at(u)) {
            if (this->_map_r.at(v) == -1) {
                found = true;
            }
            else if (this->_distance.at(this->_map_r.at(v)) == INT_MAX) {
                this->_distance.at(this->_map_r.at(v)) = this->_distance.at(u) + 1;
                q.push(this->_map_r.at(v));
            }
        }
    }

    return found;
}

auto HKSolver::dfs(std::usize u) -> bool {
    for (auto v : this->_adj_left.at(u)) {
        if (this->_map_r.at(v) == -1 || (this->_distance.at(this->_map_r.at(v)) == this->_distance.at(u) + 1 && dfs(this->_map_r.at(v)))) {
            this->_map_l.at(u) = v;
            this->_map_r.at(v) = u;
            return true;
        }
    }
    this->_distance.at(u) = INT_MAX;
    return false;
}

auto HKSolver::max_matching() -> std::usize {
try{
    if (!this->init()) {
        debug::error("Empty adjcent table!");
        std::exit(EXIT_FAILURE);
    }

    int result = 0;

    while (bfs()) {
        for (auto& [u, _]: this->_adj_left) {
            if (this->_map_l.at(u) == -1 && dfs(u)) {
                result++;
            }
        }
    }

    return result;
}
catch(RetryExpt& re) {
    std::String msg = std::format("max_mactching(): {}", re.what());
    throw RetryExpt{msg};
}
catch(std::exception& e) {
    debug::debug_fmt("Unexpected exception in max_matching(): {}", e.what());
    std::exit(EXIT_FAILURE);
}
}

auto HKSolver::init() -> bool {
    if (this->_adj_left.empty()) 
        return false;
    
    for (auto& [left, rights]: this->_adj_left) {
        this->_map_l.emplace(left, -1);
        this->_distance.emplace(left, 0);
        for (auto& right: rights) {
            this->_map_r.emplace(right, -1);
        }
    }
    return true;
}

}

