#ifndef TXT2JSON_HH
#define TXT2JSON_HH

#include <parse/reader/config/config.hh>
#include <global/std/file.hh>
#include <global/std/collection.hh>
#include <global/std/integer.hh>
#include <serde/ser.hh>
#include <serde/json/json.hh>
#include <hardware/tob/tobcoord.hh>
#include <hardware/track/trackcoord.hh>
#include <global/debug/debug.hh>


// Implement Serialize specializations locally since they are missing in headers
namespace kiwi::serde {

    // Basic types
    template <> struct Serialize<Json, bool> {
        static void to(Json& j, const bool& v) { j = Json::boolean(v); }
    };
    template <> struct Serialize<Json, int> {
        static void to(Json& j, const int& v) { j = Json::integer(v); }
    };
    template <> struct Serialize<Json, std::usize> {
        static void to(Json& j, const std::usize& v) { j = Json::integer(static_cast<int>(v)); }
    };
    template <> struct Serialize<Json, std::i64> {
        static void to(Json& j, const std::i64& v) { j = Json::integer(static_cast<int>(v)); }
    };
    template <> struct Serialize<Json, std::String> {
        static void to(Json& j, const std::String& v) { j = Json::string(v); }
    };

    // Containers
    template <typename V>
    struct Serialize<Json, std::Vector<V>> {
        static void to(Json& j, const std::Vector<V>& v) {
            j = Json::array();
            for (const auto& item : v) {
                Json val;
                Serialize<Json, V>::to(val, item);
                j.push(val);
            }
        }
    };

    template <typename V>
    struct Serialize<Json, std::HashMap<std::String, V>> {
        static void to(Json& j, const std::HashMap<std::String, V>& map) {
            j = Json::object();
            // To ensure deterministic output order if needed, we might want to sort keys.
            // But HashMap is unordered. The requirement doesn't specify order, but JSON objects are unordered.
            // However, typical JSON serializers might not sort.
            // Let's just iterate.
            for (const auto& [key, val] : map) {
                Json json_val;
                Serialize<Json, V>::to(json_val, val);
                j.insert(key, json_val);
            }
        }
    };
    
    // Config structs
    SERIALIZE_STRUCT(kiwi::parse::TopDieConfig,
        SER_FEILD(pin_map)
    )

    SERIALIZE_STRUCT(kiwi::hardware::Coord,
        SER_FEILD(row)
        SER_FEILD(col)
    )
    
    // TrackDirection Enum
    template <>
    struct Serialize<Json, kiwi::hardware::TrackDirection> {
        static void to(Json& sr, const kiwi::hardware::TrackDirection& d) {
            if (d == kiwi::hardware::TrackDirection::Horizontal) sr = Json::string("hori");
            else sr = Json::string("vert");
        }
    };

    SERIALIZE_STRUCT(kiwi::hardware::TrackCoord,
        SER_FEILD(row)
        SER_FEILD(col)
        SER_FEILD(dir)
        SER_FEILD(index)
    )

    SERIALIZE_STRUCT(kiwi::parse::TopdieInstConfig,
        SER_FEILD(topdie)
        SER_FEILD(coord)
    )

    SERIALIZE_STRUCT(kiwi::parse::ExternalPortConfig,
        SER_FEILD(coord)
    )
}

namespace kiwi::parse {

    void txt2json(const std::FilePath& config_folder, const std::FilePath& json_folder, int mode) {
        auto config = load_config(config_folder, mode);
        
        // 1. topdies.json
        debug::info("load topdies.json");
        {
            kiwi::serde::Json j;
            kiwi::serde::Serialize<kiwi::serde::Json, decltype(config.topdies)>::to(j, config.topdies);
            std::OutFile file(json_folder / "topdies.json");
            file << j.to_string();
        }

        // 2. topdie_insts.json
        debug::info("load topdie_insts.json");
        {
            kiwi::serde::Json j;
            kiwi::serde::Serialize<kiwi::serde::Json, decltype(config.topdie_insts)>::to(j, config.topdie_insts);
            std::OutFile file(json_folder / "topdie_insts.json");
            file << j.to_string();
        }

        // 3. external_ports.json
        debug::info("load external_ports.json");
        {
            kiwi::serde::Json j;
            kiwi::serde::Serialize<kiwi::serde::Json, decltype(config.external_ports)>::to(j, config.external_ports);
            std::OutFile file(json_folder / "external_ports.json");
            file << j.to_string();
        }

        // 4. connections.json
        debug::info("load connections.json");
        {
            kiwi::serde::Json j = kiwi::serde::Json::object();
            
            for(const auto& [mode, inner_map] : config.connections) {
                 for(const auto& [group_index, vec] : inner_map) {
                     kiwi::serde::Json arr = kiwi::serde::Json::array();
                     for(const auto& conn : vec) {
                         kiwi::serde::Json pair = kiwi::serde::Json::array();
                         pair.push(kiwi::serde::Json::string(conn.input));
                         pair.push(kiwi::serde::Json::string(conn.output));
                         arr.push(pair);
                     }
                     j.insert(std::to_string(group_index), arr);
                 }
            }
            std::OutFile file(json_folder / "connections.json");
            file << j.to_string();
        }

    }

}


int main() {
    kiwi::debug::initial_log("./debug.log");

    for (int i = 8; i <= 15; i++) {
        std::FilePath config_folder = "../test/config/case" + std::to_string(i);
        std::FilePath json_folder = "./json/case" + std::to_string(i);
        kiwi::parse::txt2json(config_folder, json_folder, 0);
    }
    
    return 0;
}

#endif  // TXT2JSON_HH
