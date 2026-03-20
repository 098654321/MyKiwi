#include "./occupancy_view.hh"

#include <hardware/track/track.hh>
#include <debug/debug.hh>

namespace kiwi::algo {

    OccupancyView::OccupancyView(hardware::Interposer* interposer)
        : _interposer{interposer}, _cob_occupied{}, _tob_muxreg_occupied{}, _cob_locked{}, _tob_muxreg_locked{} {
        debug::check(this->_interposer != nullptr, "OccupancyView: interposer must not be null");
    }

    auto OccupancyView::clear_mode(int mode) -> void {
        auto it_cob = this->_cob_occupied.find(mode);
        if (it_cob != this->_cob_occupied.end()) {
            std::erase_if(it_cob->second, [&](std::u64 k) { return !this->_cob_locked.contains(k); });
        }
        
        auto it_tob = this->_tob_muxreg_occupied.find(mode);
        if (it_tob != this->_tob_muxreg_occupied.end()) {
            std::erase_if(it_tob->second, [&](std::u64 k) { return !this->_tob_muxreg_locked.contains(k); });
        }   
    }

    auto OccupancyView::cob_key(const hardware::COBConnector& c) const -> std::u64 {
        // Pack into 64-bit: [coord(row:8)(col:8)][from_dir:4][to_dir:4][from_idx:10][to_idx:10][pad]
        const auto coord = c.coord();
        const auto row = static_cast<std::u64>(coord.row & 0xff);
        const auto col = static_cast<std::u64>(coord.col & 0xff);
        const auto from_dir = static_cast<std::u64>(static_cast<int>(c.from_dir()) & 0x0f);
        const auto to_dir = static_cast<std::u64>(static_cast<int>(c.to_dir()) & 0x0f);
        const auto from_idx = static_cast<std::u64>(c.from_track_index() & 0x3ff);
        const auto to_idx = static_cast<std::u64>(c.to_track_index() & 0x3ff);

        return (row << 56) | (col << 48) | (from_dir << 44) | (to_dir << 40) | (from_idx << 30) | (to_idx << 20);
    }

    auto OccupancyView::tob_key(const hardware::TOBConnector& c) const -> std::HashSet<std::u64> {
        // Use TOBMuxRegister pointer identities as conflict keys.
        auto regs = c.check_mux_pregister();
        auto keys = std::HashSet<std::u64>{};
        keys.reserve(regs.size());
        for (auto* r : regs) {
            keys.emplace(static_cast<std::u64>(reinterpret_cast<std::uintptr_t>(r)));
        }
        return keys;
    }

    auto OccupancyView::is_cobconnector_occupied(int mode, const hardware::COBConnector& c) const -> bool {
        auto it = this->_cob_occupied.find(mode);
        if (it == this->_cob_occupied.end()) {
            return false;
        }
        return it->second.contains(this->cob_key(c));
    }

    auto OccupancyView::occupy_cobconnector(int mode, const hardware::COBConnector& c, bool locked) -> void {
        auto& set = this->_cob_occupied[mode];
        const auto key = this->cob_key(c);
        set.emplace(key);
        if (locked) {
            this->_cob_locked.emplace(key);
        }
    }

    auto OccupancyView::is_tobconnector_occupied(int mode, const hardware::TOBConnector& c) const -> bool {
        auto it = this->_tob_muxreg_occupied.find(mode);
        if (it == this->_tob_muxreg_occupied.end()) {
            return false;
        }
        const auto keys = this->tob_key(c);
        for (auto k : keys) {
            if (it->second.contains(k)) {
                return true;
            }
        }
        return false;
    }

    auto OccupancyView::occupy_tobconnector(int mode, const hardware::TOBConnector& c, bool locked) -> void {
        auto& set = this->_tob_muxreg_occupied[mode];
        const auto keys = this->tob_key(c);
        for (auto k : keys) {
            set.emplace(k);
        }
        if (locked) {
            this->_tob_muxreg_locked.insert(keys.begin(), keys.end());
        }
    }

    auto OccupancyView::is_idle_track(int mode, hardware::Track* track) const -> bool {
        for (auto [t, connector] : this->_interposer->adjacent_tracks(track)) {
            (void)t;
            if (this->is_cobconnector_occupied(mode, connector)) {
                return false;
            }
        }
        return true;
    }

    auto OccupancyView::adjacent_idle_tracks(int mode, hardware::Track* track) const -> std::Vector<std::Tuple<hardware::Track*, hardware::COBConnector>> {
        auto result = std::Vector<std::Tuple<hardware::Track*, hardware::COBConnector>>{};
        for (auto [t, connector] : this->_interposer->adjacent_tracks(track)) {
            if (!this->is_cobconnector_occupied(mode, connector) && this->is_idle_track(mode, t)) {
                result.emplace_back(t, connector);
            }
        }
        return result;
    }

}

