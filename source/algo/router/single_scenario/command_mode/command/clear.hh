#pragma once

#include <algo/router/single_scenario/command_mode/command.hh>


namespace kiwi::algo {

class Clear: public Command {
public:
    Clear() = default;
    ~Clear() = default;

public:
    auto execute(hardware::Interposer*, RouteEngine&) const -> void override;
    auto to_string() const -> const std::String override;
};

}