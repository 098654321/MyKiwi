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
        int mode, std::optional<int> compare, bool placement
    ) -> int {
    try {
        debug::initial_log("./debug.log");
        std::FilePath output_file = std::FilePath(output_path.has_value() ? *output_path : ".");

        auto [interposer, basedie] = kiwi::parse::read_config(config_path, mode);
        algo::build_nets(basedie.get(), interposer.get());

        if (placement) {
            std::Vector<circuit::TopDieInstance*> topdies;
            for (auto& [name, topdie_inst] : basedie->topdie_insts()) {
                topdies.push_back(topdie_inst.get());
            }
            if (topdies.empty()) {
                debug::warning("No chip instances require layout");
            }

            place(interposer.get(), basedie.get(), topdies);
        }

        route(interposer.get(), basedie.get(), config_path, output_file, mode, compare);

        return 0;
    }
    catch (const Exception& err){
        debug::exception("Unexpected exception");
    }
    }

    auto place(kiwi::hardware::Interposer* interposer, kiwi::circuit::BaseDie* basedie, std::vector<kiwi::circuit::TopDieInstance*>& topdies) -> void {
        debug::debug("Start layout ...");
        auto strategy = algo::SAPlaceStrategy();
        place(interposer, topdies, basedie, strategy);
        assert(strategy.is_valid_placement(interposer, topdies));
        auto end_time1 = std::chrono::high_resolution_clock::now();

        debug::info("Layout result:");
        for (const auto& topdie : topdies) {
            if (topdie->tob()) {
                debug::info_fmt("Chip {} is located in {}", topdie->name(), topdie->tob()->coord());
            } else {
                debug::warning_fmt("Chip {} has not been assigned a TOB", topdie->name());
            }
        }
    }

    auto route(
        kiwi::hardware::Interposer* interposer, kiwi::circuit::BaseDie* basedie,
        std::StringView config_path,  const std::FilePath& output_file,
        int mode, std::optional<int> compare
    ) -> void {
        debug::debug("Start routing ...");
        if (mode == 0) {  // not incremental routing
            auto [has_bits, has_other_bits] = parse::read_controlbits(config_path, interposer, basedie, mode);
            if (!has_bits) {
                algo::route_nets(interposer, basedie, algo::MazeRouteStrategy{false}, algo::HK{}, mode, false);
                parse::output_from_routing_results(interposer, output_file, basedie, mode);
            }
            else if (has_other_bits) {
                debug::info("Has other control bits, skip the routing process");
            }
        }
        else {  // incremental routing with single mode (mode > 0)
            basedie->merge_same_mode_nets();
            auto [has_bits, has_other_bits] = parse::read_controlbits(config_path, interposer, basedie, mode);
            if (!has_bits) {
                algo::route_nets(interposer, basedie, algo::MazeRouteStrategy{true}, algo::HK{}, mode, true, has_other_bits);
                parse::output_from_routing_results(interposer, output_file, basedie, mode);
            }
            else {
                debug::info("Already has control bits, skip the routing process");
            }

            if (compare.has_value()) {
                std::string current_file {"controlbits_" + std::to_string(mode) + ".txt"};
                std::string target_file {"controlbits_" + std::to_string(compare.value()) + ".txt"};
                parse::compare(current_file, target_file);
            }
        }
    }
    

}