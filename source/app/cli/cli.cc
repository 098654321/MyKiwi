#include "./cli.hh"
#include "algo/netbuilder/netbuilder.hh"

#include <hardware/interposer.hh>
#include <circuit/basedie.hh>

#include <algo/router/route_nets.hh>
#include <algo/router/common/maze/mazeroutestrategy.hh>
#include <algo/router/common/allocate/hopcroft_karp.hh>

#include <parse/reader/module.hh>
#include <parse/writer/module.hh>

#include <std/utility.hh>
#include <std/range.hh>
#include <std/string.hh>
#include <debug/debug.hh>
#include <std/algorithm.hh>

namespace kiwi {

    auto cli_main(std::StringView config_path, std::Option<std::StringView> output_path, int mode) -> int {
    try {
        debug::initial_log("./debug.log");
        std::FilePath output_file = std::FilePath(output_path.has_value() ? *output_path : ".") / ("controlbits_" + std::to_string(mode) + ".txt");

        auto [interposer, basedie] = kiwi::parse::read_config(config_path, mode); 

        algo::build_nets(basedie.get(), interposer.get());
        
        if (mode == 0) {
            algo::route_nets(interposer.get(), basedie.get(), algo::MazeRouteStrategy{false}, algo::HK{}, mode, false);
            parse::write_control_bits(
                interposer.get(),
                output_file
            );
        }
        else {
            basedie->merge_same_mode_nets();
            auto [has_bits, has_other_bits] = parse::read_controlbits(config_path, interposer.get(), basedie.get(), mode);
            if (!has_bits) {
                algo::route_nets(interposer.get(), basedie.get(), algo::MazeRouteStrategy{true}, algo::HK{}, mode, true, has_other_bits);
                parse::write_control_bits(
                    interposer.get(),
                    output_file
                );
            }
            else {
                debug::info("Already has control bits, skip the routing process");
            }
        }
        
        return 0;
    }
    catch (const Exception& err){
        debug::exception("Unexpected exception");
    }
    }

}