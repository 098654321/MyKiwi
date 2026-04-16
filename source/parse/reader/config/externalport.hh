#pragma once

#include "serde/de.hh"
#include <hardware/track/trackcoord.hh>

namespace PR_tool::parse {

    struct ExternalPortConfig {
        hardware::TrackCoord coord;
    };
    
}

DESERIALIZE_STRUCT(PR_tool::parse::ExternalPortConfig,
    DE_FILED(coord)
)