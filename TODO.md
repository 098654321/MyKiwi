## TODO

- 增量布线算法调整：

  - 完善各个单元模块的测试

  - 确定 entry/exit cob 的方法还需要优化，目前对于路径的预估不是很准确

  - 最后对剩余net布线的时候少了检查已有路径的步骤（见route.cc）

  - shared_nets 完成布线之后打印结果

  - 可能导致 connect 的时候 tob mux 错误的原因：

    cursor/Multi-mode wiring error analysis (C)(E)

  - 当前打印的结果当中，有很多update信息是不需要的，然后没有清楚的展示出paired nets布线成功/失败的原因

  - 配对nets布线结束之后三段路径里面要把cob to cob的路径以reuse_type=true设置一下

  - 完整检查bus_reroute的调整是否有问题

  - 收敛条件需要改。当前只考虑了 non_reuse 占用的变化，没有考虑两个 mode 在 entry/exit cob 之间的路径有没有真实的走到一起

  - 目前
--- 

- 当前hardware底层的数据结构不是很安全，最好查一下怎么给指针包装一层，放置上层使用的时候直接操作指针

- 把 vpr 针对 high-fanout 的优化加上

- 一对多的 net 的 path_in_order() 函数需要重新实现

- 加一个功能：如果吧其它 mode 的布线结果加进来，但是布线失败了，就把与失败的线冲突的、已经加载的线移除重布

- GUI 还没有适配增量布线**
