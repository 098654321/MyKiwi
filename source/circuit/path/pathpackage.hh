#pragma once

#include <hardware/track/track.hh>
#include <hardware/bump/bump.hh>
#include <hardware/cob/cobconnector.hh>
#include <hardware/tob/tobconnector.hh>
#include <global/std/collection.hh>
#include <global/std/utility.hh>
#include <global/debug/debug.hh>
#include <algorithm>


namespace kiwi::hardware {

    class COBConnector;
    class TOBConnector;
    class Track;
    class Bump;

}


namespace kiwi::circuit {

// store the path temporarily with positive track sequence 
//! connector stored in package -> set state as "suspended". 
//! if connected, set state as "connected" and "given out". 
//! if removed from package, set state as "disconnected" and "stay inside"
struct PathPackage {
    PathPackage(): _regular_path{}, _tob_to_track{}, _track_to_tob{}, _length{0} {}

    auto show() const -> void {
        debug::debug("\nPrinting path...");

        if (this->_tob_to_track.size() > 0) {
            for (auto& [bump, tobconnector, track]: this->_tob_to_track) {
                debug::debug_fmt("Begin_bump: ({}, index={})", bump->coord(), bump->index());
            }
        }

        for (auto& [track, cob_connector]: this->_regular_path) {
            debug::debug_fmt("{}", track->coord());
        }

        if (this->_track_to_tob.size() > 0) {
            for (auto& [bump, tobconnector, track]: this->_track_to_tob) {
                debug::debug_fmt("End_bump: ({}, index={})", bump->coord(), bump->index());
            }
        }
        debug::debug("\n");
    }

    auto find_bump(const hardware::Bump* bump) const -> std::Option<std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*>> {
        for (auto& t: this->_tob_to_track) {
            if (bump->coord() == std::get<0>(t)->coord()) {
                return t;
            }
        }
        for (auto& t: this->_track_to_tob) {
            if (bump->coord() == std::get<0>(t)->coord()) {
                return t;
            }
        }
        return std::nullopt;
    }

    auto find_track(const hardware::Track* track) const -> std::Option<std::Tuple<hardware::Track*, std::Option<hardware::COBConnector>>> {
        for (auto& p: this->_regular_path) {
            if (track->coord() == std::get<0>(p)->coord()) {
                return p;
            }
        }
        return std::nullopt;
    }

    // track & the COBConnector before track
    std::Vector<std::Tuple<hardware::Track*, std::Option<hardware::COBConnector>>> _regular_path;
    std::Vector<std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*>>_tob_to_track;
    std::Vector<std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*>> _track_to_tob;
    std::usize _length;
};

}
