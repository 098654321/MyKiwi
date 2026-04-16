#include <debug/debug.hh>
#include "route_data.hh"
#include <exception>
#include <algorithm>


namespace PR_tool::algo {

auto RouteData::collect_net_length(const std::Vector<circuit::Net*>& nets) const -> std::Tuple<float, std::usize, float, std::usize, std::usize> {
    float total_length {0};
    std::usize sync_net_number {0};
    float sync_length {0};
    std::usize failed_net {0};
    std::usize max_length {0};
    
    for (const auto& net: nets) {
        auto l = net->length();

        if (l > 0) {
            total_length += l;
            auto [n, l] = net->sync_length();       // non_sync_net: return [0, 0]
            sync_net_number += n;
            sync_length += l;

            if (n == 0) {                           // non_sync_net
                max_length = std::max(max_length, l);
            }
            else {                                  // sync_net
                max_length = std::max(max_length, l/n);
            }
        }
        else {
            failed_net++;
        }
    }   

    return std::Tuple<float, std::usize, float, std::usize, std::usize>{total_length, sync_net_number, sync_length, max_length, failed_net};
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
    double monopolized_by_reuse {0.0}, has_nonreuse {0.0};

    if (incre) {
        auto global_bits = this->collect_global_bits(nets);
        auto reg_rate = global_bits.get_rate();
        std::tie(monopolized_by_reuse, has_nonreuse) = reg_rate;
    }

    auto [total_length, sync_net_number, sync_length, max_length, failed_net] = this->collect_net_length(nets);
    auto ave_sync_length = sync_length / sync_net_number;

    return DataPerCycle(total_length, ave_sync_length, max_length, std::make_tuple(monopolized_by_reuse, has_nonreuse), sync_net_number, failed_net);
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
        auto [monopolized_by_reuse, has_nonreuse] = data._reg_data;
        auto sum = monopolized_by_reuse + has_nonreuse;
        debug::info_fmt("\n\
        --------------------------------Cycle {}--------------------------------\n\
        Total Length: {}\n\
        Sync Net Number: {}\n\
        Average Sync Length: {}\n\
        Max Length: {}\n\
        Monopolized by Reuse: {}({}%)\n\
        Has Nonreuse: {}({}%)\n\
        Failed routing nubmer: {}\n\
        ------------------------------------------------------------------------\
        ", cycle, data._total_length, data._sync_net_number, data._ave_sync_length, data._max_length, monopolized_by_reuse, 100*monopolized_by_reuse/sum, has_nonreuse, 100*has_nonreuse/sum, data._failed_net
        );
    }
    else {
        debug::info_fmt("\n\
        ------------------------------------------------------------------------\n\
        Total Length: {}\n\
        Sync Net Number: {}\n\
        Average Sync Length: {}\n\
        Max Length: {}\n\
        Failed routing nubmer: {}\n\
        ------------------------------------------------------------------------\
        ", data._total_length, data._sync_net_number, data._ave_sync_length, data._max_length, data._failed_net
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

