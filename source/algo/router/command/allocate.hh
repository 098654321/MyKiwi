#pragma once

#include <algo/router/command.hh>


namespace kiwi::algo {

class Allocate: public Command {
public:
    Allocate() = default;
    ~Allocate() = default;

public:
    auto execute(hardware::Interposer*, RouteEngine&) const -> void override;
    auto to_string() const -> const std::String override;
};

}