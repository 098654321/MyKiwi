import numpy as np
import matplotlib.pyplot as plt
import random
import time
from minheap import MinHeap
from copy import deepcopy

DIRS = [(-1, 0), (1, 0), (0, -1), (0, 1)]


def backtrace(prev, starts, end):
    path = []
    cx, cy = end
    while (cx, cy) not in starts:
        path.append((cx, cy))
        cx, cy = prev[cx, cy]
    path.reverse()
    return path


# 传统方式
def maze_route_traditional(grid, ports):
    h, w = grid.shape
    used_mask = np.full((h, w), False)
    total_cost = 0
    total_path = [ports[0]]
    remaining = set(ports[1:])

    while remaining:
        dist = np.full((h, w), np.inf)
        prev = np.full((h, w, 2), -1)
        visited = np.full((h, w), False)
        heap = MinHeap()
        arr = []
        for current in total_path:
            dist[current] = 0
            arr.append((0, current))
        heap.heapify(arr)

        target_found = None
        while heap:
            cost, (x, y) = heap.pop()
            if visited[x, y]:
                continue
            visited[x, y] = True
            if (x, y) in remaining:
                target_found = (x, y)
                break
            for dx, dy in DIRS:
                nx, ny = x + dx, y + dy
                if 0 <= nx < h and 0 <= ny < w and not used_mask[nx, ny]:
                    ncost = cost + grid[nx, ny]
                    if ncost < dist[nx, ny]:
                        dist[nx, ny] = ncost
                        prev[nx, ny] = (x, y)
                        heap.push((ncost, (nx, ny)))

        if not target_found:
            break
        path = backtrace(prev, total_path, target_found)
        for x, y in path:
            used_mask[x, y] = True
        total_path.extend(path)
        total_cost += sum(grid[x, y] for x, y in path)
        remaining.remove(target_found)

    return total_path, total_cost


# 优化方式：保留波前，新增路径以0代价加入
def maze_route_optimized(grid, ports):
    h, w = grid.shape
    used_mask = np.full((h, w), False)
    total_path = [ports[0]]
    total_cost = 0

    dist = np.full((h, w), np.inf)
    prev = np.full((h, w, 2), -1)
    visited = np.full((h, w), False)
    heap = MinHeap()

    dist[ports[0]] = 0
    heap.heapify([(0, ports[0])])

    remaining = set(ports[1:])

    while remaining:
        target_found = None
        while heap:
            cost, (x, y) = heap.pop()
            if visited[x, y]:
                continue
            visited[x, y] = True
            if (x, y) in remaining:
                target_found = (x, y)
                break
            for dx, dy in DIRS:
                nx, ny = x + dx, y + dy
                if 0 <= nx < h and 0 <= ny < w and not used_mask[nx, ny]:
                    ncost = cost + grid[nx, ny]
                    if ncost < dist[nx, ny]:
                        visited[nx, ny] = False
                        dist[nx, ny] = ncost
                        prev[nx, ny] = (x, y)
                        if heap.has((nx, ny)):
                            heap.remove((nx, ny))
                        heap.push((ncost, (nx, ny)))

        if not target_found:
            break

        # 回溯路径
        path = backtrace(prev, total_path, target_found)
        for x, y in path:
            used_mask[x, y] = True
            visited[x, y] = False
            dist[x, y] = 0
            heap.push((0, (x, y)))
        total_path.extend(path)
        total_cost += sum(grid[x, y] for x, y in path)
        remaining.remove(target_found)

    return total_path, total_cost


# 可视化
def visualize(grid, ports, path, title="Routing"):
    plt.figure(figsize=(8, 8))
    plt.imshow(grid, cmap='gray_r')
    plt.colorbar()
    px, py = zip(*path)
    plt.scatter(py, px, c='red', marker='o', s=5)
    port_x, port_y = zip(*ports)
    plt.scatter(port_y, port_x, c='blue', marker='x', s=5)
    plt.title(title)
    plt.show()


# 对比测试
def test_and_compare():
    # np.random.seed(42)
    # random.seed(42)
    size = 100
    grid = np.random.randint(1, 6, (size, size))
    ports = random.sample([(i, j) for i in range(size) for j in range(size)], 20)

    # Traditional
    t0 = time.time()
    path1, cost1 = maze_route_traditional(grid, ports)
    t1 = time.time()

    # Optimized
    t2 = time.time()
    path2, cost2 = maze_route_optimized(grid, ports)
    t3 = time.time()

    print(f"网格规模：{size}*{size}")
    print(f"端口数量：{len(ports)}")

    print("优化前:")
    print(f"   总 cost: {cost1}")
    print(f"   用时: {t1 - t0:.4f} 秒\n")

    print("优化后:")
    print(f"   总 cost: {cost2}")
    print(f"   用时: {t3 - t2:.4f} 秒")

    visualize(grid, ports, path1, "traditional maze")
    visualize(grid, ports, path2, "optimized maze")


if __name__ == '__main__':
    test_and_compare()
