#pragma once

#include <algo/router/command.hh>


namespace kiwi::algo {

class Sort: public Command {
public:
    Sort() = default;
    ~Sort() = default;

public:
    auto execute(hardware::Interposer*, RouteEngine&, const RouteStrategy&) const -> void override;
    auto to_string() const -> const std::String override;
    
};

}