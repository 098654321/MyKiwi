// load tob
    // registers:
    // tob2bump_bank/bump2tob_bank: value=0/1 for begin bump, value=1/0 for end bump; value=0/0 for bump not used
    // dly, drv: useless
    // hctrl_bank, vctrl_bank, bank_sel: parse mux value
    // tob2track, track2tob: value=1/0 for begin track, value=0/1 for end track; value=0/0 for track not used

    // datastructure:
    // unordered_map1: begin_bump -> end_bump
    // unordered_map2: bump -> tuple<tobconnector, track>
    // unordered_map3: begin_bump -> end_track
    // unordered_map4: begin_track -> end_bump

    // algo:
    // 1. find all the begin_bumps & end_bumps by tob2bump_bank/bump2tob_bank
    // 2. find all the begin_tracks & end_tracks by tob2track/track2tob
    // 3. parse mux_values using begin_bumps & end_bumps(load_tobconnector()), and get begin_tracks_parse & end_tracks_parse respectively
    // 4. check if begin_tracks_parse & end_tracks_parse are consistent with begin_tracks & end_tracks respectively
    // 5. for all the bumps, construct tobconnector, and construct unordered_map2
    
// load cob
    // registers:
    // <dir>_sel: value=0 for signal goes from track into cob, value=1 for signal goes from cob into track
    // sw_<dir>: value=0 for disconnected, value=1 for connected

    // datastructure:
    // unordered_map5: begin_track -> path(track+cobconnector)

    // algo:
    // 1. for all the begin_tracks, find the path using algo in load_path_with_maze and store the path in unordered_map5

// build nets using unordered_map1, mode, name, and add the net to basedie
// set path for all the nets using unordered_map1, 2, 3

#include <global/debug/debug.h>
#include <global/utility/collection.h>
#include <hardware/interposer.hh>
#include <parse/controlbits.hh>
#include <string>
#include <tuple>
#include <unordered_map>
#include <exception>


auto print_help() -> void;
auto parse_arguments(int, char**) -> std::tuple<std::string, int>;
auto load_hardware(hardware::Interposer*, const parser::ControlBits&) -> std::tuple<std::set<hardware::Bump*>, std::set<hardware::Bump*>, std::set<hardware::Track*>, std::set<hardware::Track*>>;
auto check_hardware(const std::set<hardware::Bump*>&, const std::set<hardware::Bump*>&, const std::set<hardware::Track*>&, const std::set<hardware::Track*>&) -> std::tuple<std::unordered_map<hardware::Bump*, hardware::TOBConnector>, std::unordered_map<hardware::Bump*, hardware::TobConnector>>;


/////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
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
    auto controlbits = load_controlbits(folder, mode);

    // step5: load hardware from tob controlbits
    // notice: only part of the tracks are loaded here, because some other tracks are IO ports of the interposer
    //         and will be loaded during the maze algorithm
    auto [begin_bumps, end_bumps, begin_tracks, end_tracks] = load_hardware(interposer.get());

    // step6: check if there is error in the loaded hardware
    auto [begin_tobconnectors, end_tobconnectors] = check_hardware(begin_bumps, end_bumps, begin_tracks, end_tracks);

    // 到这里有bump->tobconnector, track的信息
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

    auto show_help = [](std::string& info) -> void {
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

    return {file_path, mode};
}
    
auto load_hardware(hardware::Interposer* interposer, const parser::ControlBits& controlbits) -> std::tuple<std::set<hardware::Bump*>, std::set<hardware::Bump*>, std::set<hardware::Track*>, std::set<hardware::Track*>>
{
    auto begin_bumps = std::set<hardware::Bump*>{};
    auto end_bumps = std::set<hardware::Bump*>{};
    auto begin_tracks = std::set<hardware::Track*>{};
    auto end_tracks = std::set<hardware::Track*>{};
    std::size_t N = 128;

    for (auto& [tobcoord, bits]: controlbits.tob_bumpsig_direction) {
        for (std::size_t i = 0; i < N; i++) {
            if (bits[i] == hardware::TOBBumpDirection::BumpToTrack) {
                begin_bumps.insert(interposer->get_bump(tobcoord, i));
            } else {
                end_bumps.insert(interposer->get_bump(tobcoord, i));
            }
        }
    }
    for (auto& [tobcoord, bits]: controlbits.tob_tracksig_direction) {
        for (std::size_t i = 0; i < N; i++) {
            if (bits[i] == hardware::TOBTrackDirection::TrackToBump) {
                begin_tracks.insert(interposer->get_track(tobcoord, i));
            } else {
                end_tracks.insert(interposer->get_track(tobcoord, i));
            }
        }
    }
}

auto check_hardware(const std::set<hardware::Bump*>& begin_bumps, const std::set<hardware::Bump*>& end_bumps, const std::set<hardware::Track*>& begin_tracks, const std::set<hardware::Track*>& end_tracks) -> std::tuple<std::unordered_map<hardware::Bump*, hardware::TOBConnector>, std::unordered_map<hardware::Bump*, hardware::TobConnector>>
{
    auto begin_tracks_copy = begin_tracks;
    auto end_tracks_copy = end_tracks;
    auto begin_tobconnectors = std::unordered_map<hardware::Bump*, hardware::TOBConnector>{};
    auto end_tobconnectors = std::unordered_map<hardware::Bump*, hardware::TOBConnector>{};

    auto get_tobconnector = [](hardware::Bump* bump, hardware::TOBBumpDirection dir, auto& bump_to_tobconnector) -> void {
        auto tobcoord = bump->tob()->coord();
        auto bump_index = bump->index();
        
        auto hctrl = controlbits.bumptohctrl.at(tobcoord).at(bump_index) + (bump_index/8)*8;
        auto trans_hctrl = (hctrl/64)*64 + (hctrl%8)*8 + (hctrl - (hctrl/64)*64)/8;
        auto vctrl = controlbits.hctrltovctrl.at(tobcoord).at(trans_hctrl) + (hctrl/64)*64 + (hctrl%8)*8;
        auto track_index = controlbits.vctrltotrack.at(tobcoord)[vctrl%64] == 0? vctrl : (vctrl+64)%128;
        auto connector = bump->tob()->bump_track_connectors_chain(bump_index, track_index, dir); 

        bump_to_tobconnector.emplace(bump, connector);
        bump->set_connected_track(interposer->get_track(tobcoord, track_index));
        
        debug::debug_fmt("load_tobconnector(): calculated index = {}->{}->{}->{}", bump_index, hctrl, vctrl, track_index);
    }

    auto check_track_index = [](const std::set<hardware::Bump*>& bumps, std::set<hardware::Track*>& tracks_copy, hardware::TOBBumpDirection dir, auto& bump_to_tobconnector) {
        for (auto bump: bumps) {
            get_tobconnector(bump, dir, bump_to_tobconnector);
            auto track = bump->connected_track();
            if (tracks_copy.contains(track)) {
                tracks_copy.erase(track);
            }
            else {
                throw std::logic_error(std::format("bumps {} contains track {} that is not in tracks", bump->coord(), track->coord()));
            }
        }
        if (!tracks_copy.empty()) {
            throw std::logic_error("tracks contains track that is not connected to the bumps");
        }
    }
    
    check_track_index(begin_bumps, begin_tracks_copy, hardware::TOBBumpDirection::BumpToTrack, begin_tobconnectors);
    check_track_index(end_bumps, end_tracks_copy, hardware::TOBBumpDirection::TrackToBump, end_tobconnectors);

    return {begin_tobconnectors, end_tobconnectors};
}



