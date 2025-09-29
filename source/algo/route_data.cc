#include <debug/debug.hh>
#include "route_data.hh"
#include <exception>


namespace kiwi::algo {

auto RouteData::collect_net_length(const std::Vector<circuit::Net*>& nets) const -> std::Tuple<float, std::usize, float, std::usize> {
    float total_length {0};
    std::usize sync_net_number {0};
    float sync_length {0};
    std::usize failed_net {0};
    
    for (const auto& net: nets) {
        auto l = net->length();

        if (l > 0) {
            total_length += l;
            auto [n, l] = net->sync_length();
            sync_net_number += n;
            sync_length += l;
        }
        else {
            failed_net++;
        }
    }   

    return std::Tuple<float, std::usize, float, std::usize>{total_length, sync_net_number, sync_length, failed_net};
}


auto RouteData::collect_global_bits(const std::Vector<circuit::Net*>& nets) const -> GlobalBoundBits {
    GlobalBoundBits bits {};
    for (const auto& net: nets) {
        auto type = net->reuse_type();
        if (!type.has_value()) {
            throw std::logic_error("show_bits(): net reuse type should not be nullopt when routing");
        }

        for (auto& [track, cob_connector]: net->pathpackage()._regular_path) {
            bits.record_track(track->coord(), *type);
            if (cob_connector.has_value()) {
                bits.record_cob(*cob_connector, *type);
            }
        }
        for (auto& [bump, connector, track]: net->pathpackage()._tob_to_track) {
            bits.record_tob(bump->tob()->coord(), connector, *type);
        }
        for (auto& [bump, connector, track]: net->pathpackage()._track_to_tob) {
            bits.record_tob(bump->tob()->coord(), connector, *type);
        }
    }

    return bits;
}


auto RouteData::collect_data(const std::Vector<circuit::Net*>& nets, bool incre) -> DataPerCycle {
    float monopolized_rate {0.0}, mixed_rate {0.0};

    if (incre) {
        auto global_bits = this->collect_global_bits(nets);
        auto rate_tuple = global_bits.get_rate();
        monopolized_rate = std::get<0>(rate_tuple);
        mixed_rate = std::get<1>(rate_tuple);
    }

    auto [total_length, sync_net_number, sync_length, failed_net] = this->collect_net_length(nets);
    auto ave_sync_length = sync_length / sync_net_number;

    return DataPerCycle(total_length, ave_sync_length, std::make_tuple(monopolized_rate, mixed_rate), sync_net_number, failed_net);
}


auto RouteData::collect_data_in_cycle(std::usize cycle, const std::Vector<circuit::Net*>& nets) -> void {
    auto datapercycle = this->collect_data(nets, true);
    this->_data.emplace(cycle, datapercycle);
}


auto RouteData::collect_data_in_cycle(std::usize cycle, const DataPerCycle& data) -> void {
    this->_data.emplace(cycle, data);
}


auto RouteData::show_data_in_cycle(std::usize cycle, bool incre) const -> void {
try {
    auto& data = this->_data.at(cycle);

    if (incre) {
        auto [monopolized_rate, mixed_rate] = data._reg_data;
        debug::info_fmt("\n\
        --------------------------------Cycle {}--------------------------------\n\
        Total Length: {}\n\
        Sync Net Number: {}\n\
        Average Sync Length: {}\n\
        Monopolized Rate: {}%\n\
        Mixed Rate: {}%\n\
        Failed routing nubmer: {}\n\
        ------------------------------------------------------------------------\
        ", cycle, data._total_length, data._sync_net_number, data._ave_sync_length, 100*monopolized_rate, 100*mixed_rate, data._failed_net
        );
    }
    else {
        debug::info_fmt("\n\
        ------------------------------------------------------------------------\n\
        Total Length: {}\n\
        Sync Net Number: {}\n\
        Average Sync Length: {}\n\
        Failed routing nubmer: {}\n\
        ------------------------------------------------------------------------\
        ", data._total_length, data._sync_net_number, data._ave_sync_length, data._failed_net
        );
    }
}
catch (std::exception& e) {
    throw std::runtime_error(std::format("RouteData::show_data_in_cycle({}): {}", cycle, e.what()));
}
}


auto RouteData::clear_data() -> void {
    this->_data.clear();
}

}

