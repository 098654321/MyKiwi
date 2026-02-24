## TODO

- 目前有概率出现布局之后某个net的begin/end bump tob相同，导致布线失败的问题。添加输出信息

- 把 vpr 针对 high-fanout 的优化加上

- 一对多的 net 的 path_in_order() 函数需要重新实现**

- 加一个功能：如果吧其它 mode 的布线结果加进来，但是布线失败了，就把与失败的线冲突的、已经加载的线移除重布**

- GUI 还没有适配增量布线**

**当前版本在output文件夹里面有自己 mode 的布线结果的时候会不布线。有其它mode布线结果的时候暂时把bits_to_path禁用了**







