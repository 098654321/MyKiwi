# Kiwi Project Source Code Documentation

This document provides an overview of the `source/` directory for AI agents. The Kiwi project appears to be an EDA (Electronic Design Automation) tool focused on 2.5D/3D IC design, specifically handling interposer-based designs, placement, and routing.

## Directory Structure

*   **`algo/`**: Core algorithms.
    *   `netbuilder/`: Logic for building nets from connections.
    *   `placer/`: Placement algorithms, including Simulated Annealing (`sa/`).
    *   `router/`: Routing algorithms. Includes `maze/` (Maze routing), `incremental/` (Incremental routing), and `command_mode/` (Interactive routing commands).
*   **`app/`**: Application entry points.
    *   `cli/`: Command Line Interface implementation.
    *   `gui/`: Graphical User Interface entry point.
    *   `main.cc`: Main entry point.
*   **`circuit/`**: Logical circuit model.
    *   `connection/`: Pin and connection definitions.
    *   `net/`: Different types of nets (BBNet, SyncNet, etc.).
    *   `topdie/` & `topdieinst/`: Logical definitions of Top Dies and their instances.
    *   `export/`: Export logic.
*   **`global/`**: Cross-cutting concerns and utilities.
    *   `debug/`: Logging (`debug::info`, `debug::exception`) and assertions.
    *   `std/`: **IMPORTANT**: Custom wrappers around C++ Standard Library types (`std::Vector`, `std::String`, `std::HashMap`). Always check here before using raw std types.
    *   `utility/`: Random number generation, string manipulation, file I/O.
*   **`hardware/`**: Physical hardware model.
    *   `interposer.hh`: Definition of the interposer dimensions and properties.
    *   `cob/` (Connect-On-Bump) & `tob/` (Top-On-Bump): Physical interconnect structures.
    *   `track/`: Routing track definitions (`TrackCoord`).
    *   `coord.hh`: Basic coordinate system.
*   **`parse/`**: Input/Output processing.
    *   `reader/`: Parsing configuration files (JSON, TXT) into internal data structures (`Config` struct).
    *   `writer/`: Outputting results (Register values, module info).
*   **`serde/`**: Serialization/Deserialization library.
    *   `json/`: JSON parser and generator.
    *   `ser.hh` / `de.hh`: Macros (`SERIALIZE_STRUCT`, `DESERIALIZE_STRUCT`) for easy struct serialization.
*   **`widget/`**: GUI implementation.
    *   Likely based on Qt (uses `.h` extensions, `graphicsview`, `signals/slots` patterns implied).
    *   `schematic/`, `view2d/`, `view3d/`: Visualization widgets.

## Code Style & Conventions

*   **Namespaces**: All code is enclosed in the `kiwi::` namespace (e.g., `kiwi::hardware`, `kiwi::algo`).
*   **File Extensions**:
    *   `.cc`: C++ source files.
    *   `.hh`: C++ header files (core logic).
    *   `.h`: C++ header files (GUI/Qt related).
*   **Standard Library**: The project heavily uses wrappers defined in `global/std/`.
    *   Example: Use `std::FilePath` instead of `std::filesystem::path`.
    *   Example: Use `std::Vector` instead of `std::vector`.
*   **Error Handling**:
    *   Use `debug::info()`, `debug::warn()`, `debug::exception_fmt()`.
    *   `THROW_UP_WITH("Context")` macro is used for exception propagation context.
*   **Serialization**:
    *   To make a struct serializable, use the macros in `serde/ser.hh` and `serde/de.hh`.

## Key Concepts

*   **Interposer**: The substrate upon which Top Dies are placed.
*   **TopDie**: Functional dies (chips) placed on the interposer.
*   **Grid/Coords**: The design uses a coordinate system (`row`, `col`) often wrapped in `Coord`, `TrackCoord`, or `TOBCoord`.
*   **Routing**: Involves connecting pins via tracks using algorithms like Maze routing or Incremental routing.
