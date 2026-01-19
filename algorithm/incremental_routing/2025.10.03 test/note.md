## 调整 update cost 的策略 （修改 typerecorder.update_cost）

### 尝试最简单的方法

不迭代，只跑一轮看看实力。

```C++
if (this->reuse_type == true) {
    if (reuse_num > 0 && non_reuse_num == 0){
        this->cost = 0;
    }
    else if (non_reuse_num > 0){
        this->cost = BASIC_COST * 100;
    }
}
else {
    if (reuse_num > 0 && non_reuse_num == 0){
        this->cost = BASIC_COST * 100;
    }
    else if (non_reuse_num > 0){
        this->cost = 0;
    }
}
```


#### 实验 1 ：不适用任何修改 cost 的方法，直接裸奔

```C++
update_cost() {
    this->cost = BASIC_COST;
}
```
incremental 布线不迭代，只跑一轮

- 实验设置

    ```C++
    PLEASE_DO_NOT_FAIL_INCRE(19, "modified case18 to with more reusable/uon-reusable nets be mixed together", 1, 30);
    ```

- 实验结果

    Average Total Length: 686.8

    Average Sync Length: 8.563158

    Average Fail Rate: 1

- 分析

    和 10.1 test_5 相比，寄存器的同类型独占率提高了 1.1% ，同时全局总线长和同步线平均线长都有极少量增加。说明 10.1 test_5 采用的算法在分离不同类型 net 占用的寄存器上效果不好。（？为什么效果不好）


#### 实验 2 ：使用修改 cost 的策略

```C++
const float BASICCOST = 5;
const float EPSILON = 5;
const float GROUPCOEF = 0.5;
const float HISTORYCOEF = 0.5;
const float TOBMUXGROUPSIZE = 8;
const float TRACKGROUPSIZE = 32;

auto h_reuse_n = std::get<0>(this->_history);
auto h_nonre_n = std::get<1>(this->_history);
auto history_ratio = (this->_reuse_type ? (h_reuse_n-h_nonre_n) : (h_nonre_n-h_reuse_n)) / (h_reuse_n+h_nonre_n+EPSILON);
auto group_ratio = (this->_reuse_type ? (reuse_num-nonre_num) : (nonre_num-reuse_num))/(nonre_num + reuse_num + EPSILON);

this->_cost = BASICCOST * (1-GROUPCOEF*group_ratio-HISTORYCOEF*history_ratio);
```
incremental 布线迭代 20 轮

- 实验设置

    ```C++
    PLEASE_DO_NOT_FAIL_INCRE(19, "modified case18 to with more reusable/uon-reusable nets be mixed together", 1, 1);
    ```

- 实验结果

    * Has Nonreuse 一直在反复变，Monopolized by Reuse 也在反复变，没有稳定增加或者减少的趋势

    * 检查布线路径，发现 has nonreuse 增大是因为重布线，导致 nonreuse_net 占用了更多的寄存器。但是如果不管重布线的情况，只看不出现重布线的情况，总体是在下降的


#### 实验 3

- 实验设置

    cost 的策略和实验 2 一样，但是跑了 100 轮

- 实验结果

    * 单从 has nonreuse 这个指标来看，这个数值一直上下波动，说明算法没有起到优化的效果

    * 没有出现重布线的轮次，不管 has nonreuse 如何上下波动，其值都比出现重布线的轮次低


#### 实验 4 

```C++
auto group_ratio = 1 + (this->_reuse_type ? nonre_num : reuse_num);
this->_cost = BASICCOST * (1-history_ratio) * group_ratio;
```

- 实验设置

    修改了 cost 的设置，跑了 7 轮

- 实验结果

    has nonreuse 一直在上升，并且保持在较高的值。查看路径情况，发现总是有重布线导致长度增加的情况，说明用于提高长度的重布线是最主要的原因。


## 综上所述

- 重布线导致同步线长度增长，是 has nonreuse 数值增长的主要原因

- 还有两个逻辑待确认：

    - 在那些没有重布线的例子中，has nonreuse 波动的原因是什么？

    - has nonreuse 和最终需要切换的寄存器数量之间是怎样的关系



