## TODO

- 把 vpr 针对 high-fanout 的优化加上

- 一对多的 net 的 path_in_order() 函数需要重新实现**

- 加一个功能：如果吧其它 mode 的布线结果加进来，但是布线失败了，就把与失败的线冲突的、已经加载的线移除重布**

- GUI 还没有适配增量布线**

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
- 对应代码位置见 printControlBit.cpp:L665-L687 、 L885-L914 、 L1112-L1131 、 L1331-L1350 。
项目如何给这 8 个寄存器赋值（逻辑链路）

- 路由阶段先把每个 sink 的 linkedtrack/father/tob_* 建好（由 tryFindSinks/findSyncNet 搜索 + routing 回填树），见 routing.cpp:setPathInTree/getRouteTree 。
- printControlBit 遍历每条路由，把 track→COB 的方向写进 *_sel ：
  - setTrack2Cob(..., dirvalue=1) 表示 COB→track（输出）
  - setTrack2Cob(..., dirvalue=0) 表示 track→COB（输入）
  - 见 printControlBit.cpp:L1915-L1962 与调用处 L2398-L2401 。
- 然后把未赋值位 -1 统一补默认值（多数补 0），见 printControlBit.cpp:L2499-L2510 。
- 另外还有固定覆盖： resetintr01 把 4 个端口（71/78/85/92）在四个指定 COB 边强制置为 1（TX），见 printControlBit.cpp:L3-L16 和调用 L2514 。
- 最后把对应 *_sel 向量送进 controlbit2stringfor_padctrl/for_SiPpadctrl 生成 pad 寄存器位串，其中 ie 位由 *_sel 的 0/1 决定。 padctrl 与 SiPpadctrl 的差异核心是端口重排表不同（ cob_ext_port_index 不同），见 L116-L117 与 L172-L173 。
这 8 个寄存器写文件时遵循的方式

- 先把 128bit pad 控制串转成 32 个 hex 字符（每 4bit 一个 hex），然后按每 8 个 hex（32bit）打一行，行尾带寄存器名后缀 _0/_1/_2/_3 ，逻辑是 if (i>0 && (i+1)%8==0) name=i/8 ，见 printControlBit.cpp:L673-L677 （其余 7 个同模式）。
- 四个方向分别落在四个分区文件里：
  - left 在 botleft_controlbit.txt （见打开文件 L469 ）
  - up 在 topleft_controlbit.txt （ L698 ）
  - down 在 botright_controlbit.txt （ L925 ）
  - right 在 topright_controlbit.txt （ L1144 ）







