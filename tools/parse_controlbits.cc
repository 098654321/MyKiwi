// parse_controlbits -file_path -mode
// init interposer
// load_controlbits()
// load tob
    // registers:
    // tob2bump_bank/bump2tob_bank: value=0/1 for begin bump, value=1/0 for end bump; value=0/0 for bump not used
    // dly, drv: useless
    // hctrl_bank, vctrl_bank, bank_sel: parse mux value
    // tob2track, track2tob: value=1/0 for begin track, value=0/1 for end track; value=0/0 for track not used

    // datastructure:
    // unordered_map1: begin_bump -> end_bump
    // unordered_map2: bump -> tuple<tobconnector, track>

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
    // unordered_map1: begin_track -> path(track+cobconnector)

    // algo:
    // 1. for all the begin_tracks, find the path using algo in load_path_with_maze and store the path in unordered_map1

// build nets using unordered_map1, mode, name, and add the net to basedie
// set path for all the nets using unordered_map1, 2, 3


    



