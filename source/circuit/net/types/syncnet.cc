#include "./syncnet.hh"

#include <hardware/coord.hh>
#include <assert.h>
#include <algorithm>



namespace kiwi::circuit
{

    SyncNet::SyncNet(
        std::Vector<std::Box<BumpToBumpNet>> btbnets,
        std::Vector<std::Box<BumpToTrackNet>> bttnets,
        std::Vector<std::Box<TrackToBumpNet>> ttbnets
    ) : 
        _btbnets{std::move(btbnets)},
        _bttnets{std::move(bttnets)},
        _ttbnets{std::move(ttbnets)},
        Net{Priority{0}}
    {
    }

    auto SyncNet::update_tob_postion(hardware::TOB* prev_tob, hardware::TOB* next_tob) -> void {
        for (auto& net : this->_btbnets) {
            net->update_tob_postion(prev_tob, next_tob);
        }
        
        for (auto& net : this->_bttnets) {
            net->update_tob_postion(prev_tob, next_tob);
        }

        for (auto& net : this->_ttbnets) {
            net->update_tob_postion(prev_tob, next_tob);
        }
    }

    auto SyncNet::route(hardware::Interposer* interposer, const algo::RouteStrategy& strategy) -> void {
        strategy.route_sync_net(interposer, this);
    }

    auto SyncNet::update_priority(float bias) -> void {
        assert(0 <= bias && bias < 1);
        for (auto& net : this->_btbnets) {
            net->update_priority(bias);
        }

        for (auto& net : this->_bttnets) {
            net->update_priority(bias);
        }

        for (auto& net : this->_ttbnets) {
            net->update_priority(bias);
        }
    }

    auto SyncNet::coords() const -> std::Vector<hardware::Coord> {
        auto coords = std::Vector<hardware::Coord>{};
        for (auto& net : this->_btbnets) {
            for (auto& coord : net->coords()) {
                coords.emplace_back(coord);
            }
        }

        for (auto& net : this->_bttnets) {
            for (auto& coord : net->coords()) {
                coords.emplace_back(coord);
            }
        }

        for (auto& net : this->_ttbnets) {
            for (auto& coord : net->coords()) {
                coords.emplace_back(coord);
            }
        }

        return coords;
    }

    auto SyncNet::check_accessable_cobunit() -> void {
        for (auto& net: _btbnets) {
            net->check_accessable_cobunit();
        }
        for (auto& net: _bttnets) {
            net->check_accessable_cobunit();
        }
        for (auto& net: _ttbnets) {
            net->check_accessable_cobunit();
        }
    }

    
    auto SyncNet::to_string() const -> std::String {
        auto ss = std::StringStream {};
        ss << "Syncnet net:\n";
        for (auto& net: _btbnets) {
            ss << "    " << net->to_string() << '\n';
        }
        for (auto& net: _bttnets) {
            ss << "    " << net->to_string() << '\n';
        }
        for (auto& net: _ttbnets) {
            ss << "    " << net->to_string() << '\n';
        }
        ss << "End syncnet net";
        return ss.str();
    }

    auto SyncNet::port_number() const -> std::usize {
        std::usize port_number = 0;
        for (auto& net: _btbnets) {
            port_number += net->port_number();
        }
        for (auto& net: _bttnets) {
            port_number += net->port_number();
        }
        for (auto& net: _ttbnets) {
            port_number += net->port_number();
        }
        return port_number;
    }

    auto SyncNet::set_pathpackage(const circuit::PathPackage& package) -> void {
        if (!package._tob_to_track.empty() && package._track_to_tob.empty()) {
            assert(package._tob_to_track.size() == 1);
            auto bump = std::get<0>(package._tob_to_track[0]);
            for (auto& net: _bttnets) {
                if (net->begin_bump()->coord() == bump->coord())
                {
                    net->set_pathpackage(package);
                }
            }
        }
        else if (package._tob_to_track.empty() && !package._track_to_tob.empty()) {
            assert(package._track_to_tob.size() == 1);
            auto bump = std::get<0>(package._track_to_tob[0]);
            for (auto& net: _ttbnets) {
                if (net->end_bump()->coord() == bump->coord())
                {
                    net->set_pathpackage(package);
                }
            }
        }
        else if (!package._tob_to_track.empty() && !package._track_to_tob.empty()) {
            assert(package._track_to_tob.size() == 1 && package._tob_to_track.size() == 1);
            auto bump_begin = std::get<0>(package._tob_to_track[0]);
            auto bump_end = std::get<0>(package._track_to_tob[0]);
            for (auto& net: _btbnets) {
                if (net->begin_bump()->coord() == bump_begin->coord() && net->end_bump()->coord() == bump_end->coord()){
                    net->set_pathpackage(package);
                }
            }
        }
    }

    auto SyncNet::show() const -> void {
        for (auto& net: this->_btbnets) {
            net->show();
        }
        for (auto& net: this->_bttnets) {
            net->show();
        }
        for (auto& net: this->_ttbnets) {
            net->show();
        }
    }

    auto SyncNet::length() const -> std::usize {
        auto total_length = std::usize{0};
        for (auto& net: this->_btbnets) {
            total_length += net->length();
        }
        for (auto& net: this->_bttnets) {
            total_length += net->length();
        }
        for (auto& net: this->_ttbnets) {
            total_length += net->length();
        }
        return total_length;
    }

    auto SyncNet::check_relativity(const hardware::Bump* node) const -> const Net* {
        for (auto& net: this->_btbnets) {
            if (net->check_relativity(node) != nullptr) {
                return net.get();
            }
        }
        for (auto& net: this->_bttnets) {
            if (net->check_relativity(node) != nullptr) {
                return net.get();
            }
        }
        for (auto& net: this->_ttbnets) {
            if (net->check_relativity(node) != nullptr) {
                return net.get();
            }
        }
        return nullptr;
    }

    auto SyncNet::check_relativity(const hardware::Track* node) const -> const Net* {
        for (auto& net: this->_bttnets) {
            if (net->check_relativity(node) != nullptr) {
                return net.get();
            }
        }
        for (auto& net: this->_ttbnets) {
            if (net->check_relativity(node) != nullptr) {
                return net.get();
            }
        }
        return nullptr;
    }

    auto SyncNet::search_related_nets(std::Vector<Net*>& nets) -> void {
        clear_related_nets();
        //* Unsupported operation for syncnet currently
    }

 }


