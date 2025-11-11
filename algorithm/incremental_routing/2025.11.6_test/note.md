## test

- 实验目的：实现收敛条件后的测试，检测收敛阈值是否合理

- 实验设置

    * 参数设置

    ```C++
    const float BASICCOST = 5;
    const float EPSILON = 1;
    const float GROUPCOEF = 0.1;
    const float HISTORYCOEF = 0.9;
    const float TOBMUXGROUPSIZE = 8;
    const float TRACKGROUPSIZE = 32;

    auto history_ratio = (h_reuse_n-h_nonre_n) / (h_reuse_n+h_nonre_n+EPSILON);
    auto group_ratio = (reuse_num-nonre_num) / (nonre_num + reuse_num + EPSILON);

    this->_cost_reuse = BASICCOST * (1-GROUPCOEF*group_ratio-HISTORYCOEF*history_ratio);
    this->_cost_nonreuse = BASICCOST * (1+GROUPCOEF*group_ratio+HISTORYCOEF*history_ratio);
    ```

    * 测试数据

        case 20 

- 测试内容

    * 对 mode1 的测试

        - 先跑 mode2 到收敛，保留 controlbits_2.txt 作为对比的标准

        - 检测一下用这个文件作为前置路径跑 mode1 能不能行，不行就重来

        - 跑 5 遍 mode1 ，每一遍跑到收敛，测试全局线长、同步线长、最大线长。每一遍保留 debug.log 和 controlbits_1.txt

        - 使用上面的五个 controbits 文件和标准文件对比得到寄存器差异数量

        - 结果

            |序号|全局线长|同步线长|最大线长|切换的寄存器数量|
            |:--:|:--:|:--:|:--:|:--:|
            |1|3245|6.300971|15|1645|
            |2|3265|6.3398056|15|1693|
            |3|3245|6.300971|15|1627|
            |4|3309|6.425243|15|1651|
            |5|3245|6.300971|15|1677|
            |AVG|3261.8|6.33359232|15|1658.6|
            
    * 对 mode2 的测试

        - 跑 mode1 的结果作为标准对比文件，然后其余内容和上面一摸一样

        - 结果

            |序号|全局线长|同步线长|最大线长|切换的寄存器数量|
            |:--:|:--:|:--:|:--:|:--:|
            |1|3157|6.130097|15|1757|
            |2|3317|6.440777|15|1938|
            |3|3217|6.246602|15|1826|
            |4|3269|6.347573|15|1887|
            |5|3117|6.0524273|15|1721|
            |AVG|3215.4|6.24349526|15|1825.8|

    * 使用第一个测试当中 controlbits_2.txt 的内容作为已有路径结果，跑 5 遍 mode1 ，对比寄存器差异数量

    * 使用第二个测试当中 controlbits_1.txt 的内容作为已有路径结果，跑 5 遍 mode2 ，对比结果

        |序号|全局线长|同步线长|最大线长|切换的寄存器数量|
        |:--:|:--:|:--:|:--:|:--:|
        |1|3301|6.4097085|15|954|
        |2|3301|6.4097085|15|956|
        |3|3301|6.4097085|15|982|
        |4|3301|6.4097085|15|944|
        |5|3301|6.4097085|15|934|
        |AVG|3301|6.4097085|15|954|
