#include "./bits_group.hh"
#include <hardware/tob/tob.hh>


namespace kiwi::algo {

inline constexpr std::usize BumpMuxNum = 16;
inline constexpr std::usize BumpMuxSize = 8;
inline constexpr std::usize HoriMuxNum = 16;
inline constexpr std::usize VertMuxNum = 2;
inline constexpr std::usize VertMuxSize = 32;

class TOBGroup {

public:
    TOBGroup() : _bump_groups{}, _hori_groups{}, _vert_groups{} {}
    ~TOBGroup() = default;

public:
    auto bump_to_hori_info(std::usize index) const -> std::Tuple<std::usize, std::usize>;
    auto hori_to_vert_info(std::usize index) const -> std::Tuple<std::usize, std::usize>;
    auto vert_to_track_info(std::usize index) const -> std::Tuple<std::usize, std::usize>;

    auto record_tobmux(const hardware::TOBConnector& connector, bool reuse_type) -> void;

    auto to_string() const -> std::String;
    auto info() const -> std::Tuple<std::usize, std::usize>;

private:
    std::Array<BitsGroup<BumpMuxSize>, BumpMuxNum> _bump_groups;
    std::Array<BitsGroup<BumpMuxSize>, HoriMuxNum> _hori_groups;
    std::Array<BitsGroup<VertMuxSize>, VertMuxNum> _vert_groups;
};

class  GlobalTOBGroup {
public:
    GlobalTOBGroup() : _tob_groups{} {}
    ~GlobalTOBGroup() = default;

public:
    auto tob_group(const hardware::TOBCoord& coord) -> TOBGroup&;
    auto record_tob(const hardware::TOBCoord& coord, const hardware::TOBConnector& connector, bool reuse_type) -> void;

    auto show() const -> void;
    auto info() const -> std::Tuple<std::usize, std::usize>;

private:
    std::HashMap<hardware::TOBCoord, TOBGroup> _tob_groups;
};

}



