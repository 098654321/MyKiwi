#pragma once

#include <algo/router/command_mode/command.hh>


namespace kiwi::algo {

class Incre_route: public Command {
public:
    Incre_route() = default;
    ~Incre_route() = default;

public:
    auto execute(hardware::Interposer*, RouteEngine&) const -> void override;
    auto to_string() const -> const std::String override;

private:
    auto iterate_routing(hardware::Interposer*, RouteEngine&, std::Vector<circuit::Net*>&, HardwareRecorder&) const -> bool;
    auto reset(RouteEngine&, std::Vector<circuit::Net*>&) const -> void;
    auto set_history_as_current(std::Vector<circuit::Net*>&) const -> void;
    auto sort_incre(std::Vector<circuit::Net*>&) const -> std::Vector<circuit::Net*>;
};

}