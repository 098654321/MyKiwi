## test

- 实验目的：文件夹里面的两个配置文件用于测试加载已有路径。然后这个文件夹的测试还用于检测收敛条件。当前的收敛条件尝试用全局被使用的路径资源对应的 cost_non_reuse的变化来确定，如果这个总量变化越来越小，小于一个给定的阈值，就视为收敛

- 测试数据：case20 mode1

- 参数设置：

    ```C++
    min_cycle{50}

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

    1. 根据 debug_50.log 里面提取的代价函数信息，迭代在每进行一段时间时候都会突然增长同步线的路径，产生一个新的布线方式，同时代价函数因为路径变长，使用的寄存器变多，函数值突然增加很多。说明同类型的线在一个寄存器里面收缩的太多之后会引起布线资源冲突，导致重布线。

        * 原因是同步线集中在寄存器里面后出现资源冲突，导致重新布线。这是这个算法本身的缺陷，因为算法希望线聚集在同一个寄存器里面就一定会出现资源冲突。所以迭代的过程没有处理资源冲突

    2. 记录的总代价函数值的大小也能反应路径的总长度。这个总代价函数值也会跟随 1 的现象发生变化，在 1 出现的时候，总代价函数值会突然增加很多。而且还发现这个变化跟 Epsilon 参数值、 his & cur 的比例有关。   

        * 增加一个测试：

            ```C++
            min_cycle{50} // 能跑到多少就是多少
            const float EPSILON = 0.1, 1, 5, 10, 20, 50, 100;
            const float GROUPCOEF = 0.5;
            const float HISTORYCOEF = 0.5;
            ```

            试下来 1 的效果可以。再试一下 0.5，1，1.5，2，2.5，3，3.5，4，4.5，5 这些情况，看一下 cost_non_reuse 的变化趋势

            ```C++
            min_cycle{50} // 能跑到多少就是多少
            const float EPSILON = 0.5，1，1.5，2，2.5，3，3.5，4，4.5，5;
            const float GROUPCOEF = 0.5;
            const float HISTORYCOEF = 0.5;
            ```

            0.5 的情况 cost_non_reuse 很容易出现重布线，并且没有重布线的时候 cost_non_reuse 也会缓慢增长，没有收敛趋势
            1 同上，有收敛趋势
            1.5 也会有突然变大的现象，同时在没有变大的时候也缓慢增长
            2 在突然变大之前有微弱的逐渐平缓的趋势，不能明显的收敛
            3 突然变大的次数比较多
            4 突然变大的次数也比较多
            6 突然变大的次数也比较多

            综合下来 1 的效果最好，但是 1 有时候会跑不出来。在 EPSILON = 1 的时候，代价函数变化的比较快，同类聚集之后 cost 下降的快，但是这个时候重布线出现的也更频繁。感觉需要这种 cost 下降快的才有明显的收敛趋势，所以只需要解决布线路径收缩过程中资源冲突的问题。

        * 然后再测 CUR & HIS 的比例

            实验发现，把 CUR 的比例调高之后，重布线的次数会更加频繁。这和上面有没有联系？

            根据当前实验结果,只要 HIS >= CUR ，效果还行



