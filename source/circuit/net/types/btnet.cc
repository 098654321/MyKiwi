#include "./btnet.hh"
#include <hardware/bump/bump.hh>
#include <hardware/track/track.hh>
#include <algo/router/single_mode/incremental/maze/routing.hh>


namespace kiwi::circuit {

    BumpToTrackNet::BumpToTrackNet(hardware::Bump* begin_bump, hardware::Track* end_track, const std::HashSet<int>& modes, std::String& name) :
        _begin_bump{begin_bump},
        _end_track{end_track},
        Net{Priority{3}, modes, name}
    {
    }

    BumpToTrackNet::~BumpToTrackNet() noexcept {
    }

    auto BumpToTrackNet::update_tob_postion(hardware::TOB* prev_tob, hardware::TOB* next_tob) -> void {
        this->_begin_bump = hardware::Bump::update_bump(this->_begin_bump, prev_tob, next_tob);
    }

    auto BumpToTrackNet::swap_tob_position(hardware::TOB* tob1, hardware::TOB* tob2) -> void {
        this->_begin_bump = hardware::Bump::swap_bump(this->_begin_bump, tob1, tob2);
    }

    auto BumpToTrackNet::route(hardware::Interposer* interposer, const algo::RouteStrategy& strategy) -> void {
        strategy.route_bump_to_track_net(interposer, this);
    }

    auto BumpToTrackNet::incremental_route(
        hardware::Interposer* interposer, const algo::IncreRouting& strategy, algo::RouteEngine& engine
    ) -> bool {
        return strategy.route_bump_to_track_net(interposer, this, engine);
    }

    auto BumpToTrackNet::update_priority(float bias) -> void {
        assert(0 <= bias && bias < 1);
        this->_priority = Priority{this->_priority.value() + bias};
    }

    auto BumpToTrackNet::coords() const -> std::Vector<hardware::Coord> {
        return std::Vector<hardware::Coord>{this->begin_bump()->coord(), this->end_track()->coord()};
    }

    auto BumpToTrackNet::check_accessable_cobunit() -> void {
        std::usize index = _end_track->coord().index;
        std::usize bank_size = hardware::COB::INDEX_SIZE/2;
        std::usize wilton_size = hardware::COBUnit::WILTON_SIZE;
        std::usize id {(index/bank_size)*wilton_size + (index%wilton_size)};
        
        std::HashSet<std::usize> cobunit_ids {id};
        _begin_bump->intersect_access_unit(cobunit_ids);
    }

    auto BumpToTrackNet::accessable_cobunit() -> std::HashMap<hardware::Bump*, std::HashSet<std::usize>> {
        std::HashMap<hardware::Bump*, std::HashSet<std::usize>> map {};
        map.emplace(this->_begin_bump, this->_begin_bump->accessable_cobunit());
        return map;
    }

    auto BumpToTrackNet::to_string() const -> std::String {
        return std::format(
            "{}: Begin bump: '{}' to End track '{}'", this->_name, this->_begin_bump->coord(), this->_end_track->coord()
        );
    }

    auto BumpToTrackNet::port_number() const -> std::usize {
        return 2;
    }

    auto BumpToTrackNet::check_relativity(const hardware::Bump* node) const -> const Net* {
        if (this->_begin_bump->coord() == node->coord()) {
            return dynamic_cast<const Net*>(this);
        }
        else {
            return nullptr;
        }
    }

    auto BumpToTrackNet::check_relativity(const hardware::Track* node) const -> const Net* {
        if (this->_end_track->coord() == node->coord()) {
            return dynamic_cast<const Net*>(this);
        }
        else {
            return nullptr;
        }
    }

    auto BumpToTrackNet::search_related_nets(std::Vector<Net*>& nets) -> void {
        clear_related_nets();

        auto iter = std::find(nets.begin(), nets.end(), this);
        if (iter != nets.end()) {
            nets.erase(iter);
        }
        this->_related_nets_bump.emplace(this->_begin_bump, search_nets_node<hardware::Bump>(this->_begin_bump, nets));
        this->_related_nets_track.emplace(this->_end_track, search_nets_node<hardware::Track>(this->_end_track, nets));
    }

    auto BumpToTrackNet::connection_state() const -> std::Tuple<std::Vector<const hardware::Bump*>, std::Vector<const hardware::Bump*>, std::Vector<const hardware::Track*>> {
        std::Vector<const hardware::Bump*> routable_bumps{}, unroutable_bumps{};
        std::Vector<const hardware::Track*> unroutable_tracks{};

        const auto& package = this->pathpackage();

        (package.find_bump(this->_begin_bump).has_value() ? routable_bumps : unroutable_bumps).emplace_back(this->_begin_bump);
        if (!package.find_track(this->_end_track).has_value()) {
            unroutable_tracks.emplace_back(this->_end_track);
        }

        return std::Tuple<std::Vector<const hardware::Bump*>, std::Vector<const hardware::Bump*>, std::Vector<const hardware::Track*>> {
            routable_bumps, unroutable_bumps, unroutable_tracks
        };
    }

    auto BumpToTrackNet::nodes_map() -> std::HashMap<hardware::Bump*, std::HashSet<hardware::Bump*>> {
        return std::HashMap<hardware::Bump*, std::HashSet<hardware::Bump*>> {};
    }

    auto BumpToTrackNet::nodes_direction() -> std::HashMap<hardware::Bump*, hardware::TOBBumpDirection> {
        return std::HashMap<hardware::Bump*, hardware::TOBBumpDirection> {
            {this->_begin_bump, hardware::TOBBumpDirection::BumpToTOB}
        };
    }

    auto BumpToTrackNet::operator == (const Net& net) const -> bool {
    try {
        auto cast_net = dynamic_cast<const BumpToTrackNet&>(net);
        return *this->_begin_bump == *cast_net._begin_bump && *this->_end_track == *cast_net._end_track;
    }
    catch (const std::bad_cast& e) {
        return false;
    }
    }

    auto BumpToTrackNet::operator == (const BumpToTrackNet& net) const -> bool {
        return *this->_begin_bump == *net._begin_bump && *this->_end_track == *net._end_track;
    }

    auto BumpToTrackNet::track_ports() const -> std::Pair<std::HashSet<hardware::Track*>, bool> {
        std::HashSet<hardware::Track*> tracks {this->_end_track};

        auto collect = [&](const auto& package_v) {
            std::for_each(package_v.begin(), package_v.end(), [&](const auto& package) {
                const auto& [_1, _2, t] = package;
                tracks.emplace(t);
            });
        };
        collect(this->_path_package._tob_to_track);
        collect(this->_path_package._track_to_tob);     // should be empty

        if (tracks.size() < this->port_number()) {
            return std::Pair<std::HashSet<hardware::Track*>, bool>{tracks, false};
        }
        else if (tracks.size() == this->port_number()) {
            return std::Pair<std::HashSet<hardware::Track*>, bool>{tracks, true};
        }
        else {
            throw std::logic_error("BumpToTrackNet::track_ports(): collected tracks.size() > port_number()");
        }
    }

    auto BumpToTrackNet::name() const -> const std::String& {
        return this->_name;
    }

    auto BumpToTrackNet::path_in_order() const -> std::Vector<PathInOrder> {
    try{
        const auto& package = this->pathpackage();
        if (package._tob_to_track.size() > 0) {
            // should has path
            const auto& head_tobconnector = std::get<1>(*package._tob_to_track.begin());
            const auto& regular_path = package._regular_path;
            return std::Vector<PathInOrder>{
                PathInOrder(
                    std::make_optional(this->_begin_bump), 
                    std::make_optional(head_tobconnector),
                    std::nullopt,
                    std::nullopt,
                    regular_path
                )
            };
        }
        else {
            return std::vector<PathInOrder>{};
        }
    }
    catch(const std::exception& e) {
        throw std::runtime_error("BumpToTrackNet::path_in_order(): " + std::string(e.what()));
    }
    }

    auto BumpToTrackNet::has_tob_in_ports(hardware::TOB* tob) const -> bool {
        return this->_begin_bump->tob()->coord() == tob->coord();
    }

    static auto classify_io_port_side(const hardware::TrackCoord& c) -> int {
        // 0: left, 1: right, 2: down, 3: up, -1: unknown
        if (c.dir == hardware::TrackDirection::Horizontal) {
            if (c.col == 0) { return 0; }
            if (c.col == hardware::Interposer::COB_ARRAY_WIDTH) { return 1; }
        } else {
            if (c.row == 0) { return 2; }
            if (c.row == hardware::Interposer::COB_ARRAY_HEIGHT) { return 3; }
        }
        return -1;
    }

    auto BumpToTrackNet::compute_bounding_box(int mode) -> std::Option<BoundingBox> {
        auto* tob = this->_begin_bump->tob();
        if (tob == nullptr) {
            return std::nullopt;
        }

        const auto chip = tob->coord_in_interposer();
        const auto io = this->_end_track->coord();
        const auto side = classify_io_port_side(io);
        if (side < 0) {
            return std::nullopt;
        }

        auto region = Region{};
        if (side == 0) {
            // Case 3: left IO
            region.col_min = io.col;
            region.col_max = chip.col;
            if (chip.row > io.row) { region.row_min = io.row; region.row_max = chip.row - 1; }
            else { region.row_min = chip.row; region.row_max = io.row; }
        } else if (side == 1) {
            // Case 4: right IO
            region.col_min = chip.col;
            region.col_max = io.col - 1;
            if (chip.row > io.row) { region.row_min = io.row; region.row_max = chip.row - 1; }
            else { region.row_min = chip.row; region.row_max = io.row; }
        } else if (side == 2) {
            // Case 5: down IO
            region.row_min = io.row;
            region.row_max = chip.row - 1;
            if (chip.col < io.col) { region.col_min = chip.col; region.col_max = io.col; }
            else { region.col_min = io.col; region.col_max = chip.col; }
        } else {
            // Case 6: up IO
            region.row_max = io.row - 1;
            region.row_min = chip.row;
            if (chip.col < io.col) { region.col_min = chip.col; region.col_max = io.col; }
            else { region.col_min = io.col; region.col_max = chip.col; }
        }

        region.normalize();
        this->_bounding_box.emplace(BoundingBox{region, mode, this, nullptr, std::nullopt});
        return this->_bounding_box;
    }
}
