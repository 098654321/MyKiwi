#pragma once

#include "./connection/connection.hh"
#include "./topdie/topdie.hh"
#include "./topdieinst/topdieinst.hh"
#include "./export/export.hh"
#include <hardware/track/trackcoord.hh>

#include <std/utility.hh>
#include <std/memory.hh>
#include <std/integer.hh>
#include <std/collection.hh>
#include <std/string.hh>

namespace PR_tool::circuit {

    class BaseDie {
    public:
        BaseDie() = default;
        ~BaseDie() noexcept;

    public:
        void clear();

    public:
        auto add_topdie(std::String name, std::HashMap<std::String, std::usize> pin_map) -> TopDie*;
        auto add_topdie_inst(std::String name, TopDie* topdie, hardware::TOB* tob) -> TopDieInstance*;
        auto add_topdie_inst(TopDie* topdie, hardware::TOB* tob) -> TopDieInstance*;
        auto add_external_port(std::String name, const hardware::TrackCoord& coord) -> ExternalPort*;
        auto add_external_port(const hardware::TrackCoord& coord) -> ExternalPort*;
        auto add_connection(int mode, int sync, Pin input, Pin output) -> Connection*;
        auto add_pose_port(const std::Vector<hardware::TrackCoord>& ports) -> void;
        auto add_nege_port(const std::Vector<hardware::TrackCoord>& ports) -> void;

        auto add_net(const std::Rc<Net>&, int) -> void;

        auto reserve_connections(int mode, int sync, int size) -> void;

    public:
        auto remove_topdie_inst(std::StringView name) -> bool;
        auto remove_topdie_inst(TopDieInstance* inst) -> bool;

        auto remove_external_port(std::StringView name) -> bool;
        auto remove_external_port(ExternalPort* eport) -> bool;

        auto remove_connection(Connection*) -> bool;

    public:
        void topdie_inst_rename(TopDieInstance* inst, std::String new_name);
        void topdie_inst_rename(std::StringView old_name, std::String new_name);

        void external_port_rename(ExternalPort* eport, std::String new_name);
        void external_port_rename(std::StringView old_name, std::String new_name);
        void external_port_set_coord(ExternalPort* eport, const hardware::TrackCoord& coord);
        void external_port_set_coord(std::StringView old_name, const hardware::TrackCoord& coord);

        void connection_set_input(Connection* connection, Pin input);
        void connection_set_output(Connection* connection, Pin output);
        void connection_set_sync(Connection* connection, int sync);
    
    public:
        auto merge_same_mode_nets() -> void;

    public:
        /*
            Get object form base die, if name not exit, return nullopt
        */
        auto get_topdie(std::StringView name) -> std::Option<TopDie*>;
        auto get_topdie_inst(std::StringView name) -> std::Option<TopDieInstance*>;
        auto get_external_port(std::StringView name) -> std::Option<ExternalPort*>;

    private:
        auto default_topdie_inst_name(TopDie* topdie) -> std::String;
        auto default_external_port_name() -> std::String;

    public:
        auto topdies() const -> const std::HashMap<std::StringView, std::Box<TopDie>>& 
        { return this->_topdies; }
        
        auto topdie_insts() const -> const std::HashMap<std::StringView, std::Box<TopDieInstance>>& 
        { return this->_topdie_insts; }
        
        auto external_ports() const -> const std::HashMap<std::StringView, std::Box<ExternalPort>>&
        { return this->_external_ports; }

        auto connections() const -> const std::HashMap<int, std::HashMap<int, std::Vector<std::Box<Connection>>>>& 
        { return this->_connections; }

        auto pose_ports() const -> const std::Vector<hardware::TrackCoord>&
        { return this-> _pose_ports; }

        auto nege_ports() const -> const std::Vector<hardware::TrackCoord>&
        { return this-> _nege_ports; }

        auto nets() -> std::HashMap<int, std::Vector<std::Rc<Net>>>& 
        { return this->_nets; }

        auto nets(int m) -> std::Vector<std::Rc<Net>>& {
            auto it = this->_nets.find(m);
            if (it == this->_nets.end()) {
                debug::fatal_fmt("Cannot find nets with mode {}", m);
            }
            else {
                return it->second;
            }
        }

        auto nets() const -> const std::HashMap<int, std::Vector<std::Rc<Net>>>& 
        { return this->_nets; }

        auto nets(int m) const -> std::Span<const std::Rc<Net>>{
            const auto it = this->_nets.find(m);
            if (it == this->_nets.end()) {
                debug::fatal_fmt("Cannot find nets with mode {}", m);
            }
            else {
                return it->second;
            }
        }

        auto nets_to_vector() -> std::Vector<std::Rc<Net>> {
            std::Set<std::Rc<Net>> nets {};
            for (auto& [m, ns]: this->_nets) {
                nets.insert(ns.begin(), ns.end());
            }
            return std::Vector<std::Rc<Net>>(nets.begin(), nets.end());
        }

    private:
        std::HashMap<std::StringView, std::Box<TopDie>> _topdies {};
        std::HashMap<std::StringView, std::Box<TopDieInstance>> _topdie_insts {};
        std::HashMap<std::StringView, std::Box<ExternalPort>> _external_ports {};
        std::HashMap<int, std::HashMap<int, std::Vector<std::Box<Connection>>>> _connections {};
        std::Vector<hardware::TrackCoord> _pose_ports {};
        std::Vector<hardware::TrackCoord> _nege_ports {};

        std::HashMap<int, std::Vector<std::Rc<Net>>> _nets {};
    };

}