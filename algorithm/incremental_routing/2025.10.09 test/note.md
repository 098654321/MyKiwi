## 2025.10.09 test note

- 实验目的：测试迭代轮次增长的情况下最终需要切换的寄存器数量会怎么变化，如果变少了说明算法有效果。然后分析原因

- 代价函数设置

    ```C++
    auto h_reuse_n = std::get<0>(this->_history);
    auto h_nonre_n = std::get<1>(this->_history);
    auto history_ratio = (this->_reuse_type ? (h_reuse_n-h_nonre_n) : (h_nonre_n-h_reuse_n)) / (h_reuse_n+h_nonre_n+EPSILON);

    auto group_ratio = (this->_reuse_type ? (reuse_num-nonre_num) : (nonre_num-reuse_num))/(nonre_num + reuse_num + EPSILON);
    this->_cost = BASICCOST * (1-GROUPCOEF*group_ratio-HISTORYCOEF*history_ratio);
    ```

- 实验设置

    * 先在迭代轮数为 1 的情况下跑一遍 case17 的 mode 2 ，得到 controlbits 作为对比文件

    * 然后跑 mode 1 ：

        ```C++
        PLEASE_DO_NOT_FAIL_INCRE(17, "", 1, 5);
        ```

        每个迭代次数下跑五遍，记录每一遍得到的 controlbits 文件和标准文件之间不同寄存器（即要切换的寄存器）的数量，计算平均值。并同理计算全局总线长的平均值与布线失败率（有失败的线就算失败）

- 实验结果

    |迭代次数|1|2|3|4|5|AVG|FAIL_RATE|TOTAL_AVG_LENGTH|
    |---|---|---|---|---|---|---|---|---|
    |1|2102|2116|2118|2108|2097|2108.2|0.0|2880|
    |2|2108|2090|2108|2094|2109|2101.8|0.0|2880.6|
    |3|2114|2153|2127|2102|2129|2125|0.0|2881.2|
    |4|2116|2119|2097|2125|2119|2115.2|0.0|2881.2|
    |5|2118|2131|2112|2114|2105|2116|0.0|2881|
    |6|2108|2111|2120|2114|2104|2115|0.0|2881.2|
    |9|2100|2115|2102|2093|2130|2108|0.0|2881|
    |10|2126|2111|2127|2109|2113|2117.2|0.0|2881|

    




