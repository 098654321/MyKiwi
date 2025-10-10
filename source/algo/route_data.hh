#pragma once

#include <std/collection.hh>
#include <std/integer.hh>
#include <circuit/net/nets.hh>
#include <circuit/net/net.hh>
#include <algo/router/incremental/bound_bits/global_bits.hh>


namespace kiwi::algo {

    struct DataPerCycle {
        DataPerCycle(float total_length, float ave_sync_length, std::Tuple<double, double> reg_data, std::usize sync_net_number, std::usize failed_net) :
            _total_length(total_length),
            _ave_sync_length(ave_sync_length),
            _reg_data(reg_data),
            _sync_net_number(sync_net_number),
            _failed_net(failed_net)
        {}
        ~DataPerCycle() noexcept = default;

        float _total_length;
        float _ave_sync_length;
        std::usize _sync_net_number;
        std::usize _failed_net;
        std::Tuple<double, double> _reg_data;
    };


    class RouteData {
    public:
        RouteData() : _data{} {};
        ~RouteData() noexcept = default;
        
    public:
        // get tuple<total_length, sync_net_number, sync_net_length, failed_net>
        auto collect_net_length(const std::Vector<circuit::Net*>& nets) const -> std::Tuple<float, std::usize, float, std::usize>;
        // collect global bits info
        auto collect_global_bits(const std::Vector<circuit::Net*>& nets) const -> GlobalBoundBits;
        auto collect_data(const std::Vector<circuit::Net*>& nets, bool incre) -> DataPerCycle;
        // construct DataPerCycle in cycle
        auto collect_data_in_cycle(std::usize cycle, const std::Vector<circuit::Net*>& nets) -> void;
        auto collect_data_in_cycle(std::usize cycle, const DataPerCycle& data) -> void;
        auto clear_data() -> void;
        auto show_data_in_cycle(std::usize cycle, bool incre) const -> void;

    public:
        auto data() const -> const std::unordered_map<std::usize, DataPerCycle>& { return _data; }

    private:
        std::unordered_map<std::usize, DataPerCycle> _data;
    };

}



