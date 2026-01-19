## Description

- 参考 [vpr算法论文](https://www.eecg.toronto.edu/~vaughn/papers/fpl97.pdf)

    针对 **high-fanout net** 的迷宫算法优化，即每一次找到 sink 之后并连上一段路径后，不要清空 wavefront ，而是将新连上的路径以 cost=0 加入 wavefront ，然后正常进行扩展

- 在新的扩展进行的时候，如果发现扩展到的格点与原有 wavefront 里面的格点重合，且新扩展得到的格点对应的 cost 更小，那么就更新这个格点的 cost


- Algorithm

```python
Input: grid, ports
Output: total_path, total_cost

Init:
height, width = grid.shape
used_mask 记录被路径覆盖的格点
visited 保存格点是否被访问过
dist = 记录每个格点的cost
prev = 记录每个格点的前驱

total_path = [ports[0]]
total_cost = 0
remaining = ports[1:]
dist[ports[0]] = 0
minheap[(0, ports[0])]

1. while remaining is not empty:
2.     target_found = None
3.     while heap is not empty:
4.         cost, (x, y) = heap.pop()
5.         if (x, y) is visited, continue; else, set (x, y) as visited
6.         if (x, y) in remaining, set target_found = (x, y), break
7.         for neighbours (nx, ny) of (x, y):
8.             if (nx, ny) is in grid and not used as path node:
9.                 ncost = cost + grid[nx, ny]
10.                if ncost < dist[nx, ny]:
11.                    set (x, y) as unvisited
12.                    update dist[nx, ny] = ncost
13.                    set prev[nx, ny] = (x, y)
14.                    if heap.has((nx, ny)), update its cost and keep minheap
15.                endif
16.            endif
17.        endfor
18.    endwhile
19.    if not target_found,break
20.    path = backtrace from target_found to total_path with the help of prev
21.    for (x, y) in path, set as used, not visited, 0 dist and push (0, (x, y)) to heap
22.    total_path += path
23.    total_cost += sum(grid[x, y] for (x, y) in path)
24.    remaining -= target_found
25. endwhile
26. return total_path, total_cost
```


- 以下是 charGPT 的 prompt ：

    ```
    在 Electronic Design Automation 领域，布线问题的典型解决方法是迷宫算法。现在需要你生成一段 python 代码实现迷宫算法。

    生成算法函数之后，需要生成一个测试用例。在四邻接的50*50的网格当中，不需要生成障碍，且所有点的 cost 值都随机的在整数 1-5 之间选择，随机选取20个点作为一条线网的端口，并用你生成的迷宫算法函数完成这条线网的布线，需要将所有端口都连起来。你可以从某一个端口开始扩展，当扩展到另一个端口的时候就回溯得到一条路径，然后再将这条路径视作下一次扩展的起点，这样重复操作直到连上所有端口。最终需要用可视化的图像展示布线的路径，并打印布线的总长度，和迷宫算法运行的时间。
    ```

    ```
    传统的迷宫算法在计算高扇出线网(high-fanout)的路径的时候，采用逐个端口连接的方式，通过波前(wavefront)扩展，每次搜索到一个端口，就倒退得到这个端口到达起点的路径，然后将这个路径视作起点，重新进行波前扩展搜索下一个节点。这种方式比较浪费时间。

    现在我需要对高扇出线网的迷宫算法进行优化，优化的方法来自于一篇论文，这是论文的原文：
    When a net sink is reached, add all the    routing resource segments required to connect the sink and the current partial routing to the wavefront (i.e. the expansion list) with a cost of 0.  Do not empty the current maze routing wavefront; just continue expanding normally.  Since the new path added to the partial routing has a cost of zero, the maze router will expand around it at first. Since this new path is typically fairly small, it will take relatively little time to add this new wavefront, and the next sink will be reached much more quickly than if the entire wavefront expansion had been started from scratch.

    你根据这段英文文本描述的优化方式，帮我写一段 python 代码，实现优化后的迷宫算法。然后同时运行之前你写的传统迷宫算法和优化后的算法，对比一下两种算法的运行时间和布线结果的总 cost 。测试例子还是和之前一样
    ```

