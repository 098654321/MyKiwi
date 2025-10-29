#include "./module.hh"
#include "./config/config.hh"
#include "./reader.hh"
#include "circuit/basedie.hh"
#include <parse/reader/controlbits/controlbits.hh>
#include "hardware/interposer.hh"
#include <algorithm>

#include <debug/debug.hh>
#include <memory>

namespace kiwi::parse {

    auto read_config(const std::FilePath& config_folder, int mode, bool try_all_modes)
        -> std::Tuple<std::Box<hardware::Interposer>, std::Box<circuit::BaseDie>> 
    {
        debug::info_fmt("Read config in '{}'", config_folder.string());

        auto interposer = std::make_unique<hardware::Interposer>();
        auto basedie = std::make_unique<circuit::BaseDie>();

        read_config(config_folder, interposer.get(), basedie.get(), mode, try_all_modes);
        
        return {std::move(interposer), std::move(basedie)};
    }

    auto read_config(
        const std::FilePath& config_folder,
        hardware::Interposer* interposer,
        circuit::BaseDie* basedie,
        int mode,
        bool try_all_modes
    ) -> void
    {
        auto config = load_config(config_folder, mode, try_all_modes);
        auto reader = Reader{config, interposer, basedie};
        reader.build();
    }

    // TODO: 改一下返回值，需要能够判断是否需要做增量布线，以及如果要做的情况下是否读入了 controlbits
    auto read_controlbits(
        const std::FilePath& config_folder,
        hardware::Interposer* interposer,
        circuit::BaseDie* basedie,
        int mode,
        bool try_all_modes
    ) -> std::Pair<bool, bool> {
        if (try_all_modes) {
            return {false, false};
        }

        debug::info("Load controlbits ...");

        auto controlbits = load_controlbits(config_folder, mode);
        if (controlbits.has_value()) {
            bits_to_paths(interposer, basedie, controlbits.value(), mode);
            return std::Pair<bool, bool>{true, true};
        }
        else {
            std::Vector<std::pair<int, std::usize>> mode_size {};
            for (auto& [m, nets]: basedie->nets()) {
                if (m == mode) continue;

                std::usize size {0};
                for (auto& p_net: nets) {
                    size += p_net->port_number();
                }
                mode_size.emplace_back(m, size);
            }

            std::sort(mode_size.begin(), mode_size.end(), [](const std::pair<int, std::usize>& a, const std::pair<int, std::usize>& b) {
                return a.second > b.second;
            });
            
            bool has_controlbits = false;
            for (auto& [m, _]: mode_size) {
                auto controlbits = load_controlbits(config_folder, m);
                if (controlbits.has_value()) {
                    bits_to_paths(interposer, basedie, controlbits.value(), m);
                    has_controlbits = true;
                    break;
                }
            }
            return {false, has_controlbits};
        }        
    }

}