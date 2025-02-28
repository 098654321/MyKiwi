#pragma once

#include <algo/router/command.hh>

namespace kiwi::algo {

class Reroute: public Command {
public:
    Reroute() = default;
    ~Reroute() = default;

public:
    auto execute(hardware::Interposer*, RouteEngine&, const RouteStrategy&) const -> void override;
    auto to_string() const -> const std::String override;
    
};

}