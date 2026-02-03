## test

- 实验目的：对比不同的EPSILON，HISTORYCOEF，GROUPCOEF对布线结果的影响

- 实验设置

    * 参数设置

    ```C++
    const float BASICCOST = 5;
    const float TOBMUXGROUPSIZE = 8;
    const float TRACKGROUPSIZE = 32;

    auto history_ratio = (h_reuse_n-h_nonre_n) / (h_reuse_n+h_nonre_n+EPSILON);
    auto group_ratio = (reuse_num-nonre_num) / (nonre_num + reuse_num + EPSILON);

    this->_cost_reuse = BASICCOST * (1-GROUPCOEF*group_ratio-HISTORYCOEF*history_ratio);
    this->_cost_nonreuse = BASICCOST * (1+GROUPCOEF*group_ratio+HISTORYCOEF*history_ratio);
    ```

    * 测试数据

        case 20 

- 测试内容

    1. 使用2026.01.21_test/test2里面的mode1_1.txt ~ mode1_5.txt作为 mode2 的输入，分别在一下参数设置下运行从 mode1 -> mode2 的切换，得到每一种参数设置的五个运行结果（controlbits.txt + debug.log）

    2. 计算每个参数设置下五次结果的平均值，指标包括全局线长，挂件路径长度，切换的寄存器数量，迭代轮数

    3. 参数组合如下

        |序号|EPSILON|GROUPCOEF|HISTORYCOEF|全局线长|最大线长|切换的寄存器数量|
        |:--:|:--:|:--:|:--:|:--:|:--:|:--:|
        |1|0.1|0.1|0.9|/|/|/|
        |2|0.1|0.3|0.7||||
        |3|0.1|0.5|0.5||||
        |4|0.1|0.7|0.3||||
        |5|0.1|0.9|0.1||||
        |6|1|0.1|0.9||||(直接用之前的结果)
        |7|1|0.3|0.7||||
        |8|1|0.5|0.5||||
        |9|1|0.7|0.3||||
        |10|10|0.9|0.1||||
        |11|10|0.1|0.9||||
        |12|10|0.3|0.7||||
        |13|10|0.5|0.5||||
        |14|10|0.7|0.3||||
        |15|10|0.9|0.1||||

        注：/表示无法收敛
