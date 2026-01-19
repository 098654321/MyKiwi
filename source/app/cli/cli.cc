#include "./cli.hh"
#include "algo/netbuilder/netbuilder.hh"

#include <hardware/interposer.hh>
#include <circuit/basedie.hh>

#include <algo/router/route_nets.hh>
#include <algo/router/common/maze/mazeroutestrategy.hh>
#include <algo/router/common/allocate/hopcroft_karp.hh>
#include <algo/placer/place.hh>
#include <algo/placer/placestrategy.hh>
#include <algo/placer/sa/saplacestrategy.hh>

#include <parse/reader/module.hh>
#include <parse/writer/module.hh>
#include <parse/comparator/controlbits_parser.hh>

#include <std/utility.hh>
#include <std/range.hh>
#include <std/string.hh>
#include <debug/debug.hh>
#include <std/algorithm.hh>

namespace kiwi {

    auto cli_main(
        std::StringView config_path, std::Option<std::StringView> output_path, 
        int mode, std::optional<int> compare, bool try_all_modes, bool placement
    ) -> int {
    try {
        debug::initial_log("./debug.log");
        std::FilePath output_file = std::FilePath(output_path.has_value() ? *output_path : ".");

        auto [interposer, basedie] = kiwi::parse::read_config(config_path, mode, try_all_modes); 
        algo::build_nets(basedie.get(), interposer.get());

        if (placement) {
            debug::debug("Start layout ...");
            auto strategy = algo::SAPlaceStrategy();
            place(interposer.get(), topdies, basedie.get(), strategy);
            assert(strategy.is_valid_placement(interposer.get(), topdies));
            auto end_time1 = std::chrono::high_resolution_clock::now();

            auto cost = evaluate_placement(interposer.get(), topdies, basedie.get(), strategy);
            debug::info_fmt("Layout evaluation result: {}", cost);
            debug::info("Layout result:");
            for (const auto& topdie : topdies) {
                if (topdie->tob()) {
                    debug::info_fmt("Chip {} is located in {}", topdie->name(), topdie->tob()->coord());
                } else {
                    debug::warning_fmt("Chip {} has not been assigned a TOB", topdie->name());
                }
            }
        }
       

        debug::debug("Start routing ...");
        if (!try_all_modes && mode == 0) {  // not incremental routing 
            algo::route_nets(interposer.get(), basedie.get(), algo::MazeRouteStrategy{false}, algo::HK{}, mode, false, try_all_modes);
            parse::output_from_routing_results(interposer.get(), output_file, basedie.get(), mode, try_all_modes);
        }
        else {  // incremental routing with two situations: route all modes (try_all_modes == true) or route single mode (try_all_modes == false && mode > 0)
            basedie->merge_same_mode_nets();
            auto [has_bits, has_other_bits] = parse::read_controlbits(config_path, interposer.get(), basedie.get(), mode, try_all_modes);
            if (!has_bits) {
                algo::route_nets(interposer.get(), basedie.get(), algo::MazeRouteStrategy{true}, algo::HK{}, mode, true, try_all_modes, has_other_bits);
                parse::output_from_routing_results(interposer.get(), output_file, basedie.get(), mode, try_all_modes);
            }
            else {
                debug::info("Already has control bits, skip the routing process");
            }

            if (!try_all_modes && compare.has_value()) {
                std::string current_file {"controlbits_" + std::to_string(mode) + ".txt"};
                std::string target_file {"controlbits_" + std::to_string(compare.value()) + ".txt"};
                parse::compare(current_file, target_file);
            }
        }
        
        return 0;
    }
    catch (const Exception& err){
        debug::exception("Unexpected exception");
    }
    }

}