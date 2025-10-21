#include "pathpackage.hh"
#include <format>


namespace kiwi::circuit {


    PathPackage::PathPackage()
        : _regular_path{}, _tob_to_track{}, _track_to_tob{}, _length{0} {}

    PathPackage::PathPackage(const HistoryPathPackage& history_pathpackage, hardware::Interposer* pinterposer) {
        this->_length = history_pathpackage._length;

// for debug
auto preg_set = std::unordered_set<const hardware::TOBMuxRegister*> {};
//

        // create regular path
        std::Vector<std::Tuple<hardware::Track*, std::Option<hardware::COBConnector>>> regular_path {};
        for (const auto& [trackcoord, cobconnectorinfo]: history_pathpackage._regular_path) {
            auto track = pinterposer->get_track(trackcoord);
            if (!track.has_value()) {
                throw std::runtime_error("create Pathpackage by History: Track not found in interposer");
            }

            if (!cobconnectorinfo.has_value()) {
                regular_path.emplace_back(track.value(), std::nullopt);
            }
            else {
                auto cob_connector = cobconnectorinfo.value().create_cobconnector(pinterposer);
                regular_path.emplace_back(track.value(), cob_connector);
            }
        }
        this->_regular_path = regular_path;

        // create tobconnector tob to track
        std::Vector<std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*>> tob_to_track {};
        for (const auto& [bumpcoord, tobconnectorinfo, trackcoord]: history_pathpackage._tob_to_track) {
            auto track = pinterposer->get_track(trackcoord);
            if (!track.has_value()) {
                throw std::runtime_error(std::format("create Pathpackage by History: Track not found in interposer, trackcoord: ({}, {}, {}, {})", trackcoord.row, trackcoord.col, trackcoord.dir, trackcoord.index));
            }


            auto bump = pinterposer->get_bump(bumpcoord.row / 2, bumpcoord.col / 3, bumpcoord.index);
            if (!bump.has_value()) {
                throw std::runtime_error(std::format("create Pathpackage by History: Bump not found in interposer, bumpcoord: ({}, {}, {})", bumpcoord.row, bumpcoord.col, bumpcoord.index));
            }

            auto tob_connector = tobconnectorinfo.create_tobconnector(pinterposer);
            tob_to_track.emplace_back(bump.value(), tob_connector, track.value());

// for debug
auto pregs = tob_connector.check_mux_pregister();
for (auto& preg: pregs) {
    if (preg_set.contains(preg)) {
        debug::exception("create Pathpackage by History: TOBMuxRegister in TOBConnector is not unique");
    }
    else {
        preg_set.emplace(preg);
    }
}
//
        }
        this->_tob_to_track = tob_to_track;

        // create tobconnector track to tob
        std::Vector<std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*>> track_to_tob {};
        for (const auto& [bumpcoord, tobconnectorinfo, trackcoord]: history_pathpackage._track_to_tob) {
            auto track = pinterposer->get_track(trackcoord);
            if (!track.has_value()) {
                throw std::runtime_error(std::format("create Pathpackage by History: Track not found in interposer, trackcoord: ({}, {}, {}, {})", trackcoord.row, trackcoord.col, trackcoord.dir, trackcoord.index));
            }

            auto bump = pinterposer->get_bump(bumpcoord.row / 2, bumpcoord.col / 3, bumpcoord.index);
            if (!bump.has_value()) {
                throw std::runtime_error(std::format("create Pathpackage by History: Bump not found in interposer, bumpcoord: ({}, {}, {})", bumpcoord.row, bumpcoord.col, bumpcoord.index));
            }

            auto tob_connector = tobconnectorinfo.create_tobconnector(pinterposer);
            track_to_tob.emplace_back(bump.value(), tob_connector, track.value());

// for debug
auto pregs = tob_connector.check_mux_pregister();
for (auto& preg: pregs) {
    if (preg_set.contains(preg)) {
        debug::exception("create Pathpackage by History: TOBMuxRegister in TOBConnector is not unique");
    }
    else {
        preg_set.emplace(preg);
    }
}
//
        }
        this->_track_to_tob = track_to_tob;
    }

    auto PathPackage::show() const -> void {
        debug::info("\nPrinting path...");

        if (this->_tob_to_track.size() > 0) {
            for (auto& [bump, tobconnector, track]: this->_tob_to_track) {
                debug::info_fmt("Begin_bump: ({}, index={})", bump->coord(), bump->index());
            }
        }

        for (auto& [track, cob_connector]: this->_regular_path) {
            debug::info_fmt("{}", track->coord());
        }

        if (this->_track_to_tob.size() > 0) {
            for (auto& [bump, tobconnector, track]: this->_track_to_tob) {
                debug::info_fmt("End_bump: ({}, index={})", bump->coord(), bump->index());
            }
        }
        debug::info("\n");
    }

    auto PathPackage::find_bump(const hardware::Bump* bump) const -> std::Option<std::Tuple<hardware::Bump*, hardware::TOBConnector, hardware::Track*>> {
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

    auto PathPackage::find_track(const hardware::Track* track) const -> std::Option<std::Tuple<hardware::Track*, std::Option<hardware::COBConnector>>> {
        for (auto& p: this->_regular_path) {
            if (track->coord() == std::get<0>(p)->coord()) {
                return p;
            }
        }
        return std::nullopt;
    }

    auto PathPackage::reset_all() -> void {
        this->_length = 0;
        for (auto& [t, cob_connector]: this->_regular_path) {
            if (cob_connector.has_value()) {
                (*cob_connector).disconnect();
            }
        }
        for (auto& [b, tob_connector, t]: this->_tob_to_track) {
            tob_connector.disconnect();
        }
        for (auto& [b, tob_connector, t]: this->_track_to_tob) {
            tob_connector.disconnect();
        }
    }

    auto PathPackage::clear_all() -> void {
        this->reset_all();

        this->_regular_path.clear();
        this->_tob_to_track.clear();
        this->_track_to_tob.clear();
    }

    auto  PathPackage::occupy_all() -> void {
        for (auto& [t, cob_connector]: this->_regular_path) {
            if (cob_connector.has_value()) {
                (*cob_connector).suspend();
            }
        }
        for (auto& [b, tob_connector, t]: this->_tob_to_track) {
            tob_connector.give_out();
        }
        for (auto& [b, tob_connector, t]: this->_track_to_tob) {
            tob_connector.give_out();
        }
    }

    auto PathPackage::connect_all() -> void {
        // connect the path
        hardware::Track* prev_track = nullptr;
        for (auto iter = this->_regular_path.begin(); iter != this->_regular_path.end(); ++iter) {
            auto& [track, connector] = *iter;
            if (connector.has_value()) {
                connector->connect();
            }
            if (prev_track != nullptr) {
                track->set_connected_track(prev_track);
            }
            prev_track = track;
        }
        
        // connect the head bump
        for (auto& [bump, tob_connector, h_track] : this->_tob_to_track) {
            bump->set_connected_track(h_track, hardware::TOBSignalDirection::BumpToTrack);
            tob_connector.connect();
        }

        // connect the tail bump
        for (auto& [bump, tob_connector, t_track] : this->_track_to_tob) {
            bump->set_connected_track(t_track, hardware::TOBSignalDirection::TrackToBump);
            tob_connector.connect();
        }
    }

    auto PathPackage::check_tobconenctor_consistency() const -> void {
        std::String except_mess {};

        auto check = [&](const hardware::TOBConnector& connector) {
        try{
            connector.check_consistency();
        }
        catch (const std::exception& e) {
            std::String mess{std::format("({}, {}, {}, {}): ", connector.bump_index(), connector.hori_index(), connector.vert_index(), connector.track_index())};
            except_mess += mess + std::string(e.what()) + "\n";
        }
        };
    
        for (const auto& [bump, tob_connector, track]: this->_tob_to_track) {
            check(tob_connector);
        }
        for (const auto& [bump, tob_connector, track]: this->_track_to_tob) {
            check(tob_connector);
        }

        if (except_mess.size() > 0) {
            throw std::runtime_error(except_mess);
        }
    }


    HistoryPathPackage::HistoryPathPackage(const PathPackage& path_package) {
        this->_length = path_package._length;

        std::Vector<std::Tuple<hardware::TrackCoord, std::Option<COBConnectorInfo>>> regular_path {};
        for (const auto& [ptrack, cobconnector]: path_package._regular_path) {
            if (cobconnector.has_value()) {
                regular_path.emplace_back(ptrack->coord(), std::make_optional(COBConnectorInfo{
                    cobconnector->coord(),
                    cobconnector->from_dir(),
                    cobconnector->from_track_index(),
                    cobconnector->to_dir(),
                    cobconnector->to_track_index(),
                }));
            }
            else {
                regular_path.emplace_back(ptrack->coord(), std::nullopt);
            }
        }
        this->_regular_path = regular_path;

        std::Vector<std::Tuple<hardware::BumpCoord, TOBConnectorInfo, hardware::TrackCoord>> tob_to_track {};
        for (const auto& [pbump, tobconnector, ptrack]: path_package._tob_to_track) {
            tob_to_track.emplace_back(
                pbump->coord(), 
                TOBConnectorInfo{
                    tobconnector.bump_index(),
                    tobconnector.hori_index(),
                    tobconnector.vert_index(),
                    tobconnector.track_index(),
                    tobconnector.single_direction(),
                    pbump->tob()->coord(),
                },
                ptrack->coord()
            );
        }
        this->_tob_to_track = tob_to_track;

        std::Vector<std::Tuple<hardware::BumpCoord, TOBConnectorInfo, hardware::TrackCoord>> track_to_tob {};
        for (const auto& [pbump, tobconnector, ptrack]: path_package._track_to_tob) {
            track_to_tob.emplace_back(
                pbump->coord(), 
                TOBConnectorInfo{
                    tobconnector.bump_index(),
                    tobconnector.hori_index(),
                    tobconnector.vert_index(),
                    tobconnector.track_index(),
                    tobconnector.single_direction(),
                    pbump->tob()->coord(),
                },
                ptrack->coord()
            );
        }
        this->_track_to_tob = track_to_tob;
    }

    auto HistoryPathPackage::clear_all() -> void  {
        this->_regular_path.clear();
        this->_tob_to_track.clear();
        this->_track_to_tob.clear();
        this->_length = 0;
    }


    auto COBConnectorInfo::create_cobconnector(hardware::Interposer* pinterposer) const -> hardware::COBConnector {
        auto cob = pinterposer->get_cob(this->cob_coord);
        if (!cob.has_value()) {
            throw std::runtime_error("create Pathpackage by History: COB not found in interposer");
        }
        auto cob_connector = cob.value()->get_cob_connector(from_dir, from_track_index, to_dir, to_track_index, cob_coord);
        return cob_connector;
    }

    auto TOBConnectorInfo::create_tobconnector(hardware::Interposer* pinterposer) const -> hardware::TOBConnector {
        auto tob = pinterposer->get_tob(this->tob_coord);
        if (!tob.has_value()) {
            throw std::runtime_error("create Pathpackage by History: TOB not found in interposer");
        }
        const auto ptob = tob.value();
        
        auto bump_to_hori_info = ptob->bump_to_hori_mux_info(bump_index); 
        auto bump_output_index = this->hori_index % hardware::TOB::BUMP_TO_HORI_MUX_SIZE;
        auto bump_to_hori_mux = ptob->bump_to_hori_muxs(std::get<0>(bump_to_hori_info))->connector(std::get<1>(bump_to_hori_info), bump_output_index, false);

        auto hori_to_vert_info = ptob->hori_to_vert_mux_info(hori_index);
        auto vert_output_index = this->vert_index % hardware::TOB::HORI_TO_VERI_MUX_SIZE;
        auto hori_to_vert_mux = ptob->hori_to_vert_muxs(std::get<0>(hori_to_vert_info))->connector(std::get<1>(hori_to_vert_info), vert_output_index, false);

        auto vert_to_track_info = ptob->vert_to_track_mux_info(vert_index);
        auto track_output_index = this->track_index / (std::size_t)hardware::TOB::VERI_TO_TRACK_MUX_COUNT;
        auto vert_to_track_mux = ptob->vert_to_track_muxs(std::get<0>(vert_to_track_info))->connector(std::get<1>(vert_to_track_info), track_output_index, false);
        
        auto bump_dir_reg = ptob->bump_dir_register(bump_index);
        auto track_dir_reg = ptob->track_dir_register(track_index);

        auto tobconnector = hardware::TOBConnector(
            bump_index,
            hori_index,
            vert_index,
            track_index,
            bump_to_hori_mux,
            hori_to_vert_mux,
            vert_to_track_mux,
            bump_dir_reg,
            track_dir_reg,
            signal_dir
        );
        return tobconnector;
    }

}

