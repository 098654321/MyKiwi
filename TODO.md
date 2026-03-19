## TODO

- 增量布线算法调整：

  - 目前的迭代在case21当中无法收敛，需要检查无法收敛的原因，调整收敛条件**（看一下incremental_route.cc的iterate_routing里面的clear_history_records这一步是否必要）**

- bugs

  - -m 模式下，读取connections文件的时候没有指定读取mode1、2的信息，导致读取的层次不对

--- 

- 把 vpr 针对 high-fanout 的优化加上

- 一对多的 net 的 path_in_order() 函数需要重新实现

- 加一个功能：如果吧其它 mode 的布线结果加进来，但是布线失败了，就把与失败的线冲突的、已经加载的线移除重布

- GUI 还没有适配增量布线**
