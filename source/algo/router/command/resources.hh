#pragma once

#include <algo/router/command.hh>

namespace kiwi::algo {

class Resources: public Command {
public:
    Resources() = default;
    ~Resources() = default;

public:
    auto execute(hardware::Interposer*, RouteEngine&, const RouteStrategy&) const -> void override;
    auto to_string() const -> const std::String override;
    
};

}

