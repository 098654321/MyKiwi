#pragma once

#include <hardware/interposer.hh>
#include <std/collection.hh>
#include <std/integer.hh>
#include <std/memory.hh>
#include <std/utility.hh>
#include <global/debug/debug.hh>
#include <circuit/path/pathpackage.hh>


namespace kiwi::hardware {
    class Interposer;
    class Track;
    class COBConnector;
}

namespace kiwi::circuit {
    struct PathPackage;
    class Net;
}


namespace kiwi::algo{
    
    using routed_path = std::Vector<std::Tuple<kiwi::hardware::Track*, std::Option<kiwi::hardware::COBConnector>>>;
    class HardwareRecorder;

    class Node{
        // A track with additional attributes
    public:
        Node(const std::Rc<hardware::Track> track, const hardware::COBConnector& connector, const std::Rc<Node> prev_node, float cost, std::Option<bool> reuse_type, std::usize level)
        : _track{track}, _connector{connector}, _prev_node{prev_node}, _cost{cost}, _post_nodes{}, _reuse_type{reuse_type}, _level{level}
        {}
    
    public:
        auto remove_child(const std::Rc<Node> child) -> void;
        inline auto add_child(const std::Rc<Node> child) -> void {_post_nodes.emplace_back(child);}

        auto operator == (const Node& other) const -> bool {return this->_track->coord() == other._track->coord();}
        static inline bool CompareNodes(const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
            return a->_cost > b->_cost; // min_heap
        }
    
    public:
        inline auto parent() -> std::Option<std::Rc<Node>>;
        inline auto track() -> std::Rc<hardware::Track> {return this->_track;}
        inline auto post_nodes() -> std::Vector<std::Rc<Node>>& {return this->_post_nodes;}
        inline auto cost() -> float {return this->_cost;}
        inline auto connector() -> std::Option<hardware::COBConnector>& {return this->_connector;}
        inline auto reuse_type() -> std::Option<bool> {return this->_reuse_type;}
        inline auto level() -> std::usize {return this->_level;}

    private:
        std::Rc<hardware::Track> _track;
        std::Option<hardware::COBConnector> _connector;
        std::Rc<Node> _prev_node;
        std::Vector<std::Rc<Node>> _post_nodes; 
        float _cost;
        std::Option<bool> _reuse_type;
        std::usize _level;
    };

    struct Tree{
        // A tree of nodes, describing the path
        Tree(const std::Rc<Node> root, std::Option<bool> reuse_type);

        auto is_a_predecessor(const std::Rc<Node> current_node, const std::Rc<Node> to_be_checked) -> bool;
        auto backtrace(const std::Rc<Node> node) -> std::Vector<std::Rc<Node>>;
        auto reuse_type() const -> std::Option<bool> {return this->_reuse_type;}
        auto check_max_level(std::usize level) -> void;
    
        std::Rc<Node> _root;    
        std::Option<bool> _reuse_type;
        std::usize _max_level;
    };

    struct NodeTrackInterface{
        auto track_rootify(hardware::Track* track, hardware::COBConnector& connector, std::Option<bool> reuse_type) const -> std::Rc<Node>{
            auto track_sptr {std::make_shared<hardware::Track>(*track)};
            return std::make_shared<Node>(track_sptr, connector, nullptr, 0.0, reuse_type, 0);
        }

        auto nodes_trackify(std::Vector<std::Rc<Node>>& nodes) const -> routed_path{
            routed_path tracks{};
            std::transform(nodes.begin(), nodes.end(), std::back_inserter(tracks), [](const std::Rc<Node>& node){
                hardware::Track* new_track = new hardware::Track(*node->track().get());
                std::Option<hardware::COBConnector>& connector {node->connector()};
                return std::tuple{new_track, connector};
            });
            return tracks;
        }
    };

    class MazeRerouter{

    static constexpr float CUT_RATE = 0.5;
    
    public:
        MazeRerouter(bool incremental = false): _incremental{incremental}, _recorder{nullptr} {}

        auto bus_reroute(       // reroute through pointer path_ptrs
            hardware::Interposer* interposer, std::Vector<circuit::Net*>& net_ptrs, std::usize max_length
        ) const -> std::tuple<bool, std::usize>;

auto bus_reroute(
    hardware::Interposer* interposer, circuit::PathPackage* path_ptr, std::usize max_length, bool reuse_type
) const -> std::tuple<bool, std::usize>;

        auto set_recorder(HardwareRecorder* recorder) -> void {this->_recorder = recorder;}
    
    private:
        auto remove_tracks(
            circuit::PathPackage* path_ptr, float cut_rate = MazeRerouter::CUT_RATE
        ) const -> void;
        auto refind_path(
            hardware::Interposer* interposer, Tree& tree, circuit::PathPackage* path_ptr,\
            std::usize max_length, const std::HashSet<hardware::Track*>& end_tracks, std::usize bump_length
        ) const -> std::tuple<bool, std::usize>;
        auto cost_function(const std::Rc<Node> node, hardware::COBConnector& connector, const std::HashSet<hardware::Track*>& end_tracks, HardwareRecorder* recorder) const -> float;
        auto Manhattan_distance(const std::Rc<Node> node, const std::HashSet<hardware::Track*>& end_tracks) const -> std::usize;
    
    private:
        const NodeTrackInterface _node_track_interface {};
        bool _incremental;
        HardwareRecorder* _recorder;
    };

    auto check_found(
        const std::HashSet<hardware::Track*>& end_tracks, 
        hardware::Track* track
    ) -> bool;

    
}


