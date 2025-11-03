## TODO

- 把 vpr 针对 high-fanout 的优化加上

- 原来对于增量布线的问题定义不合理，仅把可复用/不复用的线分开到不同的寄存器不能起到效果，同时应该加上全局总线长和关键路径尽可能小的目标

    不可复用的线网走在同一个寄存器里，可以减少修改寄存器的次数。

- 毕业论文里面举得算法例子说明代价函数没有办法起到分割的效果

- 原来算法更新代价的时机不合理，在 PathFinder 当中是每连完一条 net 就更新一次代价

- 首先在 preroute 得到一个新的 pathpackage 的时候，是存在分立的各个一对一小线网自己的 package 里面的，没有移动到 SyncNet 里面的 pathpackage 里面去的。所以 SyncNet 的 package 里面的内容还是旧的，但是小的 package 里面的内容是新的。但是二者使用的是同一个 TOB 上的 register 资源，所以其实都是重复的 register ，因为使用指针存的，指向的是同一个 register 。然后再 collect_package() 这个函数里面，this->_path_package.clear_all() 这一步看上去是清空了 SyncNet 自己的 package ，但是连带小的、最新的 register 一起清空了。这个是最主要的原因。当前的解决方式就是把 clear_all() 删了。

**使用 history package 替换最终结果会在 connect 的过程中出错。**

**一对多的 net 的 path_in_order() 函数需要重新实现**

**加一个功能：如果吧其它 mode 的布线结果加进来，但是布线失败了，就把与失败的线冲突的、已经加载的线移除重布**

**GUI 还没有适配增量布线**

**当前版本在output文件夹里面有自己 mode 的布线结果的时候会不布线。有其它mode布线结果的时候暂时把bits_to_path禁用了**

**算法需要加一个收敛的条件**






