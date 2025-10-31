## Incremental routing


### 普通布线算法

- 假设有多个布线模式：

  mode1 有 net1, net2, net3
  mode2 有 net1, net2, net4
  mode3 有 net1, net5

- 输入部分：

  - 命令行输入方式加一个参数：-i \<mode\>

  - 检测对应 mode 的 controlbits_\<mode\>.txt 文件

  - 把 bits 转换成程序的路径数据结构

- 输出部分：

  - 输出 controlbits_\<mode\>.txt 文件

- 布线过程

  - 不存在已有路径的时候

    - 针对 controlbits 绑定的情况，优先将【复用的线】和【不复用的线】分配不同的绑定组

        - 给每一个硬件单元设置两个属性，一个记录当前占用的线网类型，一个记录布线历史上分别被两种类型的线网使用了多少次

        - 设置启发式的 cost 函数

        - 用比值的形式计算 cost （待调整）

    - 针对布线资源冲突导致布线失败的情况

        - 当前的失败主要存在于 tob 上，给 bump 分配一些 tobmux 使其连到某条 track 的过程

        - 极端情况下，允许共享

        - 给每个 tobmux 定义计数器，记录被共享的次数，并将共享次数转化为 cost 

    - 终止条件的判断

        - 给定 while 的最大迭代次数

        - 如果到达最大迭代次数，还存在共享，就增加迭代
    
    第一个版本：考虑共享
    ```c++
    load nets
    sort nets by reuse frequency
    while:
        for nets:
            path = route net with maze(using the new cost function)
            if fail:
                path = allow sharing, reroute net with maze(using the new cost function)
            endif
            update attributes of hardware resources in path
        endfor

        update global recorder, including shared times of each tobmux

        if (MAX_CYCLE_NUMBER is reached) and (no tobmux is shared):
            return success
        else if (MAX_CYCLE_NUMBER+additional_cycle_number is reached)
            return success if no tobmux is shared else failure
        endif
    endwhile
    ```

    第二个版本：不考虑共享
   ```c++
    cycle = 0
    if (load_all_nets) {
      load nets from all modes
    }
    else {
      load nets in this mode
    }
    sort nets by reuse frequency（用 reuse frequency 排序）
    while (true) {
        for net in nets {
            path = route net with maze(using the new cost function)
        }
        
        if fail {
            if (cycle == 0) {
                remove nets in other modes
                reroute
            }
            else {
                return success with path from last cycle
            }
        }
            
        update recorder        

        if (cycle >= MIN_CYCLE_NUMBER) {
            return success with path
        }

        cycle++
    }
    if (load_all_nets) {
      output controlbits in all modes
    }
    else {
      output controlbits in this modes
    }
    ```


### 硬件要求

- 注意 TOB 当中，每个 bank 都可以视作独立的分区，可进行独立配置。例如在高并行的需求下激活多个 bank ，而在低功耗的需求下减少激活的 bank；或者是部分 bank 损坏的时候使用其它 bank 以提高容错

- TOB拟引入双向缓冲器结构。这些双向缓冲器能够在特定情况下断开TOB与track的连接，从而便于其他信号自由通过。双向缓冲器不仅支持信号的双向传输，还在信号路径断开时有效隔离信号，确保其他路径的信号稳定传输。这意味着，当某一特定信号路径不再需要时，系统可以通过断开连接减少功耗，同时双向缓冲器的存在确保其他信号路径的传输不会受到干扰。

### 增量式

- 场景上，可以考虑高性能计算下的低延迟通路需求与低功耗运算下的节能互连
  **这个在硬件上还没发实现**


### 硬件 controlbits 

- 通过上位机将添加地址的controlbits文件逐行发送给芯粒，一行是一个寄存器，包括寄存器的值和地址信息。interposer 接收一个寄存器的信息之后根据地址将整个寄存器写入，即一次写一整个

- 可以只修改某一个寄存器的值


### 一些已有的工具

1. [参考资源：VPR工具](https://www.eecg.utoronto.ca/~vaughn/vpr/vpr.html)

  - 这个工具也是采用迷宫算法布线，并结合 PathFinder 的协商策略

  - 提到一个针对 **high-fanout net** 的算法优化，即每一次找到 sink 之后并连上一段路径后，不要清空 wavefront ，而是将新连上的路径以 cost=0 加入 wavefront ，然后正常进行扩展。如果新加入的路径的邻居的 cost 比原来 wavefront 里面的 cost 小，就更新这个邻居的 cost 。这样可以避免从头搜索。







  