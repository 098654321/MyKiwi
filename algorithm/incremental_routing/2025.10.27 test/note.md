##

- 打印出路径资源的所有信息，并用 matlab 可视化，查看路径资源随迭代轮次的变化中有没有聚集到相同寄存器里面，同时查看路径长度有没有增长

- 测试数据：case8

- 参数设置：

    ```C++
    min_cycle{20}

    const float BASICCOST = 5;
    const float EPSILON = 5;
    const float GROUPCOEF = 0.5;
    const float HISTORYCOEF = 0.5;

    auto history_ratio = (h_reuse_n-h_nonre_n) / (h_reuse_n+h_nonre_n+EPSILON);
    auto group_ratio = (reuse_num-nonre_num) / (nonre_num + reuse_num + EPSILON);

    this->_cost_reuse = BASICCOST * (1-GROUPCOEF*group_ratio-HISTORYCOEF*history_ratio);
    this->_cost_nonreuse = BASICCOST * (1+GROUPCOEF*group_ratio+HISTORYCOEF*history_ratio);
    ```

    跑一遍，收集 debug.log


- 实验结果

    - 根据线网路径情况的 cycle0, cycle5, cycle10, cycle15 的可视化结果，可以发现在右下角的四个芯粒连接中 cpu -> 左侧npu 的路径在 cycle 0 是最短的，但是从 cycle 5 开始从下边绕了一下。然后到 cycle15 的时候又从上边绕。

        cycle5 从下边绕还没想明白。cycle15从上边绕是因为上边的资源已经有很多同类型的 net 使用了，所以代价会低一点，但是这样也导致路径变长，好处是实现了共用寄存器的想法。



