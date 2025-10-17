# 2025.10.17 test note

- 测试目的：对比现在的布线算法在布线质量上和之前的布线算法有什么差别。布线质量包括**全局布线长度、同步线布线长度、布通率**

- 测试数据：用 case13 ，case12，case9 分别测试（case14，15，16现在的方法还无法连通）

- 实验设置：

    * 用 version_before_command 跑之前单模式的布线算法，同一个例子运行 10 次，取上述三个指标的平均值

    * 用现在的算法跑三个例子，同一个例子运行 10 次，取上述三个指标的平均值

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

        



