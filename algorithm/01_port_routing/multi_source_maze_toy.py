#!/usr/bin/env python3
"""Toy example: multi-source maze routing on a 10x10 grid (numpy + matplotlib + deque).

Dependencies: numpy, matplotlib
"""

from __future__ import annotations

import random
from collections import deque

import matplotlib.pyplot as plt
import numpy as np

GRID = 10
DIRS = ((1, 0), (-1, 0), (0, 1), (0, -1))


def boundary_cells() -> list[tuple[int, int]]:
    cells: set[tuple[int, int]] = set()
    for x in range(GRID):
        for y in range(GRID):
            if x == 0 or x == GRID - 1 or y == 0 or y == GRID - 1:
                cells.add((x, y))
    return list(cells)


def sample_io_ports(rng: random.Random) -> list[tuple[int, int]]:
    b = boundary_cells()
    return rng.sample(b, 3)


def sample_pins(rng: random.Random, io: set[tuple[int, int]]) -> list[tuple[int, int]]:
    candidates = [(x, y) for x in range(GRID) for y in range(GRID) if (x, y) not in io]
    return rng.sample(candidates, 5)


def multi_source_bfs_until_pin(
    routed_network: set[tuple[int, int]],
    all_pins: set[tuple[int, int]],
    connected_pins: set[tuple[int, int]],
) -> tuple[tuple[int, int] | None, dict[tuple[int, int], tuple[int, int]]]:
    """Expand from all nodes in routed_network; stop when any unconnected pin is reached.

    Returns (found_pin, parent_map). parent_map is defined for all visited cells off
    the rooted forest plus the found pin entry.
    """
    parent_map: dict[tuple[int, int], tuple[int, int]] = {}
    q: deque[tuple[int, int]] = deque(routed_network)
    visited: set[tuple[int, int]] = set(routed_network)

    while q:
        cx, cy = q.popleft()
        for dx, dy in DIRS:
            nx, ny = cx + dx, cy + dy
            if nx < 0 or nx >= GRID or ny < 0 or ny >= GRID:
                continue
            v = (nx, ny)
            if v in all_pins and v not in connected_pins:
                parent_map[v] = (cx, cy)
                return v, parent_map
            if v in visited:
                continue
            parent_map[v] = (cx, cy)
            visited.add(v)
            q.append(v)
    return None, parent_map


def backtrace_path(
    found_pin: tuple[int, int],
    parent_map: dict[tuple[int, int], tuple[int, int]],
    routed_network: set[tuple[int, int]],
) -> list[tuple[int, int]]:
    path: list[tuple[int, int]] = []
    cur = found_pin
    path.append(cur)
    while cur not in routed_network:
        cur = parent_map[cur]
        path.append(cur)
    path.reverse()
    return path


def route_all_pins(
    io_ports: list[tuple[int, int]],
    pins: list[tuple[int, int]],
) -> tuple[set[tuple[int, int]], set[tuple[int, int]]]:
    all_pins = set(pins)
    routed_network: set[tuple[int, int]] = set(io_ports)
    connected_pins: set[tuple[int, int]] = set()

    while len(connected_pins) < len(pins):
        found, parent_map = multi_source_bfs_until_pin(
            routed_network, all_pins, connected_pins
        )
        if found is None:
            raise RuntimeError(
                "BFS could not reach any unconnected pin; check grid/obstacles (toy has none)."
            )
        path = backtrace_path(found, parent_map, routed_network)
        for cell in path:
            routed_network.add(cell)
        connected_pins.add(found)

    return routed_network, connected_pins


def visualize(
    io_ports: list[tuple[int, int]],
    pins: list[tuple[int, int]],
    routed_network: set[tuple[int, int]],
) -> None:
    io_set = set(io_ports)
    pin_set = set(pins)
    intermediate = routed_network - io_set - pin_set

    fig, ax = plt.subplots(figsize=(7, 7))
    ax.set_xlim(-0.5, GRID - 0.5)
    ax.set_ylim(-0.5, GRID - 0.5)
    ax.set_aspect("equal")
    ax.set_xticks(range(GRID))
    ax.set_yticks(range(GRID))
    ax.grid(True, which="major", linestyle="-", linewidth=0.6, color="0.75", zorder=0)
    ax.set_axisbelow(True)

    ix, iy = zip(*io_ports)
    ax.scatter(
        ix,
        iy,
        s=160,
        marker="s",
        c="tab:blue",
        edgecolors="navy",
        linewidths=0.8,
        label="I/O ports",
        zorder=4,
    )

    px, py = zip(*pins)
    ax.scatter(
        px,
        py,
        s=200,
        marker="*",
        c="tab:red",
        edgecolors="darkred",
        linewidths=0.5,
        label="Pins",
        zorder=4,
    )

    if intermediate:
        mx, my = zip(*sorted(intermediate))
        ax.scatter(
            mx,
            my,
            s=36,
            marker="o",
            c="tab:green",
            edgecolors="darkgreen",
            linewidths=0.4,
            label="Routing",
            zorder=3,
        )
    else:
        ax.scatter(
            [],
            [],
            s=36,
            marker="o",
            c="tab:green",
            edgecolors="darkgreen",
            linewidths=0.4,
            label="Routing",
        )

    # Green segments between 4-neighbors both in routed_network
    routed_list = sorted(routed_network)
    in_r = routed_network
    segs_x: list[tuple[float, float]] = []
    segs_y: list[tuple[float, float]] = []
    for x, y in routed_list:
        for dx, dy in ((1, 0), (0, 1)):
            x2, y2 = x + dx, y + dy
            if (x2, y2) in in_r:
                segs_x.append((x, x2))
                segs_y.append((y, y2))
    for sx, sy in zip(segs_x, segs_y):
        ax.plot(
            sx,
            sy,
            color="tab:green",
            linewidth=2.0,
            alpha=0.45,
            zorder=2,
        )

    ax.legend(loc="upper right")
    ax.set_title("Multi-source Maze Routing for Equivalence I/O")
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    plt.tight_layout()
    plt.show()


def main() -> None:
    rng = random.Random(42)
    np.random.seed(42)

    grid_state = np.zeros((GRID, GRID), dtype=np.int8)

    io_ports = sample_io_ports(rng)
    pins = sample_pins(rng, set(io_ports))

    for x, y in io_ports:
        grid_state[x, y] = 1
    for x, y in pins:
        grid_state[x, y] = 2

    routed_network, _connected = route_all_pins(io_ports, pins)

    visualize(io_ports, pins, routed_network)


if __name__ == "__main__":
    main()
