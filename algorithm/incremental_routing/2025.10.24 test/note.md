##

- 打印出路径资源所在的寄存器的所有信息，并用 matlab 可视化，查看寄存器资源随迭代轮次的变化

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

