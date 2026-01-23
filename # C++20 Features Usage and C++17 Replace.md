# C++20 Features Usage and C++17 Replacements

This document lists the C++20 features found in `source/` and `test/` directories and provides C++17 compliant replacements.

## 1. `std::format` and `<format>`

**Description**:
The `<format>` header and `std::format` function provide a type-safe and extensible formatting library (similar to Python's f-strings). This is the most heavily used C++20 feature in the project.

**Locations**:
Direct usage or inclusion of `<format>` found in:
- `source/global/std/format.hh` (Wrapper header, extensively used)
- `source/global/debug/debug.hh` (Uses `std::FormatString` via wrapper)
- `source/global/debug/debug.cc` (Uses `std::format` for logging)
- `source/algo/router/incremental/recorders/recorder.hh`
- `source/algo/router/incremental/recorders/tobrecorder.cc`
- `source/algo/router/incremental/recorders/hardware_recorder.cc`
- `source/algo/router/incremental/bound_bits/track_bits.cc`
- `source/algo/router/incremental/bound_bits/bits_group.hh`
- `source/algo/router/common/maze/mazererouter.cc`
- `source/algo/router/common/maze/path_length.cc`
- `source/algo/router/command_mode/invoker.cc`
- `source/algo/router/common/allocate/hopcroft_karp.cc`
- `source/circuit/path/pathpackage.cc`
- `source/circuit/net/types/bbsnet.cc`
- `source/circuit/connection/connection.hh`
- `source/circuit/basedie.cc`
- `source/hardware/tob/tobmux.cc`
- `source/hardware/tob/tob.cc`
- `source/hardware/bump/bumpcoord.cc`
- `source/global/utility/file.cc`
- `source/parse/writer/registers/tobregister.cc`
- `source/parse/writer/writer.hh`
- `test/regression_test/incremental_test.cc`

Indirect usage via `debug::*_fmt` macros/functions found in:
- `test/module_test/test.cc`
- `test/module_test/test_router.cc`
- `test/module_test/test_placer.cc`
- `test/module_test/test_interposer.cc`
- `test/module_test/test_debug.cc`

**C++17 Replacement**:
Use `std::stringstream`, `snprintf`, or the open-source `fmt` library (which `std::format` is based on and is C++11 compatible). For a standard-library-only solution:

*Example 1: Basic Formatting*
```cpp
// C++20
#include <format>
std::string msg = std::format("Value: {}, Index: {}", val, idx);

// C++17
#include <sstream>
std::stringstream ss;
ss << "Value: " << val << ", Index: " << idx;
std::string msg = ss.str();
```

*Example 2: Logging (Debug Wrappers)*
Modify `source/global/debug/debug.hh` to use `std::stringstream` internally instead of `std::format`.

## 2. `std::ranges` and `std::views` (specifically `iota`)

**Description**:
The `<ranges>` header and `std::views` namespace provide composable view adaptors and range-based algorithms. The project mainly uses `std::views::iota` for creating iteration ranges.

**Locations**:
Direct usage or inclusion of `<ranges>` found in:
- `source/global/std/range.hh` (Wrapper header)
- `source/algo/router/incremental/recorders/tobrecorder.cc` (Uses `std::views::iota`)
- `source/algo/router/incremental/recorders/hardware_recorder.cc`
- `source/algo/router/incremental/recorders/cobrecorder.cc`
- `source/algo/router/incremental/bound_bits/tob_bits.cc`
- `source/algo/router/incremental/bound_bits/cob_bits.cc`
- `source/algo/router/incremental/maze/routing.cc`
- `source/hardware/tob/tobmux.cc`
- `source/hardware/tob/tob.cc`
- `source/hardware/cob/cob.cc`
- `source/widget/view2d/detail/cob.cc`
- `source/parse/reader/controlbits/controlbits.cc`

**C++17 Replacement**:
Use standard `for` loops.

*Example: Loop over a range*
```cpp
// C++20
#include <ranges>
for (auto i : std::views::iota(0, size)) {
    // ...
}

// C++17
for (int i = 0; i < size; ++i) {
    // ...
}
```

## 3. `std::span`

**Description**:
`std::span` (from `<span>`) is a non-owning view of a contiguous sequence of objects (like an array or vector).

**Locations**:
- `source/global/std/collection.hh` (Includes `<span>`)
- `source/serde/json/json.hh` (Uses `std::span` in API)
- `source/serde/json/json.cc`

**C++17 Replacement**:
Pass a pointer and size, or use a `const std::vector&` if the underlying data is known to be a vector.

*Example: Returning a view of an array*
```cpp
// C++20
std::span<const Json> as_array() const;

// C++17
// Option A: Return const reference to vector (if the container is a vector)
const std::vector<Json>& as_array() const;

// Option B: Return pointer and size
// (Could use a custom simple struct or std::pair)
struct JsonSpan {
    const Json* ptr;
    size_t size;
    const Json& operator[](size_t i) const { return ptr[i]; }
};
JsonSpan as_array() const;
```