#pragma once

#include <algo/router/command_mode/command.hh>

namespace kiwi::algo {

class Route: public Command {
public:
    Route();
    ~Route() = default;

public:
    auto execute(hardware::Interposer*, RouteEngine&) const -> void override;
    auto to_string() const -> const std::String override;
    
};

}