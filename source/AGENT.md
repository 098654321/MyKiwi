# Kiwi Project Source Code Documentation

This document provides an overview of the `source/` directory for AI agents. The Kiwi project appears to be an EDA (Electronic Design Automation) tool focused on 2.5D/3D IC design, specifically handling interposer-based designs, placement, and routing.

## Directory Structure

*   **`algo/`**: Core algorithms for placement and routing.
    *   **`netbuilder/`**: Logic for building `Net` objects from raw connections.
    *   **`placer/`**: Placement algorithms.
        *   `sa/`: Simulated Annealing implementation for placement optimization.
        *   `place.cc/hh`: Main placement entry point.
    *   **`router/`**: Comprehensive routing logic.
        *   `command_mode/`: Implementation of interactive routing commands (e.g., `allocate`, `reroute`, `clear`).
        *   `common/`: Shared routing utilities.
            *   `allocate/`: Resource allocation algorithms (e.g., Hopcroft-Karp for bipartite matching).
            *   `maze/`: Maze routing strategy (A* search, path length calculation).
        *   `incremental/`: Incremental routing specific logic.
            *   `bound_bits/`: Management of control bits for hardware configuration.
            *   `recorders/`: Recorders for tracking hardware usage during incremental updates.
        *   `route_nets.cc`: Main entry function for routing nets.
        *   `routeengine.cc`: Core engine driving the routing process.

*   **`app/`**: Application entry points.
    *   `cli/`: Command Line Interface implementation (`cli.cc`).
    *   `gui/`: Graphical User Interface entry point (`gui.cc`).
    *   `main.cc`: The executable's main entry point, handling argument parsing and mode selection.

*   **`circuit/`**: Logical circuit model and data structures.
    *   `connection/`: Definitions for `Pin` and `Connection`.
    *   `net/`: Abstract `Net` class and its concrete implementations:
        *   `types/`: `BumpToBumpNet`, `SyncNet`, `BumpToTrackNet`, etc.
    *   `topdie/`: Logical definition of a Top Die (chiplet).
    *   `topdieinst/`: Instance of a Top Die placed on the interposer.
    *   `basedie.cc/hh`: Base die (interposer) logical management.
    *   `export/`: Logic for exporting design data.
    *   `path/`: Data structures for routing paths (`PathPackage`).

*   **`global/`**: Cross-cutting concerns and standard library wrappers.
    *   **`debug/`**: centralized logging and assertion system.
        *   `debug.hh`: Interface for logging (`info`, `debug`, `error`) and formatted output.
        *   `console.hh`: Console output handling.
    *   **`std/`**: **CRITICAL**: Custom wrappers around C++ Standard Library types.
        *   Always use `kiwi::std::Vector`, `kiwi::std::String`, `kiwi::std::HashMap` etc., instead of raw `std::` types to ensure consistency and potential custom allocator support.
    *   **`utility/`**: Helper functions for file I/O, random number generation, and string manipulation.

*   **`hardware/`**: Physical hardware model representation.
    *   `interposer.hh`: Physical properties of the interposer (dimensions, resources).
    *   `bump/`: Micro-bump definitions (`Bump`, `BumpCoord`).
    *   `cob/`: "Connect-On-Bump" structures (switching matrices on the base die).
    *   `tob/`: "Top-On-Bump" structures (interface logic on the top die).
    *   `track/`: Routing tracks on the interposer (`Track`, `TrackCoord`).
    *   `coord.hh`: Fundamental coordinate system used throughout the project.

*   **`parse/`**: Input parsing and Output generation.
    *   `reader/`: Parsers for configuration files.
        *   `config/`: Logic to read JSON/TXT configuration files into internal structs.
    *   `writer/`: Generators for output files (e.g., register configuration values).
    *   `comparator/`: Tools for comparing routing results or control bits.

*   **`serde/`**: Serialization/Deserialization infrastructure.
    *   `json/`: Custom JSON parser and tokenizer.
    *   `ser.hh` / `de.hh`: Macro-based system for easy struct serialization (`SERIALIZE_STRUCT`, `DESERIALIZE_STRUCT`).

*   **`widget/`**: GUI implementation (Qt-based).
    *   `frame/`: Basic UI frames and dialogs.
    *   `layout/`: Layout visualization widgets.
    *   `schematic/`: Schematic view widgets.
    *   `view2d/`: 2D physical view widgets.
    *   `view3d/`: 3D visualization widgets.

## Code Style & Conventions

### Naming Conventions
*   **Namespaces**: Snake case (e.g., `kiwi::hardware`, `kiwi::algo`). All code must be within the `kiwi` namespace.
*   **Classes/Structs**: PascalCase (e.g., `RouteStrategy`, `TopDieInstance`).
*   **Methods/Functions**: snake_case (e.g., `update_tob_postion`, `check_accessable_cobunit`).
*   **Member Variables**: snake_case with a leading underscore (e.g., `_coord`, `_connected_track`, `_value`).
*   **Files**: snake_case (e.g., `saplacestrategy.cc`, `net.hh`).

### Logging & Debugging
*   **Namespace**: `kiwi::debug`
*   **Levels**:
    *   `debug::debug()`: Detailed internal state, usually for development.
    *   `debug::info()`: General progress information (e.g., "Start Layout").
    *   `debug::warning()`: Potential issues that don't stop execution.
    *   `debug::error()`: Recoverable errors.
    *   `debug::fatal()`: Non-recoverable errors, terminates execution.
*   **Formatting**: Use the `_fmt` suffix variants for formatted output (similar to `std::format`).
    *   Example: `debug::info_fmt("Processing die: {}", die->name());`
*   **Exceptions**: Use `debug::exception()` or `THROW_UP_WITH("Context")` macro to throw exceptions with context.

### Standard Library Usage
*   **Wrappers**: Do **NOT** use `std::vector`, `std::string`, `std::map` directly in headers or core logic.
*   **Include**: Include headers from `global/std/` (e.g., `#include <std/collection.hh>`).
*   **Types**: Use `std::Vector`, `std::String`, `std::HashMap`, `std::HashSet`, `std::Option` (for `std::optional`), `std::Rc` (for `std::shared_ptr`), `std::Box` (for `std::unique_ptr`).

### Serialization
*   To make a struct serializable, use the macros in `serde/ser.hh` and `serde/de.hh` inside the struct definition or in a companion function.
