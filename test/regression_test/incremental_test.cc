#include <algo/route_data.hh>
#include "algo/netbuilder/netbuilder.hh"
#include <catch2/catch_test_macros.hpp>
#include <std/string.hh>
#include <std/file.hh>
#include <utility/file.hh>
#include <debug/debug.hh>
#include <parse/reader/module.hh>
#include <parse/writer/module.hh>
#include <algo/router/route_nets.hh>
#include <algo/router/common/maze/mazeroutestrategy.hh>
#include <algo/netbuilder/netbuilder.hh>
#include <algo/router/common/allocate/hopcroft_karp.hh>
#include <hardware/interposer.hh>
#include <circuit/basedie.hh>
#include <std/collection.hh>
#include <std/format.hh>
#include <std/file.hh>
#include <algorithm>


namespace kiwi::test {

    void analyze_route_data(const kiwi::algo::RouteData& data, std::FilePath output_file);

    void PLEASE_DO_NOT_FAIL_INCRE(std::usize id, std::String info, std::usize mode, std::usize total_cycle) {
        WHEN("Case " + std::to_string(id) + ": " + info) {
            kiwi::algo::RouteData data{};
            for (auto i = 0; i < total_cycle; i++) {
                std::FilePath config_path{"../test/config/case" + std::to_string(id)};
                debug::initial_log("debug.log");

                auto [interposer, basedie] = kiwi::parse::read_config(config_path, mode);
                algo::build_nets(basedie.get(), interposer.get());
                basedie->merge_same_mode_nets();
                auto [has_bits, has_other_bits] = parse::read_controlbits(config_path, interposer.get(), basedie.get(), mode);
                if (!has_bits) {
                    auto data_per_cycle = algo::route_nets(interposer.get(), basedie.get(), algo::MazeRouteStrategy{true}, algo::HK{}, mode, true, has_other_bits);
                    data.collect_data_in_cycle(i, data_per_cycle);
                }
                else {
                    debug::info("Already has control bits, skip the routing process");
                }

                // utility::append_logs("debug.log", "regression_test.log");
            }

            std::String output_file{"incremental_regression_test.log"};
            analyze_route_data(data, output_file);
        }
    }

    
    SCENARIO("Regression test for incremental routing", "[incremental]"){
        
        GIVEN("Configs, describing connections, external_ports, topdies and topdie_insts"){
            // std::ofstream file("regression_test.log", std::ios::trunc);
            //! notice: cob array here is 9*12
            PLEASE_DO_NOT_FAIL_INCRE(19, "modified case18 to with more reusable/uon-reusable nets be mixed together", 1, 1);
        }
    }


    void analyze_route_data(const kiwi::algo::RouteData& data, std::FilePath output_file) {
        std::Vector<std::String> message {
            "*****************************Loop Test Result*****************************\n"
        };

        // collector
        float ave_total_length{0.0}, ave_sync_length{0.0}, ave_fail_rate{0.0}, total_sync_net{0.0};
        std::Vector<float> total_length_vec{};
        std::Vector<float> ave_sync_l_vec{};
        std::Vector<std::usize> sync_net_num_vec{};
        std::Vector<std::usize> fail_net_vec{};
        std::Vector<float> not_used_data_vec{};
        std::Vector<float> monopolized_by_reuse_data_vec{};
        std::Vector<float> has_nonreuse_data_vec{};

        // collect data in each cycle
        for (const auto& [cycle, data_per_cycle]: data.data()) {
            ave_total_length += data_per_cycle._total_length;
            ave_sync_length += data_per_cycle._ave_sync_length * data_per_cycle._sync_net_number;
            ave_fail_rate += data_per_cycle._failed_net == 0 ? 0 : 1;
            total_sync_net += data_per_cycle._sync_net_number;

            total_length_vec.emplace_back(data_per_cycle._total_length);
            ave_sync_l_vec.emplace_back(data_per_cycle._ave_sync_length);
            sync_net_num_vec.emplace_back(data_per_cycle._sync_net_number);
            fail_net_vec.emplace_back(data_per_cycle._failed_net);
            not_used_data_vec.emplace_back(std::get<0>(data_per_cycle._reg_data));
            monopolized_by_reuse_data_vec.emplace_back(std::get<1>(data_per_cycle._reg_data));
            has_nonreuse_data_vec.emplace_back(std::get<2>(data_per_cycle._reg_data));
        }

        // print global message
        message.emplace_back(std::format("Average Total Length: {}\n", ave_total_length / (float)data.data().size()));
        message.emplace_back(std::format("Average Sync Length: {}\n", ave_sync_length / total_sync_net));
        message.emplace_back(std::format("Average Fail Rate: {}\n", ave_fail_rate / (float)data.data().size()));
        message.emplace_back(std::format("Final Not Used: {}\n", not_used_data_vec.back()));
        message.emplace_back(std::format("Final Monopolized by Reuse: {}\n", monopolized_by_reuse_data_vec.back()));
        message.emplace_back(std::format("Final Has Nonreuse: {}\n", has_nonreuse_data_vec.back()));
        message.emplace_back(
            "**************************End of Loop Test Result**************************\n"
        );

        std::String m{};
        for (const auto& msg: message) {
            m += msg + "\n";
        }
        debug::info(m);

        // write to file
        std::ofstream output_file_stream(output_file);
        if (!output_file_stream.is_open()) {
            debug::exception_in("analyze_route_data", "output file open failure");
        }
        auto output = [&]<typename T>(const std::Vector<T>& vec) {
            for (const auto& v: vec) {
                output_file_stream << std::format("{} ", v);
            }
            output_file_stream << "\n";
        };

        output_file_stream << "// cycle, total_length, ave_sync_length, sync_net_number, failed_net, not_used, monopolized_by_reuse, has_nonreuse\n";
        output_file_stream << data.data().size() << "\n";
        output(total_length_vec);
        output(ave_sync_l_vec);
        output(sync_net_num_vec);
        output(fail_net_vec);
        output(not_used_data_vec);
        output(monopolized_by_reuse_data_vec);
        output(has_nonreuse_data_vec);
    }

}



