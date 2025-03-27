# TODO

- tobmux 的 controlbits 在输出的时候，是先把128个int存在_rv里面(fetch())，然后拆成16组*8个int，拆的时候就已经反过来了（split_array）。然后把每组反过来的8个int(MSB在左侧)从左往右逐个翻译成三位二进制，再把三位二进制存进result里面，存的时候这三位又取反（write_tob_template_mux）;
作用相当于先把8个int翻译成二进制然后再整体反过来

- tobconnector 没有修改到interposer 里面方向寄存器的值。tob 的bump 给出去以后消失了，不在interposer的tob里面

- 在 bits_to_path 的时候，应该传入需要 laod 的 net ，因为如果加载的是相邻 mode 的 controlbits ，那么只要 load 两个 mode 有重合的那些 path

- 测试布局
- 布局部分的 GUI 
- 补充文档
 