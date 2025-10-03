## TODO

- 把 vpr 针对 high-fanout 的优化加上

- 原来对于增量布线的问题定义不合理，仅把可复用/不复用的线分开到不同的寄存器不能起到效果，同时应该加上全局总线长和关键路径尽可能小的目标

    不可复用的线网走在同一个寄存器里，可以减少修改寄存器的次数。

- 毕业论文里面举得算法例子说明代价函数没有办法起到分割的效果

- 原来算法更新代价的时机不合理，在 PathFinder 当中是每连完一条 net 就更新一次代价

- 首先在 preroute 得到一个新的 pathpackage 的时候，是存在分立的各个一对一小线网自己的 package 里面的，没有移动到 SyncNet 里面的 pathpackage 里面去的。所以 SyncNet 的 package 里面的内容还是旧的，但是小的 package 里面的内容是新的。但是二者使用的是同一个 TOB 上的 register 资源，所以其实都是重复的 register ，因为使用指针存的，指向的是同一个 register 。然后再 collect_package() 这个函数里面，this->_path_package.clear_all() 这一步看上去是清空了 SyncNet 自己的 package ，但是连带小的、最新的 register 一起清空了。这个是最主要的原因。当前的解决方式就是把 clear_all() 删了。**但是还有一个问题，增量布线失败的时候不能使用 history_package 作为结果，因为里面的 give_out 的值已经在当前轮次被修改的和 output_index 不一样了。可以把存储 history package 的方式改一改，用数值把 tobmuxregister 里面的东西都先存下来，不要直接 history_package = package**

**改一下回归测试里面debug.log不能准确记录每一次运行信息的问题**

**处理 RetryExcept 的方法有点问题，不能放在route_nets.cc 里面，要放在 incremental_test.cc 里面.一条线失败了可以考虑把使用的资源都退掉，然后后面的还可以接着布线**

**布线顺序的方法需要设计一个能反映失败率变化的例子进行测试**

