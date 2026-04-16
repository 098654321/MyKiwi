#pragma once

#include <serde/de.hh>
#include <std/string.hh>

namespace PR_tool::parse {

    struct ConnectionConfig {
        std::String input;
        std::String output;
    };

}

DESERIALIZE_STRUCT(PR_tool::parse::ConnectionConfig,
    DE_FILED(input)
    DE_FILED(output)
)

