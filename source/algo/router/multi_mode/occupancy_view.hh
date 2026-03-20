#pragma once

#include <hardware/interposer.hh>
#include <hardware/cob/cobconnector.hh>
#include <hardware/tob/tobconnector.hh>
#include <std/collection.hh>
#include <std/integer.hh>
#include <std/utility.hh>

namespace kiwi::algo::multi_mode {

    class OccupancyView {
    public:
        explicit OccupancyView(hardware::Interposer* interposer);

        auto clear_mode(int mode) -> void;

        auto is_cobconnector_occupied(int mode, const hardware::COBConnector& c) const -> bool;
        auto occupy_cobconnector(int mode, const hardware::COBConnector& c, bool locked = false) -> void;

        auto occupy_tobconnector(int mode, const hardware::TOBConnector& c, bool locked = false) -> void;
        auto is_tobconnector_occupied(int mode, const hardware::TOBConnector& c) const -> bool;

        auto is_idle_track(int mode, hardware::Track* track) const -> bool;
        auto adjacent_idle_tracks(int mode, hardware::Track* track) const -> std::Vector<std::Tuple<hardware::Track*, hardware::COBConnector>>;

    private:
        auto cob_key(const hardware::COBConnector& c) const -> std::u64;
        auto tob_key(const hardware::TOBConnector& c) const -> std::HashSet<std::u64>;

    private:
        hardware::Interposer* _interposer;

        // Per-mode occupied COB connectors (hash key by connector identity)
        std::HashMap<int, std::HashSet<std::u64>> _cob_occupied;
        // Per-mode occupied TOB mux registers (hash key by TOBMuxRegister address)
        std::HashMap<int, std::HashSet<std::u64>> _tob_muxreg_occupied;

        // COB/TOB connectors that are "locked" (e.g. shared nets) and cannot be cleared.
        std::HashSet<std::u64> _cob_locked;
        std::HashSet<std::u64> _tob_muxreg_locked;
    };

}

