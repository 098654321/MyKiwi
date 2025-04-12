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
};

}