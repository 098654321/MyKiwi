## TODO

- 增量布线算法调整：

  - 完善各个单元模块的测试

  - 完善try-catch 异常收集机制

  - classify nets的时候暂时用vector存，如果之后没有影响，可以改成unordered_set

  - 最后对剩余net布线的时候少了set_reuse_type和检查已有路径的步骤（见route.cc）

  - shared_nets 完成布线之后打印结果

  - 配对nets布线结束之后三段路径里面要把cob to cob的路径以reuse_type=true设置一下

  - build three segment 还没查完，没有包含同步线

  - bfz_maze没有用到cost

--- 

- 当前hardware底层的数据结构不是很安全，最好查一下怎么给指针包装一层，放置上层使用的时候直接操作指针

- 把 vpr 针对 high-fanout 的优化加上

- 一对多的 net 的 path_in_order() 函数需要重新实现

- 加一个功能：如果吧其它 mode 的布线结果加进来，但是布线失败了，就把与失败的线冲突的、已经加载的线移除重布

- GUI 还没有适配增量布线**
