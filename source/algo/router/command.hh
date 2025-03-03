#pragma once

#include <global/std/collection.hh>
#include <algo/router/routestrategy.hh>
#include <global/std/memory.hh>
#include "./routeengine.hh"


namespace kiwi::hardware {
    class Interposer;
}

namespace kiwi::circuit {
    class BaseDie;
}


namespace kiwi::algo {

class Command {
public:
    Command(): _remediation() {}
    virtual ~Command() = default;

public:
    virtual auto execute(hardware::Interposer*, RouteEngine&, const RouteStrategy&) const -> void = 0;
    virtual auto to_string() const -> const std::String = 0;
    virtual auto remediation() const -> const std::Vector<std::Rc<Command>>& {
        return this->_remediation;
    }

protected:
    std::Vector<std::Rc<Command>> _remediation;    // execute from front to back
};

    
}