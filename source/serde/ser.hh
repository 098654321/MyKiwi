#pragma once

namespace PR_tool::serde {

    #define SER_FEILD(f) sr.at(#f).set_to(c.f);
    #define SER_FEILD_AS(f, n) sr.at(n).set_to(c.f);

    #define SERIALIZE_STRUCT(Struct, ...)\
    template <typename Serializer>\
    struct PR_tool::serde::Serialize<Serializer, Struct> {\
        static void to(Serializer& sr, const Struct& c) {\
            sr = Serializer::object();\
            __VA_ARGS__\
        }\
    };

    template <typename Serializer, typename Value>
    struct Serialize {
        static void to(Serializer& sr, const Value& value) { static_assert(false, "Unimple"); }
    };

    template <typename Serializer, typename Value>
    auto serialize(Serializer& sr, const Value& value) -> void {
        Serialize<Serializer, Value>::to(sr, value);
    }

}