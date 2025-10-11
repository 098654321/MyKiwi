## TODO

- 把 vpr 针对 high-fanout 的优化加上

- 原来对于增量布线的问题定义不合理，仅把可复用/不复用的线分开到不同的寄存器不能起到效果，同时应该加上全局总线长和关键路径尽可能小的目标

    不可复用的线网走在同一个寄存器里，可以减少修改寄存器的次数。

- 毕业论文里面举得算法例子说明代价函数没有办法起到分割的效果

- 原来算法更新代价的时机不合理，在 PathFinder 当中是每连完一条 net 就更新一次代价

- 首先在 preroute 得到一个新的 pathpackage 的时候，是存在分立的各个一对一小线网自己的 package 里面的，没有移动到 SyncNet 里面的 pathpackage 里面去的。所以 SyncNet 的 package 里面的内容还是旧的，但是小的 package 里面的内容是新的。但是二者使用的是同一个 TOB 上的 register 资源，所以其实都是重复的 register ，因为使用指针存的，指向的是同一个 register 。然后再 collect_package() 这个函数里面，this->_path_package.clear_all() 这一步看上去是清空了 SyncNet 自己的 package ，但是连带小的、最新的 register 一起清空了。这个是最主要的原因。当前的解决方式就是把 clear_all() 删了。**但是还有一个问题，增量布线失败的时候不能使用 history_package 作为结果，因为里面的 give_out 的值已经在当前轮次被修改的和 output_index 不一样了。可以把存储 history package 的方式改一改，用数值把 tobmuxregister 里面的东西都先存下来，不要直接 history_package = package**

**在两个模式之间，把相同类型的net 分到同一个寄存器，但是没有保证这个寄存器里面的每一位的配置方式都是一样的，这样还是有可能导致两个模式之间的配置文件相差比较大**

**在 write controlbits 的时候，不是当前 mode 的 net 如果布线成功了，结果需要保留下来。**

**当前的程序中 reuse net 的路径没有再切换模式的时候复用**

**每一轮迭代结束，把所有路过的路径使用的寄存器编号打印一下，然后看使用的寄存器的历史占用情况，还有当前的 cost 都打印出来。然后对比在相邻两个迭代轮次之间的变化，检查这个变化和指标之间有什么联系**

**HardwareRecorder::update_track_recorders_cost()只更新了路径里面的每一条 track ，但是没有更新和当前 track 在同一个寄存器里面的所有 track ，导致后续搜索同组 track 的时候这个代价并没有起到效果。还有就是每一条 track 应该准备两个 cost ，当两种不同类型的 net 需要使用这个 track 的时候分别给出对应的 cost ，如果这个 track 已经被占用了就不会被搜索到。**



