## Description

- 参考 [vpr算法论文](https://www.eecg.toronto.edu/~vaughn/papers/fpl97.pdf)

    针对 **high-fanout net** 的迷宫算法优化，即每一次找到 sink 之后并连上一段路径后，不要清空 wavefront ，而是将新连上的路径以 cost=0 加入 wavefront ，然后正常进行扩展

- 
