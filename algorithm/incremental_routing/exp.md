## 记录当前算法的实验

### ex1 

```c++
const std::usize BASICCOST = 5;
const float EPSILON = 1;
const float GROUPCOEF = 0.9;
const float HISTORYCOEF = 0.1;
auto group_ratio = (this->_reuse_type ? (float)(reuse_num-nonre_num) : (float)(nonre_num-reuse_num))/(float)(nonre_num + reuse_num + EPSILON);
auto history_ratio = (this->_reuse_type ? (float)(reuse_num-nonre_num) : (float)(nonre_num-reuse_num)) / (nonre_num+reuse_num+EPSILON);
return BASICCOST * (1-GROUPCOEF*group_ratio-HISTORYCOEF*history_ratio);
```

第一条线正常，从后面开始绕路绕的太远，过于牺牲路径长度换 controlbits 优化
绕路的时候会在某个cob附近打转



### ex2 2025.09.29

1. case 18 的两种 net 的重合度不够，改成 case 19

2. 
