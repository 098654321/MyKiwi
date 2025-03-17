#pragma once

#include <hardware/coord.hh>
#include <hardware/tob/tobregister.hh>
#include <std/collection.hh>
#include <std/string.hh>
#include <std/utility.hh>
#include <global/debug/debug.hh>
#include <circuit/path/pathpackage.hh>
#include <variant>
#include <cassert>
#include <type_traits>



namespace kiwi::hardware {
    class TOB;
    class Interposer;
}

namespace kiwi::algo {
    class RouteStrategy;
}

namespace kiwi::circuit {

    class Priority {
    public:
        Priority(float value): _value{value} {}
        auto operator > (const Priority& other) const -> bool {
            return this->_value < other._value;
        }
        auto value() const -> float {
            return this->_value;
        }

    private:
        float _value;
    };

    class Net {
    public:
        virtual auto update_tob_postion(hardware::TOB* prev_tob, hardware::TOB* next_tob) -> void = 0;
        virtual auto route(hardware::Interposer* interposer, const algo::RouteStrategy& strategy) -> void = 0;
        virtual auto update_priority(float bias) -> void = 0;
        virtual auto coords() const -> std::Vector<hardware::Coord> = 0;
        virtual auto check_accessable_cobunit() -> void = 0;
        virtual auto accessable_cobunit() -> std::HashMap<hardware::Bump*, std::HashSet<std::usize>> = 0;
        virtual auto port_number() const -> std::usize = 0;
        virtual auto to_string() const -> std::String = 0;
        virtual auto search_related_nets(std::Vector<Net*>& nets) -> void = 0;
        virtual auto nodes_map() -> std::HashMap<hardware::Bump*, std::HashSet<hardware::Bump*>> = 0;
        virtual auto nodes_direction() -> std::HashMap<hardware::Bump*, hardware::TOBBumpDirection> = 0;

        // return: (bumps_routable, bumps_unroutable, tracks_unroutable)
        virtual auto connection_state() const -> std::Tuple<std::Vector<const hardware::Bump*>, std::Vector<const hardware::Bump*>, std::Vector<const hardware::Track*>> = 0;
        
    public:
        virtual auto clear_related_nets() -> void;
        virtual auto show_path() const -> void;
        virtual auto length() const -> std::usize;
        virtual auto set_pathpackage(const PathPackage& path_package) -> void;
        virtual auto pathpackage() -> PathPackage& {return this->_path_package;}
        virtual auto pathpackage() const -> const PathPackage& {return this->_path_package;}
        virtual auto priority() const -> const Priority& {return this->_priority;}
        virtual auto check_relativity(const hardware::Bump* node) const -> const Net* {return nullptr;}
        virtual auto check_relativity(const hardware::Track* node) const -> const Net* {return nullptr;}
    
    public:
        template <class Node>
        auto related_nets(Node* node) const -> const std::Vector<Net*>& {
            static_assert(
                std::is_same<Node, hardware::Bump>::value || std::is_same<Node, hardware::Track>::value,\
                "Node must be hardware::Bump or hardware::Track"
            );
            
            if constexpr (std::is_same<Node, hardware::Bump>::value) {
                return this->_related_nets_bump.at(node);
            }
            else {
                return this->_related_nets_track.at(node);
            }
        }
    
    protected:
        // search nets which share common nodes with this 
        template <class Node>
        auto search_nets_node(Node* node, std::Vector<Net*>& nets) -> std::Vector<Net*> {
            std::Vector<Net*> related_nets {};
            for (auto& net : nets){
                if (net->check_relativity(node) != nullptr){
                    related_nets.emplace_back(net);
                }
            }
            return related_nets;
        }
    
    public:
        Net(Priority priority): _priority{priority}, _path_package{}, _related_nets_track{}, _related_nets_bump{} {}
        virtual ~Net() noexcept {}
    
    protected:
        Priority _priority;
        PathPackage _path_package;

        std::HashMap<hardware::Track*, std::Vector<Net*>> _related_nets_track;
        std::HashMap<hardware::Bump*, std::Vector<Net*>> _related_nets_bump;
    };

}