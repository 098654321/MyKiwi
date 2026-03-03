#include <global/debug/debug.hh>
#include <global/std/collection.hh>
#include <hardware/interposer.hh>
#include <parse/reader/controlbits/controlbits.hh>
#include <string>
#include <tuple>
#include <unordered_map>
#include <exception>
#include <type_traits>


namespace kiwi {

auto print_help() -> void;
auto parse_arguments(int, char**) -> std::tuple<std::string, int>;
auto load_hardware(hardware::Interposer*, const parse::Controlbits&) -> std::tuple<std::set<hardware::Bump*>, std::set<hardware::Bump*>, std::set<hardware::Track*>, std::set<hardware::Track*>>;
auto check_bump_and_track(hardware::Interposer*, const parse::Controlbits&,  std::set<hardware::Bump*>&, const std::set<hardware::Bump*>&, const std::set<hardware::Track*>&, const std::set<hardware::Track*>&) -> std::tuple<std::unordered_map<hardware::Bump*, hardware::TOBConnector>, std::unordered_map<hardware::Track*, hardware::TOBConnector>>;


/////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
try{
    if (argc != 3) {
        print_help();
    }

    // step1: parse arguments
    auto [folder, mode] = parse_arguments(argc, argv);

    // step2: initial log
    debug::initial_log("./debug.log");

    // step3: init hardware
    auto interposer = std::make_unique<hardware::Interposer>();
    auto basedie = std::make_unique<circuit::BaseDie>();

    // step4: load controlbits
    auto controlbits = parse::load_controlbits(folder, mode);
    if (!controlbits.has_value()) {
        throw std::logic_error("Controlbits not found");
    }

    // step5: load hardware from tob controlbits
    // notice: only part of the tracks are loaded here, because some other tracks are IO ports of the interposer
    //         and will be loaded during the maze algorithm
    auto [begin_bumps, end_bumps, begin_tracks, end_tracks] = load_hardware(interposer.get(), controlbits.value());

    // step6: check if there is error in the loaded hardware
    auto [begin_tobconnectors, end_tobconnectors] = check_bump_and_track(interposer.get(), controlbits.value(), begin_bumps, end_bumps, begin_tracks, end_tracks);

    // step7: 根据 cob 的controlbits信息开始寻找路径
    //  遍历所有的begin_tracks，对于每个begin_track，根据cob的controlbits信息做bfs的扩展搜索
    //  搜索的时候，如果遇到一个end_track，就回溯得到一条路径。然后构建一条从begin_bump到end_bump的net，再把相应的begin/end_bump, begin/end_track从记录中删除，把net加入basedie
    //  搜索的时候，如果遇到一个track是外部IO端口的，就构建一条从begin_bump到end_track的net，再把相应的begin_bump, begin_track从记录中删除

    // step8: 检查是否还有从IO到bump的net
    //  检查end_bumpds是否为空。如果是空的，说明都构造完了；如果不空，说明存在从IO为起点到end_bump的net
    //  如果不空，那么遍历end_tracks。对于每一个end_track，利用cob controlbits信息倒着得到从IO到end_track的路径，然后构建对应的net、删除end_track, end_bump的记录，把net加入basedie

    // step9: show net info
}
catch(const std::exception& e) {
    debug::error(std::format("Unexpected exception: {}", e.what()));
    return -1;
}
}

/////////////////////////////////////////////////////////////


void print_help() {
    debug::info("Usage: parse_controlbits -folder <controlbits_file_folder> -mode <mode>\n");
    return;
}

std::tuple<std::string, int> parse_arguments(int argc, char** argv) {
    auto arguments = std::vector<std::string>{};
    for (int i = 1; i < argc; i++) {
        arguments.push_back(argv[i]);
    }

    auto argument_index = [&arguments](const std::string& argument) -> int {
        for (int i = 0; i < arguments.size(); i++) {
            if (arguments[i] == argument) {
                return i;
            }
        }
        return -1;
    };

    auto show_help = [](std::string info) -> void {
        debug::info(info);
        print_help();
        std::exit(-1);
    };

    auto file_index = argument_index("-file");
    if (file_index == -1) {
        show_help("Error: -file argument not found\n");
    }
    if (file_index + 1 >= arguments.size()) {
        show_help("Error: -file argument without file path\n");
    }
    auto file_path = arguments[file_index + 1];

    auto mode_index = argument_index("-mode");
    if (mode_index == -1) {
        show_help("Error: -mode argument not found\n");
    }
    if (mode_index + 1 >= arguments.size()) {
        show_help("Error: -mode argument without mode\n");
    }
    auto mode = arguments[mode_index + 1];

    return {file_path, std::stoi(mode)};
}
    
auto load_hardware(hardware::Interposer* interposer, const parse::Controlbits& controlbits) -> std::tuple<std::set<hardware::Bump*>, std::set<hardware::Bump*>, std::set<hardware::Track*>, std::set<hardware::Track*>>
{
    auto begin_bumps = std::set<hardware::Bump*>{};
    auto end_bumps = std::set<hardware::Bump*>{};
    auto begin_tracks = std::set<hardware::Track*>{};
    auto end_tracks = std::set<hardware::Track*>{};
    std::size_t N = 128;

    for (auto& [tobcoord, bits]: controlbits.tob_bumpsig_direction) {
        for (std::size_t i = 0; i < N; i++) {
            auto bump = interposer->get_bump(tobcoord, i);
            if (!bump.has_value()) {
                throw std::logic_error("load_hardware(): Bump not found");
            }
            if (bits[i] == hardware::TOBBumpDirection::BumpToTOB) {
                begin_bumps.insert(*bump);
            } else {
                end_bumps.insert(*bump);
            }
        }
    }
    for (auto& [tobcoord, bits]: controlbits.tob_tracksig_direction) {
        for (std::size_t i = 0; i < N; i++) {
            auto coord_in_interposer = hardware::Interposer::TOB_COORD_MAP.at(tobcoord);
            auto track = interposer->get_track(coord_in_interposer.row, coord_in_interposer.col, hardware::TrackDirection::Vertical, i);
            if (!track.has_value()) {
                throw std::logic_error("load_hardware(): Track not found");
            }
            if (bits[i] == hardware::TOBTrackDirection::TrackToTOB) {
                begin_tracks.insert(*track);
            } else {
                end_tracks.insert(*track);
            }
        }
    }
}

auto check_bump_and_track(hardware::Interposer* interposer, const parse::Controlbits& controlbits, const std::set<hardware::Bump*>& begin_bumps, const std::set<hardware::Bump*>& end_bumps, const std::set<hardware::Track*>& begin_tracks, const std::set<hardware::Track*>& end_tracks) -> std::tuple<std::unordered_map<hardware::Bump*, hardware::TOBConnector>, std::unordered_map<hardware::Track*, hardware::TOBConnector>>
{
    auto begin_tracks_copy = begin_tracks;
    auto end_tracks_copy = end_tracks;
    auto begin_tobconnectors = std::unordered_map<hardware::Bump*, hardware::TOBConnector>{};
    auto end_tobconnectors = std::unordered_map<hardware::Track*, hardware::TOBConnector>{};

    auto get_tobconnector = [&controlbits, interposer](hardware::Bump* bump, hardware::TOBBumpDirection dir, auto& to_tobconnector) -> void {
        auto tobcoord = bump->tob()->coord();
        auto coord_in_interposer = hardware::Interposer::TOB_COORD_MAP.at(tobcoord);
        auto bump_index = bump->index();
        
        auto hctrl = controlbits.bumptohctrl.at(tobcoord).at(bump_index) + (bump_index/8)*8;
        auto trans_hctrl = (hctrl/64)*64 + (hctrl%8)*8 + (hctrl - (hctrl/64)*64)/8;
        auto vctrl = controlbits.hctrltovctrl.at(tobcoord).at(trans_hctrl) + (hctrl/64)*64 + (hctrl%8)*8;
        auto track_index = controlbits.vctrltotrack.at(tobcoord)[vctrl%64] == 0? vctrl : (vctrl+64)%128;
        auto connector = bump->tob()->bump_track_connectors_chain(bump_index, track_index, dir); 
        auto track = interposer->get_track(coord_in_interposer.row, coord_in_interposer.col, hardware::TrackDirection::Vertical, track_index);
        if (!track.has_value()) {
            throw std::logic_error("check_hardware(): Track not found");
        }
        auto tob_sig_direction = dir == hardware::TOBBumpDirection::BumpToTOB? hardware::TOBSignalDirection::BumpToTrack : hardware::TOBSignalDirection::TrackToBump;

        using connector_map_t = std::remove_reference_t<decltype(to_tobconnector)>;
        using key_t = typename connector_map_t::key_type;
        if constexpr (std::is_same_v<key_t, hardware::Bump*>) {
            to_tobconnector.emplace(bump, connector);
        } else if constexpr (std::is_same_v<key_t, hardware::Track*>) {
            to_tobconnector.emplace(*track, connector);
        }
        bump->set_connected_track(*track, tob_sig_direction);
        
        debug::debug_fmt("load_tobconnector(): calculated index = {}->{}->{}->{}", bump_index, hctrl, vctrl, track_index);
    };

    auto check_track_index = [get_tobconnector](const std::set<hardware::Bump*>& bumps, std::set<hardware::Track*>& tracks_copy, hardware::TOBBumpDirection dir, auto& to_tobconnector) {
        for (auto bump: bumps) {
            get_tobconnector(bump, dir, to_tobconnector);
            auto track = bump->connected_track();
            if (tracks_copy.contains(track)) {
                tracks_copy.erase(track);
            }
            else {
                throw std::logic_error(std::format("check_bump_and_track(): bumps {} contains track {} that is not in tracks", bump->coord(), track->coord()));
            }
        }
        if (!tracks_copy.empty()) {
            throw std::logic_error("check_bump_and_track(): tracks contains track that is not connected to the bumps");
        }
    };
    
    check_track_index(begin_bumps, begin_tracks_copy, hardware::TOBBumpDirection::BumpToTOB, begin_tobconnectors);
    check_track_index(end_bumps, end_tracks_copy, hardware::TOBBumpDirection::TOBToBump, end_tobconnectors);

    return {begin_tobconnectors, end_tobconnectors};
}


}
