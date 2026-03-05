#pragma once

#include <std/collection.hh>
#include <hardware/track/track.hh>
#include <hardware/bump/bump.hh>
#include <hardware/cob/cob.hh>
#include <hardware/tob/tob.hh>
#include <hardware/interposer.hh>
#include <circuit/basedie.hh>
#include <std/file.hh>
#include <std/utility.hh>
#include <std/memory.hh>


namespace kiwi::circuit {
    class Net;
    class SyncNet;
}



namespace kiwi::parse {

    struct Controlbits {
        std::HashMap<hardware::TOBCoord, std::Array<hardware::TOBBumpDirection, 128>> tob_bumpsig_direction;
        std::HashMap<hardware::TOBCoord, std::Array<hardware::TOBTrackDirection, 128>> tob_tracksig_direction;
        std::HashMap<hardware::COBCoord, std::HashMap<hardware::COBDirection, std::Bits<128>>> cobsig_direction;
        std::HashMap<hardware::TOBCoord, std::Array<int, 128>> bumptohctrl;
        std::HashMap<hardware::TOBCoord, std::Array<int, 128>> hctrltovctrl;
        std::HashMap<hardware::TOBCoord, std::Bits<64>> vctrltotrack;
        std::HashMap<hardware::COBCoord, std::HashMap<hardware::COBSWDirection, std::Bits<128>>> cobsw;
    };


    auto load_controlbits(const std::FilePath& path, int mode) -> std::Option<Controlbits>;
    auto bits_to_paths(hardware::Interposer*, circuit::BaseDie*, const Controlbits&, int mode) -> void;

    auto parse_line(std::String line) -> std::Tuple<std::String, std::Vector<std::String>>;
    auto parse_cob(const std::Vector<std::String>&, Controlbits&, const std::String&) -> void;
    auto parse_tob(const std::Vector<std::String>&, Controlbits&, const std::String&, std::Array<std::unordered_map<hardware::TOBCoord, std::bitset<128>>*, 4>&) -> bool;
    auto process_bump_sig(Controlbits&, const std::unordered_map<hardware::TOBCoord, std::bitset<128>>&, const std::unordered_map<hardware::TOBCoord, std::bitset<128>>&) -> void;
    auto process_track_sig(Controlbits&, const std::unordered_map<hardware::TOBCoord, std::bitset<128>>&, const std::unordered_map<hardware::TOBCoord, std::bitset<128>>&) -> void;

    auto load_tobconnector(hardware::Interposer*, const Controlbits&, std::HashMap<hardware::Bump*, hardware::TOBBumpDirection>, circuit::Net*) -> void;
    auto load_tobconnector_sync(hardware::Interposer*, const Controlbits&, circuit::SyncNet*) -> void;
    auto load_path_with_maze(circuit::Net*, hardware::Interposer* , const Controlbits&) -> void;
    auto load_path_with_maze_sync(circuit::SyncNet*, hardware::Interposer* , const Controlbits&) -> void;
    auto load_length(circuit::Net*) -> void;
    auto load_length_sync(circuit::SyncNet*) -> void;

    auto show_path_from_bits(const std::Vector<std::Rc<circuit::Net>>& nets) -> void;

    auto adj_tracks(hardware::Interposer*, hardware::Track*, const Controlbits&) -> std::Vector<std::Tuple<hardware::Track*, hardware::COBConnector>>;
    auto sw_type(hardware::Track*, hardware::Track*) -> std::Tuple<hardware::COBSWDirection, hardware::COBCoord, std::usize>;

}


