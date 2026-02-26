#include "./saplacestrategy.hh"
#include "debug/debug.hh"

#include <algorithm>
#include <hardware/interposer.hh>
#include <hardware/tob/tob.hh>
#include <hardware/tob/tobcoord.hh>
#include <hardware/cob/cob.hh>
#include <hardware/track/track.hh>
#include <hardware/bump/bump.hh>

#include <circuit/net/net.hh>
#include <circuit/net/nets.hh>
#include <circuit/topdie/topdie.hh>
#include <circuit/basedie.hh>
#include <circuit/export/export.hh>
#include <circuit/connection/connection.hh>

#include <utility/random.hh>
#include <random>

#include <algo/netbuilder/netbuilder.hh>

#include <std/collection.hh>
#include <std/integer.hh>
#include <std/math.hh>
// #include <QColor>

namespace kiwi::algo {
    auto SAPlaceStrategy::place(
        hardware::Interposer* interposer,
        std::Vector<circuit::TopDieInstance*>& topdies
    ) const -> void {
        debug::info("Start Layout");
        if (topdies.empty()) {
            debug::warning("No top-level chip instance needs to be laid out");
            return;
        }
        debug::info_fmt("Totally {} top-level chip instance(s) need to be laid out", topdies.size());
        
        if (!interposer) {
            debug::error("Interposer pointer is empty");
            return;
        }

        auto nets = this->collect_nets(topdies);
        if (nets.empty()) {
            debug::warning("No network connection");
        }

        debug::info("Initial placement snapshot");
        for (auto* td : topdies) {
            auto tob = td ? td->tob() : nullptr;
            if (tob) {
                debug::info_fmt("TopDie {} at TOB {}", td->name(), tob->coord());
            } else {
                debug::info_fmt("TopDie {} at TOB {}", td->name(), "None");
            }
        }

        double temperature {this->_init_temperature}; 
        // auto total_cost = this->total_net_cost(nets) * this->_wirelength_weight + this->congestion_cost(interposer) * this->_congestion_weight + this->thermal_cost(topdies) * this->_thermal_weight;
        auto total_cost = this->total_net_cost(nets) * this->_wirelength_weight + this->thermal_cost(topdies) * this->_thermal_weight;
        auto best_cost = total_cost;
        auto best_solution = this->save_current_placement(topdies);

        std::size_t no_improvement_count = 0;
        std::size_t iteration = 0;

        while (temperature > this->_freeze_temperature && no_improvement_count < this->_max_no_improvement) {
            bool improved = false;
            for (std::usize n = 0; n < this->_solve_number; ++n) {
                auto new_total_cost = total_cost;

                if (this->decide_to_swap_topdie_inst(temperature)) {
                    debug::debug("Swap two top-level chip instance");
                    auto [topdie_inst1, topdie_inst2] = this->randomly_choice_two_topdie_insts(topdies);
                    
                    if (!topdie_inst1 || !topdie_inst2) {
                        debug::debug("topdie_inst is empty");
                        continue;
                    }
                    debug::debug_fmt("get two topdie inst: {} and {}", topdie_inst1->name(), topdie_inst2->name());

                    std::HashSet<circuit::Net*> changed_nets;
                    for (auto net : topdie_inst1->nets()) {
                        changed_nets.emplace(net);
                    }

                    for (auto net : topdie_inst2->nets()) {
                        changed_nets.emplace(net);
                    }

                    for (auto net : changed_nets) {
                        new_total_cost -= this->net_cost(net) * this->_wirelength_weight;
                    }

                    new_total_cost -= this->thermal_cost(topdies) * this->_thermal_weight;

                    auto tob1 = topdie_inst1->tob();
                    auto tob2 = topdie_inst2->tob();
                    
                    if (!tob1 || !tob2) {
                        debug::debug_fmt("TOB {} or {} is empty", tob1->coord(), tob2->coord());
                        continue;
                    }
                    
                    if (!this->is_changable(topdie_inst1, tob2) || !this->is_changable(topdie_inst2, tob1)) {
                        debug::debug_fmt("TOB {} or {} is not changable for {} and {}", tob1->coord(), tob2->coord(), topdie_inst1->name(), topdie_inst2->name());
                        continue;
                    }

                    bool tob1_occupied = false;
                    bool tob2_occupied = false;
                    
                    for (const auto& topdie : topdies) {
                        if (topdie != topdie_inst1 && topdie != topdie_inst2) {
                            if (topdie->tob() == tob1) {
                                tob1_occupied = true;
                            }
                            if (topdie->tob() == tob2) {
                                tob2_occupied = true;
                            }
                        }
                    }
                    
                    if (tob1_occupied || tob2_occupied) {
                        debug::debug_fmt("TOB {} or {} is occupied", tob1->coord(), tob2->coord());
                        continue;
                    }
                    
                    debug::debug_fmt("Swap {} on TOB {} and {} on TOB {}", topdie_inst1->name(), tob1->coord(), topdie_inst2->name(), tob2->coord());
                    topdie_inst1->swap_tob_with(topdie_inst2);

                    for (auto net : changed_nets) {
                        new_total_cost += this->net_cost(net) * this->_wirelength_weight;
                    }
 
                    new_total_cost += this->thermal_cost(topdies) * this->_thermal_weight;
                    auto deltaCost = new_total_cost - total_cost;

                    if (deltaCost <= 0) {
                        total_cost = new_total_cost;
                        improved = true;
                    } else {
                        long double exp_x = -1.0 * (deltaCost) / temperature;
                        if (random_f64() <= std::exp(exp_x)) {
                            debug::debug("Accept movement");
                            total_cost = new_total_cost;
                            
                        } else {
                            debug::debug("Reject movement");
                            topdie_inst1->swap_tob_with(topdie_inst2);
                            this->check_nets(topdies);
                        }
                    }
                } else {
                    debug::debug("Move one top-level chip instance to another TOB");
                    auto topdie_inst = this->randomly_choice_one_topdie_insts(topdies);
                    
                    if (!topdie_inst) {
                        debug::debug_fmt("Top-level chip instance is empty");
                        continue;
                    }

                    debug::debug_fmt("Get Top-level chip instance {}", topdie_inst->name());
                    auto prev_tob = topdie_inst->tob();
                    auto next_tob_option = interposer->randomly_get_a_idle_tob();
                   
                    if (!next_tob_option.has_value()) {
                        debug::debug_fmt("No idle TOB");
                        continue; 
                    }
                    auto next_tob = *next_tob_option; 

                    if (this->is_changable(topdie_inst, next_tob) == false) {
                        debug::debug_fmt("TOB {} is not changable", next_tob->coord());
                        continue;
                    }

                    bool next_tob_occupied = false;
                    for (const auto& topdie : topdies) {
                        if (topdie != topdie_inst && topdie->tob() == next_tob) {
                            next_tob_occupied = true;
                            break;
                        }
                    }
                    
                    if (next_tob_occupied) {
                        debug::debug_fmt("TOB {} is occupied", next_tob->coord());
                        continue;
                    }

                    for (auto net : topdie_inst->nets()) {
                        new_total_cost -= this->net_cost(net) * this->_wirelength_weight;
                    }

                    new_total_cost -= this->thermal_cost(topdies) * this->_thermal_weight;
                    topdie_inst->move_to_tob(next_tob);
                    debug::debug_fmt("Move {} from TOB {} to TOB {}", topdie_inst->name(), prev_tob->coord(), next_tob->coord());

                    for (auto net : topdie_inst->nets()) {
                        new_total_cost += this->net_cost(net) * this->_wirelength_weight;
                    }

                    new_total_cost += this->thermal_cost(topdies) * this->_thermal_weight;
                    auto deltaCost = new_total_cost - total_cost;

                    if (deltaCost <= 0) {
                        total_cost = new_total_cost;
                        improved = true;
                    } else {
                        long double exp_x = -1.0 * (deltaCost) / temperature;
                        if (random_f64() <= std::exp(exp_x)) {
                            debug::debug("Accept movement");
                            total_cost = new_total_cost;
                            
                        } else {
                            debug::debug("Reject movement");
                            topdie_inst->move_to_tob(prev_tob);
                            this->check_nets(topdies);
                        }
                    }
                }   
            }

            if (total_cost < best_cost) {
                best_cost = total_cost;
                best_solution = this->save_current_placement(topdies);
                no_improvement_count = 0;
                debug::info_fmt("Find a better solution, cost: {}", best_cost);
            } else {
                no_improvement_count++;
            }

            temperature = this->calculate_next_temperature(temperature, iteration);
            iteration++;
            debug::info_fmt("Iteration {}: Temperature={:.2f}, Current cost={}, optimal cost={}", iteration, temperature, total_cost, best_cost);
        }
        // debug
        this->restore_placement(topdies, best_solution);
        //
        debug::info("Layout completed");
    }

    auto SAPlaceStrategy::is_changable(circuit::TopDieInstance* inst, hardware::TOB* target_tob) const -> bool {
        for (auto& net: inst->nets()) {
            if (net->has_tob_in_ports(target_tob)) {
                return false;
            }
        }
        return true;
    }

    auto SAPlaceStrategy::evaluate_placement(
        hardware::Interposer* interposer,
        const std::Vector<circuit::TopDieInstance*>& topdies,
        circuit::BaseDie* basedie
    ) const -> std::i64 {
        auto nets = this->collect_nets(topdies); 
        auto wirelength = this->total_net_cost(nets);
        // auto congestion = this->congestion_cost(interposer);
        auto thermal = this->thermal_cost(topdies);
        auto power = this->power_cost(topdies, basedie);
        
        // auto total_cost = wirelength * this->_wirelength_weight + congestion * this->_congestion_weight + thermal * this->_thermal_weight + power * this->_power_weight;
        auto total_cost = wirelength * this->_wirelength_weight + thermal * this->_thermal_weight;                    
        debug::info_fmt("Layout evaluation: Line length: {}, thermal distribution: {}, power consumption: {}, total cost: {}", wirelength, thermal, power, total_cost);     
        return total_cost;
    }

    auto SAPlaceStrategy::collect_nets(const std::Vector<circuit::TopDieInstance*>& topdies) const -> std::HashSet<circuit::Net*> {
        auto nets = std::HashSet<circuit::Net*>{};
        for (const auto& topdie_inst : topdies) {
            for (auto net : topdie_inst->nets()) {
                nets.emplace(net);
            }
        }
        return nets;
    }

    auto SAPlaceStrategy::total_net_cost(const std::HashSet<circuit::Net*>& nets) const -> std::i64 {
        std::i64 total = 0;
        for (auto net : nets) {
            total += this->net_cost(net);
        }
        return total;
    }

    auto SAPlaceStrategy::net_cost(circuit::Net* net) const -> std::i64 {
        auto coords = net->coords();
        if (coords.empty()) {
            return 0;
        }

        auto min_row = coords[0].row;
        auto max_row = coords[0].row;
        auto min_col = coords[0].col;
        auto max_col = coords[0].col;

        for (std::usize i = 1; i < coords.size(); ++i) {
            min_row = std::min(min_row, coords[i].row);
            max_row = std::max(max_row, coords[i].row);        
            min_col = std::min(min_col, coords[i].col);
            max_col = std::max(max_col, coords[i].col);
        }
        // Half week long term
        return (max_row - min_row) + (max_col - min_col);
    }

    // auto SAPlaceStrategy::congestion_cost(hardware::Interposer* interposer) const -> std::i64 {
    //     std::i64 cost = 0;
        
    //     // Obtain COB unit
    //     auto& cobs = interposer->cobs();       
    //     // Usage rate of each COB
    //     for (auto& cob : cobs) {
    //         auto usage = cob.second->usage_rate();
    //         if (usage > 0.8) {
    //             cost += static_cast<std::i64>((usage - 0.8) * 100);
    //         }
    //     }
    //     return cost;
    // }

    auto SAPlaceStrategy::thermal_cost(const std::Vector<circuit::TopDieInstance*>& topdies) const -> std::i64 {
        std::i64 cost = 0;
        std::Vector<const circuit::TopDieInstance*> high_power_chips;

        for (const auto& topdie : topdies) {
            double power = this->get_topdie_power(*topdie);

            if (power > this->_thermal_threshold) {
                high_power_chips.push_back(topdie);
            }
        }

        for (std::size_t i = 0; i < high_power_chips.size(); ++i) {
            for (std::size_t j = i + 1; j < high_power_chips.size(); ++j) {
                auto tob1 = high_power_chips[i]->tob();
                auto tob2 = high_power_chips[j]->tob();
                
                if (!tob1 || !tob2) continue;
                auto distance = this->manhattan_distance(tob1->coord(), tob2->coord());
                if (distance < this->_thermal_safe_distance) {
                    cost += (this->_thermal_safe_distance - distance) * 10;
                }
            }
        }       
        return cost;
    }

    auto SAPlaceStrategy::power_cost(
        const std::Vector<circuit::TopDieInstance*>& topdies, 
        circuit::BaseDie* basedie
    ) const -> std::i64 {
        std::i64 cost = 0;
        if (!basedie) return cost;

        auto pose_ports = basedie->pose_ports();
        auto nege_ports = basedie->nege_ports();
        std::Vector<const circuit::TopDieInstance*> high_power_chips;
        for (const auto& topdie : topdies) {
            double power = this->get_topdie_power(*topdie);
            if (power > this->_power_threshold) {
                high_power_chips.push_back(topdie);
            }
        }

        for (const auto& chip : high_power_chips) {
            auto tob = chip->tob();
            if (!tob) continue;
            
            auto chip_coord = tob->coord();
            std::i64 min_pose_distance = std::numeric_limits<std::i64>::max();
            for (const auto& port : pose_ports) {
                // auto track = port.coord();
                auto track = port;
                auto distance = std::abs(track.row - chip_coord.row) + std::abs(track.col - chip_coord.col);
                min_pose_distance = std::min(min_pose_distance, distance);
            }

            std::i64 min_nege_distance = std::numeric_limits<std::i64>::max();
            for (const auto& port : nege_ports) {
                // auto track = port.coord();
                auto track = port;
                auto distance = std::abs(track.row - chip_coord.row) + std::abs(track.col - chip_coord.col);
                min_nege_distance = std::min(min_nege_distance, distance);
            }

            if (min_pose_distance > this->_power_safe_distance) {
                cost += (min_pose_distance - this->_power_safe_distance) * 5;
            }
            
            if (min_nege_distance > this->_power_safe_distance) {
                cost += (min_nege_distance - this->_power_safe_distance) * 5;
            }
        }      
        return cost;
    }

    auto SAPlaceStrategy::manhattan_distance(
        const hardware::TOBCoord& coord1, 
        const hardware::TOBCoord& coord2
    ) const -> std::i64 {
        return std::abs(coord1.row - coord2.row) + std::abs(coord1.col - coord2.col);
    }

    auto SAPlaceStrategy::save_current_placement(const std::Vector<circuit::TopDieInstance*>& topdies) const 
        -> std::HashMap<circuit::TopDieInstance*, hardware::TOB*> {
        std::HashMap<circuit::TopDieInstance*, hardware::TOB*> placement;
        for (const auto& topdie : topdies) {
            placement[topdie] = topdie->tob();
        }
        return placement;
    }

    auto SAPlaceStrategy::restore_placement(
        std::Vector<circuit::TopDieInstance*>& topdies,
        const std::HashMap<circuit::TopDieInstance*, hardware::TOB*>& placement
    ) const -> void {
        auto tob_to_topdie = std::HashMap<hardware::TOB*, circuit::TopDieInstance*>{};
        for (auto* topdie : topdies) {
            tob_to_topdie.emplace(topdie->tob(), topdie);
        }

        for (auto* topdie : topdies) {
            auto it = placement.find(topdie);
            if (it == placement.end()) {
                throw std::runtime_error(std::format("TopDieInstance {} not found in placement", topdie->name()));
            }
            auto target_tob = it->second;
            if (!target_tob) {
                throw std::runtime_error(std::format("TopDieInstance {} target TOB is null", topdie->name()));
            }
            if (topdie->tob() == target_tob) {
                continue;
            }

            auto occ_it = tob_to_topdie.find(target_tob);
            if (occ_it != tob_to_topdie.end() && occ_it->second != topdie) { // target_tob has already have a topdie
                auto* other = occ_it->second;
                auto* tob1 = topdie->tob();
                auto* tob2 = other->tob();
                topdie->swap_tob_with(other);
                tob_to_topdie[tob1] = other;
                tob_to_topdie[tob2] = topdie;
            } else {
                auto* prev_tob = topdie->tob();
                topdie->move_to_tob(target_tob);
                tob_to_topdie.erase(prev_tob);
                tob_to_topdie.emplace(target_tob, topdie);
            }
        }
    }

    auto SAPlaceStrategy::randomly_choice_one_topdie_insts(
        std::Vector<circuit::TopDieInstance*>& topdies
    ) const -> circuit::TopDieInstance* {
        if (topdies.empty()) {
            return nullptr;
        }
        auto index = random_i64(0, topdies.size());
        return topdies.at(index);
    }

    auto SAPlaceStrategy::randomly_choice_two_topdie_insts(
        std::Vector<circuit::TopDieInstance*>& topdies
    ) const -> std::Tuple<circuit::TopDieInstance*, circuit::TopDieInstance*> {
        if (topdies.size() < 2) {
            debug::warning("Insufficient number of chips for exchange");
            return {nullptr, nullptr};
        }
        
        auto topdie_inst1 = this->randomly_choice_one_topdie_insts(topdies);
        circuit::TopDieInstance* topdie_inst2 = nullptr;
        do {
            topdie_inst2 = this->randomly_choice_one_topdie_insts(topdies);
        } while (topdie_inst2 == topdie_inst1);
        return { topdie_inst1, topdie_inst2 };
    }

    auto SAPlaceStrategy::decide_to_swap_topdie_inst(double temperature) const -> bool {
        double swap_prob = 0.7 - 0.2 * (this->_init_temperature - temperature) / this->_init_temperature;
        return random_f64() < swap_prob;
    }

    auto SAPlaceStrategy::random_f64() const -> double {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        
        std::uniform_real_distribution<double> dis(0.0, 1.0);
        return dis(gen);
    }

    auto SAPlaceStrategy::calculate_next_temperature(double current_temp, std::size_t iteration) const -> double {
        return current_temp * this->_cooling_rate;
    }

    // auto SAPlaceStrategy::try_routing(
    //     hardware::Interposer* interposer,
    //     circuit::BaseDie* basedie,
    //     const RouteStrategy& route_strategy
    // ) const -> std::i64 {
    //     try {
    //         return route_nets(interposer, basedie, route_strategy);
    //     } catch (const std::exception& e) {
    //         debug::warning_fmt("Wiring evaluation failed: {}", e.what());
    //         return std::numeric_limits<std::i64>::max(); 
    //     }
    // }

    auto SAPlaceStrategy::is_valid_placement(
        hardware::Interposer* interposer,
        const std::Vector<circuit::TopDieInstance*>& topdies
    ) const -> bool {
        for (const auto& topdie : topdies) {
            if (!topdie->tob()) {
                debug::error_fmt("Chip {} has not been assigned a TOB", topdie->name());
                return false;
            }
        }

        std::HashSet<hardware::TOB*> used_tobs;
        for (const auto& topdie : topdies) {
            auto tob = topdie->tob();
            if (used_tobs.contains(tob)) {
                debug::error_fmt("TOB {} is occupied by multiple chips", tob->coord());
                return false;
            }
            used_tobs.insert(tob);
        }

        auto interposer_rows = hardware::Interposer::TOB_ARRAY_HEIGHT;
        auto interposer_cols = hardware::Interposer::TOB_ARRAY_WIDTH;
        
        for (const auto& topdie : topdies) {
            auto tob = topdie->tob();
            auto coord = tob->coord();
            
            if (coord.row < 0 || coord.row >= interposer_rows || 
                coord.col < 0 || coord.col >= interposer_cols) {
                debug::error_fmt("Chip {} is located outside the Interposer boundary: {}", 
                    topdie->name(), coord);
                return false;
            }
        }   
        return true;
    }

    auto SAPlaceStrategy::get_topdie_power(const circuit::TopDieInstance& topdie) const -> double {
        auto nets = topdie.nets();
        return static_cast<double>(nets.size()) / 10.0; 
    }

    auto SAPlaceStrategy::check_nets(
        const std::Vector<circuit::TopDieInstance*>& topdies
    ) const -> void {
        auto nets = std::HashSet<circuit::Net*>{};
        std::usize suspicious_count {0};

        for (auto* td : topdies) {
            for (auto* net : td->nets()) {
                if (auto* syncn = dynamic_cast<circuit::SyncNet*>(net)) {
                    for (auto& r : syncn->btbnets()) {
                        auto* b = r.get();
                        auto* bb = b->begin_bump();
                        auto* eb = b->end_bump();
                        if (bb && eb && bb->tob()->coord() == eb->tob()->coord()) {
                            ++suspicious_count;
                            debug::warning_fmt(
                                "Placement driver: Sync BumpToBumpNet {} on same TOB {}, begin {}, end {}",
                                b->name(), bb->tob()->coord(), bb->coord(), eb->coord()
                            );
                        }
                    }
                } else if (auto* btb = dynamic_cast<circuit::BumpToBumpNet*>(net)) {
                    auto* bb = btb->begin_bump();
                    auto* eb = btb->end_bump();
                    if (bb && eb && bb->tob()->coord() == eb->tob()->coord()) {
                        ++suspicious_count;
                        debug::warning_fmt(
                            "Placement driver: BumpToBumpNet {} on same TOB {}, begin {}, end {}",
                            btb->name(), bb->tob()->coord(), bb->coord(), eb->coord()
                        );
                    }
                }   
            }
        }
        
        if (suspicious_count > 0) {
            debug::warning_fmt("Placement driver: {} suspicious BumpToBump nets found on same TOB", suspicious_count);
        } else {
            debug::info("Placement driver: no suspicious BumpToBump nets detected");
        }
    }
}
