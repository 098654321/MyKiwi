#include "./cli.hh"
#include "algo/netbuilder/netbuilder.hh"

#include <hardware/interposer.hh>
#include <circuit/basedie.hh>

#include <algo/router/single_mode/route_nets.hh>
#include <algo/router/common/maze/mazeroutestrategy.hh>
#include <algo/router/common/allocate/hopcroft_karp.hh>
#include <algo/placer/place.hh>
#include <algo/placer/placestrategy.hh>
#include <algo/placer/sa/saplacestrategy.hh>
#include <algo/router/multi_mode/route_multi_mode.hh>
#include <algo/router/single_mode/incremental/recorders/hardware_recorder.hh>
#include <parse/reader/module.hh>
#include <parse/writer/module.hh>
#include <parse/comparator/controlbits_parser.hh>

#include <std/utility.hh>
#include <std/range.hh>
#include <std/string.hh>
#include <debug/debug.hh>
#include <std/algorithm.hh>
#include <std/format.hh>

namespace kiwi {

    auto cli_main(
        std::StringView config_path, std::Option<std::StringView> output_path,
        int mode, std::optional<int> compare, bool placement, bool multi_mode,
        std::Option<std::usize> mm_k_candidates,
        std::Option<double> mm_converge_threshold
    ) -> int {
    try {
        debug::initial_log("./debug.log");
        std::FilePath output_file = std::FilePath(output_path.has_value() ? *output_path : ".");

        auto [interposer, basedie] = kiwi::parse::read_config(config_path, mode, multi_mode);
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

        if (multi_mode) {
            basedie->merge_same_mode_nets();
            basedie->merge_same_nonsync_nets_across_modes();    

            auto params = kiwi::algo::MultiModeParams{};
            if (mm_k_candidates.has_value()) {
                params.k_candidates = mm_k_candidates.value();
            }
            if (mm_converge_threshold.has_value()) {
                params.converge_threshold = mm_converge_threshold.value();
            }

            route_multi_mode(interposer.get(), basedie.get(), output_file, params);
        } else {
            route_single_mode(interposer.get(), basedie.get(), config_path, output_file, mode, compare);
        }

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

    auto route_single_mode(
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
    
    auto route_multi_mode(
        kiwi::hardware::Interposer* interposer, 
        kiwi::circuit::BaseDie* basedie,
        const std::FilePath& output_file,
        const kiwi::algo::MultiModeParams& params
    ) -> void {
        try {
            auto& nets_map = basedie->nets();
            auto it1 = nets_map.find(1);
            auto it2 = nets_map.find(2);
            if (it1 == nets_map.end() || it2 == nets_map.end()) {
                debug::fatal("multi-mode routing expects mode 1 and mode 2 nets, while at least one of them is not found");
            }

            auto view = kiwi::algo::OccupancyView{interposer};
            auto recorder = algo::HardwareRecorder{interposer};

            algo::set_resources(it1, it2);
            auto [shared, only1, only2] = algo::classify_nets(it1, it2);
            debug::info_fmt(
                "route_multi_mode(): classified nets summary: shared={}, mode1_only={}, mode2_only={}",
                shared.size(),
                only1.size(),
                only2.size()
            );

            algo::route_shared_nets(view, recorder, interposer, shared);
            algo::route_mode_only_nets(view, recorder, interposer, it1, it2, params);

            parse::output_two_modes_from_routing_results(interposer, output_file, basedie, 1, 2);
        }
        catch (const std::exception& err) {
            debug::error_fmt("route_multi_mode() failed: {}", err.what());
            throw std::runtime_error(std::format("route_multi_mode() failed: {}", err.what()));
        }
    }
}