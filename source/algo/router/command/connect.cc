#include "./connect.hh"
#include <global/debug/debug.hh>


namespace kiwi::algo {

auto Connect::execute(hardware::Interposer* interposer, RouteEngine& engine, const RouteStrategy& strategy) const -> void {
    debug::debug("connecting paths ...");
    auto& nets = const_cast<std::Vector<circuit::Net*>&>(engine.nets());
    for (auto& net : nets) {
        auto& path_package = net->pathpackage();
        // connect the path
        hardware::Track* prev_track = nullptr;
        for (auto iter = path_package._regular_path.begin(); iter != path_package._regular_path.end(); ++iter) {
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
        for (auto& [bump, tob_connector, h_track] : path_package._tob_to_track) {
            bump->set_connected_track(h_track, hardware::TOBSignalDirection::BumpToTrack);
            tob_connector.connect();
        }

        // connect the tail bump
        for (auto& [bump, tob_connector, t_track] : path_package._track_to_tob) {
            bump->set_connected_track(t_track, hardware::TOBSignalDirection::TrackToBump);
            tob_connector.connect();
        }
    }
}

auto Connect::to_string() const -> const std::String {
    return "Connect";
}

}

