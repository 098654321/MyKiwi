#include "./mazererouter.hh"
#include "../routeerror.hh"
#include "./path_length.hh"

#include <algorithm>
#include <format>
#include <cmath>
#include <stdexcept>
#include <format>


namespace kiwi::algo{

    auto Node::remove_child(const std::Rc<Node> child) -> void{
        auto it {std::find(_post_nodes.begin(), _post_nodes.end(), child)};
        if (it == _post_nodes.end()){
            auto coord {child->track()->coord()};
            throw FinalError(std::format("Node::remove_child: child at ({}, {}, {}, {}) not found", coord.row, coord.col, coord.dir, coord.index));
        }

        _post_nodes.erase(it);
    }

    auto Node::parent() -> std::Option<std::Rc<Node>>{
        if (this->_prev_node) {
            return std::make_optional<std::Rc<Node>>(this->_prev_node);
        } else {
            return std::nullopt;  
        }
    }

    Tree::Tree(const std::Rc<Node> root) : _root(root){
        if (root->parent() != std::nullopt || root->cost() != 0 || root->post_nodes().size() != 0){
            auto parent_coord {root->parent().value()->track()->coord()};
            kiwi::debug::fatal_fmt("Tree_constructor: root node must have null parent, empty children and a 0 cost, \
                                    but now it has parent ({}, {}, {}, {}), cost {} and {} children", \
                                    parent_coord.row, parent_coord.col, parent_coord.dir, parent_coord.index, \
                                    root->cost(), root->post_nodes().size());
        }
    }

    auto Tree::is_a_predecessor(const std::Rc<Node> current_node, const std::Rc<Node> to_be_checked) -> bool{
        assert(current_node != nullptr && to_be_checked != nullptr);
        if (*current_node == *to_be_checked){
            return true;
        }   
        else if (current_node->parent() == std::nullopt){
            return false;
        }
        else {
            return this->is_a_predecessor(current_node->parent().value(), to_be_checked);
        }
    }

    // positive sequence & do not contain the start node
    auto Tree::backtrace(const std::Rc<Node> current_node) -> std::vector<std::Rc<Node>>{

        std::Vector<std::Rc<Node>> path {};
        // end node at the head of path
        auto node = current_node;
        while (node->parent() != std::nullopt){
            path.push_back(node);
            node = node->parent().value();
        }

        // reverse
        std::reverse(path.begin(), path.end());
        return path;
    }

    auto MazeRerouter::bus_reroute(
            hardware::Interposer* interposer, std::Vector<circuit::PathPackage*>& path_ptrs,
            std::usize max_length
        ) const -> std::tuple<bool, std::usize>{

        for (std::usize i = 0; i < path_ptrs.size(); ++i){
            auto path_ptr {path_ptrs.at(i)};
            hardware::Bump* end_bump {nullptr};
            std::usize bump_length = 0; // end bump

            std::HashSet<hardware::Track*> end_tracks_set {};
            std::HashMap<kiwi::hardware::Track *, kiwi::hardware::TOBConnector> possible_end_tracks_map{};

            // if there is an end bump, then the tob state will be updated after removing end track
            // --> possible end tracks will be changed
            if (!path_ptr->_track_to_tob.empty()){
                remove_tracks(path_ptr);

                end_bump = std::get<0>(path_ptr->_track_to_tob[0]);
                bump_length = 1;
                possible_end_tracks_map = interposer->available_tracks_track_to_bump(end_bump);
                for (auto& [track, _]: possible_end_tracks_map){
                    if (track && !end_tracks_set.contains(track)){
                        end_tracks_set.emplace(track);
                    }
                }
            }
            else{
                end_tracks_set.emplace(std::get<0>(path_ptr->_regular_path.back()));
                remove_tracks(path_ptr);
            }

            // if failed, then routing failed  
            if (end_tracks_set.size() <= 0){
                throw FinalError("MazeRerouter::reroute: end_tracks_set is empty");
            }

            // construct a path-tree for reroute
            auto tree {Tree(_node_track_interface.track_rootify(std::get<0>(path_ptr->_regular_path.back()), std::get<1>(path_ptr->_regular_path.back()).value()))};
            auto [success, ml] = refind_path(interposer, tree, path_ptr, max_length, end_tracks_set, bump_length);
            max_length = ml;

            // connect end bump to end track
            if (end_bump != nullptr){
                auto end_track {std::get<0>(path_ptr->_regular_path.back())};
                if (!check_found(end_tracks_set, end_track)){
                    throw FinalError("MazeRerouter::reroute: end track not found");
                }
                
                // notice: the following "for" loop cannot be replaced by "possible_end_tracks_map.find(end_track)->second.connect();"
                // there will be unexpected behaviour during "find(end_track)"
                // for there are some other members not related to coordinates in value "end_track"
                // use "if (track->coord() == end_track->coord())" is safer
                for (auto& t: possible_end_tracks_map){
                    auto& [track, connector] = t;
                    if (track->coord() == end_track->coord()){
                        path_ptr->_track_to_tob.emplace_back(
                            std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*>(end_bump, connector, end_track)
                        );
                        connector.give_out();
                        break;
                    }
                }
            }

            if (!success){
                return {false, max_length};
            }
        }
        return {true, max_length};
    }

    auto MazeRerouter::remove_tracks(
        circuit::PathPackage* path_ptr, int cut_rate
    ) const -> void{
        // check if the net is a btb/btt/ttb net, and only has two port
        auto port_num = path_ptr->_tob_to_track.size() + path_ptr->_track_to_tob.size();
        assert(port_num <= 2 && port_num >= 1);
        assert(path_ptr->_regular_path.size() == path_ptr->_length - port_num);

        //remove end TOBConnector
        if (!path_ptr->_track_to_tob.empty()){
            assert(path_ptr->_track_to_tob.size() == 1);
            std::get<1>(path_ptr->_track_to_tob[0]).stay_inside();
            path_ptr->_track_to_tob.clear();
            path_ptr->_length -= 1;
        }
        
        // notice: do not use function path_length() here
        hardware::Track* next_track = nullptr;
        auto track_number = path_ptr->_regular_path.size();
        std::usize cut_length = track_number > 1 ? ((track_number * cut_rate) < 1 ? 1 : int(track_number * cut_rate)) : 0;
        std::usize remain_length {track_number - cut_length};

        for (std::usize i = remain_length; i < track_number; ++i) {
            auto& [t, connector] = path_ptr->_regular_path[i];
            if (connector.has_value()) {
                connector.value().disconnect();
            }
        }
        path_ptr->_regular_path.resize(remain_length);
        path_ptr->_length -= cut_length;
    }

    auto MazeRerouter::Manhattan_distance(const std::Rc<Node> node, const std::HashSet<hardware::Track*>& end_tracks) const -> std::usize{
        assert(end_tracks.size() > 0);
        // check all end_tracks are below the same tob
        auto co {(*end_tracks.begin())->coord()};
        for (auto it = end_tracks.begin(); it != end_tracks.end(); ++it){
            if((*it)->coord().row != co.row || (*it)->coord().col != co.col){
                throw std::runtime_error("Manhattan_distance: the end tracks are not in the same row or column");
            }
            co = (*it)->coord();
        }

        auto begin_coord {(*end_tracks.begin())->coord()};
        auto end_coord {node->track()->coord()};
        if (begin_coord.row == end_coord.row && begin_coord.col == end_coord.col && begin_coord.dir == end_coord.dir){
            return 0;
        }
        else if (begin_coord.row == end_coord.row && begin_coord.col == end_coord.col && begin_coord.dir != end_coord.dir) {
            return 1;
        }
        else if ((begin_coord.row == end_coord.row || begin_coord.col == end_coord.col) && begin_coord.dir == end_coord.dir){
            return std::abs(end_coord.row - begin_coord.row) + std::abs(end_coord.col - begin_coord.col) + 1;
        }
        else if ((begin_coord.row == end_coord.row || begin_coord.col == end_coord.col) && begin_coord.dir != end_coord.dir){
            if (begin_coord.dir == hardware::TrackDirection::Horizontal && end_coord.dir == hardware::TrackDirection::Vertical){
                if (begin_coord.row == end_coord.row && begin_coord.col < end_coord.col){
                    return std::abs(end_coord.row - begin_coord.row) + std::abs(end_coord.col - begin_coord.col) + 1;
                }
                else if (begin_coord.row == end_coord.row && begin_coord.col > end_coord.col){
                    return std::abs(end_coord.row - begin_coord.row) + std::abs(end_coord.col - begin_coord.col);
                }
                else if (begin_coord.row < end_coord.row && begin_coord.col == end_coord.col){
                    return std::abs(end_coord.row - begin_coord.row) + std::abs(end_coord.col - begin_coord.col);
                }
                else{
                    return std::abs(end_coord.row - begin_coord.row) + std::abs(end_coord.col - begin_coord.col) + 1;
                }
            }
            else {
                if (begin_coord.row == end_coord.row && begin_coord.col < end_coord.col){
                    return std::abs(end_coord.row - begin_coord.row) + std::abs(end_coord.col - begin_coord.col);
                }
                else if (begin_coord.row == end_coord.row && begin_coord.col > end_coord.col){
                    return std::abs(end_coord.row - begin_coord.row) + std::abs(end_coord.col - begin_coord.col) + 1;
                }
                else if (begin_coord.row < end_coord.row && begin_coord.col == end_coord.col){
                    return std::abs(end_coord.row - begin_coord.row) + std::abs(end_coord.col - begin_coord.col) + 1;
                }
                else {
                    return std::abs(end_coord.row - begin_coord.row) + std::abs(end_coord.col - begin_coord.col);
                }
            }
        }
        else {
            if (begin_coord.dir == end_coord.dir){
                return std::abs(end_coord.row - begin_coord.row) + std::abs(end_coord.col - begin_coord.col);
            }
            else{
                if (begin_coord.dir == hardware::TrackDirection::Horizontal && end_coord.dir == hardware::TrackDirection::Vertical){
                    if (begin_coord.row < end_coord.row && begin_coord.col < end_coord.col){
                        return std::abs(end_coord.row - begin_coord.row) + std::abs(end_coord.col - begin_coord.col);
                    }
                    else if (begin_coord.row < end_coord.row && begin_coord.col > end_coord.col){
                        return std::abs(end_coord.row - begin_coord.row) + std::abs(end_coord.col - begin_coord.col) - 1;
                    }
                    else if (begin_coord.row > end_coord.row && begin_coord.col < end_coord.col){
                        return std::abs(end_coord.row - begin_coord.row) + std::abs(end_coord.col - begin_coord.col) + 1;
                    }
                    else {
                        return std::abs(end_coord.row - begin_coord.row) + std::abs(end_coord.col - begin_coord.col);
                    }
                }
                else {
                    if (begin_coord.row < end_coord.row && begin_coord.col < end_coord.col){
                        return std::abs(end_coord.row - begin_coord.row) + std::abs(end_coord.col - begin_coord.col);
                    }
                    else if (begin_coord.row < end_coord.row && begin_coord.col > end_coord.col){
                        return std::abs(end_coord.row - begin_coord.row) + std::abs(end_coord.col - begin_coord.col) + 1;
                    }
                    else if (begin_coord.row > end_coord.row && begin_coord.col < end_coord.col){
                        return std::abs(end_coord.row - begin_coord.row) + std::abs(end_coord.col - begin_coord.col) - 1;
                    }
                    else {
                        return std::abs(end_coord.row - begin_coord.row) + std::abs(end_coord.col - begin_coord.col);
                    }
                }
            }
        }

    }

    auto MazeRerouter::refind_path(
            hardware::Interposer* interposer, Tree& tree, circuit::PathPackage* path_ptr,\
            std::usize max_length, const std::HashSet<hardware::Track*>& end_tracks, std::usize bump_length
        ) const -> std::tuple<bool, std::usize>{
        std::Vector<std::Rc<Node>> queue {tree._root};
        std::make_heap(queue.begin(), queue.end(), Node::CompareNodes);

//!
debug::debug("Rerouting...");
// print_end_tracks(end_tracks);
//!
        // mazing with A* 
        while (!queue.empty()) {
            // get current node
            std::pop_heap(queue.begin(), queue.end(), Node::CompareNodes);
            auto node_sptr {queue.back()};
            queue.pop_back();

            // find the end node
            auto track {node_sptr->track().get()};
            if (check_found(end_tracks, track)){                
                auto temp_node_list {tree.backtrace(node_sptr)};
                auto temp_path {_node_track_interface.nodes_trackify(temp_node_list)};
                auto temp_path_length {path_length(temp_path)};
                // add to path, and set cobconnector to "suspend" state
                for (auto& tp: temp_path){
                    path_ptr->_regular_path.emplace_back(tp);
                    auto& [_, cobconnector] = tp;
                    if(cobconnector.has_value()) {
                        cobconnector.value().suspend();
                    }
                }
                path_ptr->_length += temp_path_length + bump_length;
//!
// print_path(path_ptr);
//!
                // find a longer path
                if (path_ptr->_length + temp_path_length + bump_length > max_length){
                    return {false, path_ptr->_length};
                }
                // find a equal path
                else if(path_ptr->_length + temp_path_length + bump_length == max_length){
                    return {true, max_length};
                }
                // find a shorter path
                else{
                    auto parent {node_sptr->parent()};
                    if(parent.has_value()){
                        parent.value()->remove_child(node_sptr);
                    }
                }
            }

            // expand
            for (auto& [next_track, connector] : interposer->adjacent_idle_tracks(track)) {
                auto new_cost {node_sptr->cost() + 1 + Manhattan_distance(node_sptr, end_tracks)};
                auto next_track_sptr {std::make_shared<hardware::Track>(*next_track)};
                auto next_node {std::make_shared<Node>(next_track_sptr, connector, node_sptr, new_cost)};
                if (!tree.is_a_predecessor(node_sptr, next_node)){
                    node_sptr->add_child(next_node);
                    queue.push_back(next_node);
                    std::push_heap(queue.begin(), queue.end(), Node::CompareNodes);
                }
            }
        }

        throw RetryExpt("MazeRerouter::refind_path: path not found");
        return {false, max_length};
    }

    auto MazeRerouter::check_found(const std::HashSet<hardware::Track*>& end_tracks, hardware::Track* track) const -> bool {
        for (auto& end_track: end_tracks){
            if (track->coord() == end_track->coord()){
                return true;
            }
        }
        return false;
    }

}


