---
description: Overview of test/ layout, benchmarks, and how module vs regression tests are built and run.
alwaysApply: true
---

# Kiwi Project Test Suite Documentation

This document provides an overview of the `test/` directory for AI agents. It details the testing infrastructure, data formats, and test cases used to validate the Kiwi EDA tool.

## Directory Structure

*   **`config/`**: Integration test cases (benchmarks).
    *   Numbered directories `case1` … `case22` hold design scenarios: JSON netlists, interposer/topdie data, and (for many cases) `golden.txt` plus `description.txt`.
    *   **`case17`** is structured differently: it mainly holds `NOI_pre_test/` (sub-scenarios with control-bit text files and related inputs) and `description.txt`, rather than a full top-level JSON bundle like the other cases.
    *   **`case18`–`case22`**: multi-mode / incremental-style JSON cases (see optional `reigster_adder.json`); used by targeted regression paths (e.g. incremental routing) and for manual or experimental runs, not all of them appear in every Catch2 scenario.
*   **`module_test/`**: Module-level tests (custom driver, not Catch2).
    *   `test.cc`: CLI entry point — run one module by name or `all` (see **How to Run Tests**).
    *   One `.cc` per area: `test_cob`, `test_tob`, `test_interposer`, `test_router`, `test_placer`, `test_debug`, `test_config`, `test_comparator`, `test_occupancyview`, `test_bbox`.
    *   `utilty.hh`: shared helpers for module tests.
    *   **`test_net_classification/`**: small JSON fixture set (mirrors a minimal config layout); not wired in code by path name — treat as reference data or for ad-hoc experiments unless a test explicitly loads it.
*   **`regression_test/`**: End-to-end regression tests using **Catch2**.
    *   `compile_catch2.cc`: defines `CATCH_CONFIG_MAIN` (single translation unit supplies the test runner main).
    *   `test.cc`: routing-only scenarios over `config/caseN` — tags `[basic]`, `[CPU_MEM_AI]`, `[CPU_MEM]`, `[AI_core]`; reads each case, runs `read_config` → `build_nets` → `route_nets`, then checks total wire length against `golden.txt` where present.
    *   `flow_test.cc`: placement-then-routing flow — tag `[flow]`; uses SA placement plus routing and asserts zero failed nets (`_failed_net == 0`). Includes helpers for an incremental variant (`PLEASE_DO_NOT_FAIL_FLOW_INCRE`), currently mostly commented in the active scenario.
    *   `incremental_test.cc`: incremental routing loops — tag `[incremental]`; exercises **case20** under multiple modes and writes analysis to `incremental_regression_test.log`.
*   **`transform_format/`**: Utility binary.
    *   `txt2json.cc`: converts legacy text-based configuration into the JSON format consumed by the tool (build target `transform_format` in `xmake.lua`).

## Test Case Structure (`config/`)

A typical test case directory (e.g., `test/config/case4/`) contains:

*   **Input Files**:
    *   `config.json`: The master configuration file linking other configs.
    *   `interposer.json`: Interposer physical definition.
    *   `topdies.json`: Definitions of available Top Die types.
    *   `topdie_insts.json`: Instances of Top Dies and their placement coordinates.
    *   `external_ports.json`: Definition of I/O ports.
    *   `connections.json`: The netlist defining connectivity between pins/ports.
    *   `01_ports.json`: Special port configurations (e.g. static 0/1 or tied ports).
    *   `reigster_adder.json`: Present in some later cases (`case18`+); spelling matches the repository. Optional depending on the case.
*   **Validation Files**:
    *   `golden.txt`: For routing-only regression (`test.cc`), the file is read as a single integer: the test requires routed **total length** to be **≤** that value. An empty or unreadable golden line is handled as a soft skip (logged), not a hard length check.
*   **Documentation**:
    *   `description.txt`: Human-readable description of the test case intent.
    *   `*.xlsx`: Excel sources sometimes used before conversion to JSON.
    *   Additional `*.txt` files in some cases: legacy or Excel-derived routing/bus definitions coexisting with JSON.

## Data Formats

*   **JSON**: The primary format for configuration.
    *   Parsed using the internal `serde` library.
    *   Key structures include `pin_map` (TopDie), `coord` (Placement), and connection arrays.
*   **TXT (Legacy / Routing / Control)**:
    *   Legacy routing-style `.txt` inputs (e.g. large bus descriptions) may appear beside JSON in CPU/MEM/AI-style cases.
    *   Incremental and NOI-style subfolders may supply control-bit or register text files consumed by parsers (`read_controlbits`, output writers), not only golden-style comparisons.

## How to Run Tests

Targets are defined in the repo root `xmake.lua`; binaries are placed under `output/`. Config paths inside regression tests are relative to the **process current working directory** (they use `../test/config/case<N>`); use the same invocation style as the project README so paths resolve consistently.

*   **Module tests**:
    *   `xmake build module_test`
    *   `xmake run module_test [cob|tob|interposer|router|placer|debug|config|comparator|occupancyview|bbox|all]`
*   **Regression (Catch2)**:
    *   `xmake build regression_test`
    *   `xmake run regression_test`
    *   Optional: filter by tag, e.g. scenarios tagged `[basic]`, `[flow]`, `[incremental]` (Catch2 tag syntax applies).
*   **`transform_format` tool**:
    *   `xmake build transform_format` then `xmake run transform_format` (entry point and any fixed paths are defined in `txt2json.cc`).
