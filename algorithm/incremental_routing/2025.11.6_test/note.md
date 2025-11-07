## test

- 实验目的：实现收敛条件后的测试，检测收敛阈值是否合理

- 实验设置

    * 参数设置

    ```C++
    const float BASICCOST = 5;
    const float EPSILON = 1;
    const float GROUPCOEF = 0.1;
    const float HISTORYCOEF = 0.9;
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

    * 对 mode1 的测试

        - 先跑 mode2 到收敛，保留 controlbits_2.txt 作为对比的标准

        - 检测一下用这个文件作为前置路径跑 mode1 能不能行，不行就重来

        - 跑 5 遍 mode1 ，每一遍跑到收敛，测试全局线长、同步线长、最大线长。每一遍保留 debug.log 和 controlbits_1.txt

        - 使用上面的五个 controbits 文件和标准文件对比得到寄存器差异数量

    * 对 mode2 的测试

        - 跑 mode1 的结果作为标准对比文件，然后其余内容和上面一摸一样

    * 使用第一个测试当中 controlbits_2.txt 的内容作为已有路径结果，跑 5 遍 mode1 ，对比寄存器差异数量

    * 使用第二个测试当中 controlbits_1.txt 的内容作为已有路径结果，跑 5 遍 mode2 ，对比寄存器差异数量
