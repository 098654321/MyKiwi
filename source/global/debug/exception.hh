#pragma once

#include <std/string.hh>

namespace PR_tool {

    struct Exception {
        virtual auto what() const -> std::String = 0;
        virtual ~Exception() noexcept {};
    };

}