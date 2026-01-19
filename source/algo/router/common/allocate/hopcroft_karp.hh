#pragma once

#include <algo/router/common/allocatestrategy.hh>


namespace kiwi::algo {

class HKSolver {
public:
    HKSolver(std::usize size_l)\
        :_size_l{size_l}, _adj_left{}, _map_l{}, _map_r{}, _distance{}
    {}

    // match left(bumps) set to right(tracks)
    auto max_matching() -> std::usize;
    auto add_link(std::usize left, const std::HashSet<std::usize>& rights) -> void;
    auto add_link(std::usize left, std::usize right) -> void;
    auto matches_left() const -> const std::HashMap<std::usize, int>& {return this->_map_l;}
    auto matches_right() const -> const std::HashMap<std::usize, int>& {return this->_map_r;}

private:
    auto bfs() -> bool;
    auto dfs(std::usize u) -> bool;
    auto init() -> bool;

private:
    std::usize _size_l;
    std::HashMap<std::usize, std::HashSet<std::usize>> _adj_left;    // 左顶点集(bumps)的邻接表
    std::HashMap<std::usize, int> _map_l, _map_r;                    // 匹配结果
    std::HashMap<std::usize, int> _distance;                  
};

struct HK : public AllocateStrategy {

    // Implement the Hopcroft–Karp algorithm
    auto allocate(hardware::Interposer* interposer, std::Vector<circuit::Net*> nets) const -> void override;

private:
    auto resources_map(
        std::Vector<circuit::Net*>& nets
    ) const -> std::HashMap<hardware::TOBCoord, std::HashMap<std::usize, std::HashSet<std::usize>>>;
                                                            // bump index -> accessable track indexes

    auto related_bumps_map(
        std::Vector<circuit::Net*>& nets
    ) const -> std::Vector<std::HashSet<hardware::Bump*>>;
                                                            // begin bump index -> end bump* (of the same net)

    auto bumps_direction_map(
        std::Vector<circuit::Net*>& nets
    ) const -> std::HashMap<hardware::TOBCoord, std::HashMap<std::usize, hardware::TOBBumpDirection>>;

    auto randomly_pick(
        std::HashMap<hardware::TOBCoord, std::HashMap<std::usize, std::HashSet<std::usize>>>& map
    ) const -> std::Pair<hardware::TOBCoord, std::HashMap<std::usize, std::HashSet<std::usize>>>; 

    auto init_solver_adj(HKSolver& solver, const std::HashMap<std::usize, std::HashSet<std::usize>>& bump_map) const -> void;

    auto set_hardware(
        const std::Pair<hardware::TOBCoord, std::HashMap<std::usize, int>>&, const std::HashMap<std::usize, hardware::TOBBumpDirection>&,
        hardware::Interposer* , std::Vector<circuit::Net*>&
    ) const -> std::HashSet<hardware::Bump*>;

    auto update_related_bumps(
        std::HashMap<hardware::TOBCoord, std::HashMap<std::usize, std::HashSet<std::usize>>>&,
        const std::Vector<std::HashSet<hardware::Bump*>>&,
        const std::HashSet<hardware::Bump*>&
    ) const -> void;

};

}
