## TODO

- 增量布线算法调整：

  - 目前的迭代在case21当中无法收敛，需要检查无法收敛的原因，调整收敛条件

  - 算法需要改一下。从channel的角度，可以根据两个布线场景当中需要切换的那些net的bounding box设计一个中间点，最大化共享的路径；从track的角度，还需要具体根据cobunit group设计track的分配策略

--- 

- 把 vpr 针对 high-fanout 的优化加上

- 一对多的 net 的 path_in_order() 函数需要重新实现

- 加一个功能：如果吧其它 mode 的布线结果加进来，但是布线失败了，就把与失败的线冲突的、已经加载的线移除重布

- GUI 还没有适配增量布线**

---

- 生成xinzhai寄存器的逻辑：

    这 8 个寄存器在项目中的含义

- 它们是 C4 NOI 四个边（left/up/down/right）各两套 pad 控制寄存器：一套 noi_* ，一套 noi_SiP_* 。
- 本质都在表达 pad 方向等控制字段，最关键是 ie （输入使能方向）：注释定义为“ext->cob 时 ie=1，cob->ext 时 ie=0”，见 controlbit2stringfor_padctrl 与 controlbit2stringfor_SiPpadctrl 。
- 8 个寄存器与 COB 侧选择向量的对应关系：
  - xinzhai_C4_noi_left_pad_ctrl ← cobcontrolbit[8][2].right_sel
  - xinzhai_C4_noi_SiP_left_pad_ctrl ← cobcontrolbit[4][0].down_sel
  - xinzhai_C4_noi_up_pad_ctrl ← cobcontrolbit[0][1].left_sel
  - xinzhai_C4_noi_SiP_up_pad_ctrl ← SiP_noi_up （主要来自 cobcontrolbit[0][5].left_sel ，并覆盖 0/8/16/24 位）
  - xinzhai_C4_noi_down_pad_ctrl ← cobcontrolbit[8][10].right_sel
  - xinzhai_C4_noi_SiP_down_pad_ctrl ← cobcontrolbit[8][6].right_sel
  - xinzhai_C4_noi_right_pad_ctrl ← cobcontrolbit[2][11].up_sel
  - xinzhai_C4_noi_SiP_right_pad_ctrl ← cobcontrolbit[0][8].left_sel

- padctrl

  - 见printControlBit.cpp文件的controlbit2stringfor_padctrl函数

  - 输出字符串的赋值逻辑：共128位，从左到右分别是0～127。赋值过程从右到左，先给127～96赋值1，再给95~64赋值0，63～48根据cob_extport_index[0~15]处的端口索引提取*_sel的值决定，如果值表示ext to cob（也就是rightsel==0），赋值1，否则赋值0。47～32赋值0，31～0赋值0。转换成16进制的过程中从左侧索引0开始转换，每次向右取4位二进制数，高位在左

  - cob_ext_port_index = [71, 9, 78, 18, 85, 0, 92, 27, 45, 99, 54, 106, 36, 113, 63, 120]

  - 最终输出16为16进制数的时候，分为两组，8位一组。都是高位在左

  - 对应关系：
    
    - left->cobcontrolbit[8][2].right_sel

    - up->cobcontrolbit[0][1].left_sel

    - down->cobcontrolbit[8][10].right_sel

    - right->cobcontrolbit[2][11].up_sel


- SiPadctrl

  - 见printControlBit.cpp文件的controlbit2stringfor_SiPpadctrl函数

  - 输出字符串的赋值逻辑：共128位，从左到右分别是0～127。赋值过程从右到左，先给127～96赋值1，再给95~64赋值0，63～48根据cob_extport_index[0~15]处的端口索引提取*_sel的值决定，如果值表示ext to cob（也就是rightsel==0），赋值1，否则赋值0。47～32赋值0，31～0赋值0。转换成16进制的过程中从左侧索引0开始转换，每次向右取4位二进制数，高位在左

  - cob_ext_port_index = [0, 9, 18, 27, 36, 45, 54, 63, 71, 78, 85, 92, 99, 106, 113, 120]

  - 最终输出16为16进制数的时候，分为四组，8位一组。都是高位在左

  - 对应关系：

    - left->cobcontrolbit[4][0].down_sel
    
    - up->先取cobcontrolbit[0][5].left_sel，把[0][8][16][24]位置上的值分别换成controlbit[0][4].left_sel的[0][8][16][24]

    - down->cobcontrolbit[8][6].right_sel

    - right->cobcontrolbit[0][8].left_sel

- cobcontrolbit 的赋值：

  - 初始化为-1

  - 根据信号通信方向，如果是从cob 到 track，就赋值1；如果是track到cob，就赋值0.和上面输出字符串的赋值逻辑正好反过来

  - cob 阵列是9*12，排列方式和现在一样









