# 布线顺序调整策略：在普通布线优先级的基础上，叠加复用频率用于调整大级别相同的 net 之间的布线顺序

```C++
float max_reuse_times {0.0};
for (const auto& net : nets) {
    max_reuse_times = std::max(max_reuse_times, (float)net->modes().size());
}
for (auto& net : nets) {
    net->update_priority(0.9 * ((float)net->modes().size() / max_reuse_times));
}
```

## 2025.10.1_1 实验

- 使用 case 18 实验，跑 mode 1，自动运行 30 轮，CUT_RATE = 0.2。通过失败记录或者同步线的重布线过程反应布线顺序的调整策略是否合适

    ```C++
    PLEASE_DO_NOT_FAIL_INCRE(18, "modified case18 to with more reusable/uon-reusable nets be mixed together", 1, 30);
    ```

- 在 refinding 的过程中峰值内存占用超过 15G ，PC 机承受不了，实验在第 7 轮终止

- 实验结果

    * 总线长有两种差异很大的结果，一种是 656 ，一种是 756 。两种差在同步线的长度上。例如第 6 轮和第 7 轮：

        - 第 6 轮（按顺序统计）

            BumpToTrackNet_from_cob_1_0_to_cob_9_5_in_group_7 有 4 根同步线，长度是 15
            BumpToBumpNet_from_cob_1_6_to_cob_1_0_in_group_1 里面有 12 根同步线，长度是 10
            BumpToBumpNet_from_cob_3_0_to_cob_1_3_in_group_3 里面有 12 根同步线，长度是 8
            BumpToBumpNet_from_cob_1_0_to_cob_3_0_in_group_5 里面有 12 根同步线，长度是 5
            BumpToBumpNet_from_cob_1_9_to_cob_3_6_in_group_6 里面有 20 根同步线，长度是 8
            BumpToBumpNet_from_cob_1_3_to_cob_3_3_in_group_4 里面有 20 根同步线，长度是 5
            BumpToTrackNet_from_cob_1_3_to_cob_9_5_in_group_-1 长度是 12
            TrackToBumpNet_from_cob_9_5_to_cob_1_3_in_group_-1 里面有四根线，长度都是 12

        - 第 5 轮 

            BumpToTrackNet_from_cob_1_0_to_cob_9_5_in_group_7 有 4 根同步线，长度是 15
            BumpToBumpNet_from_cob_1_6_to_cob_1_0_in_group_1 里面有 12 根同步线，长度是 10
            BumpToBumpNet_from_cob_3_0_to_cob_1_3_in_group_3 里面有 12 根同步线，长度是 8
            BumpToBumpNet_from_cob_1_0_to_cob_3_0_in_group_5 里面有 12 根同步线，长度是 5
            BumpToBumpNet_from_cob_1_9_to_cob_3_6_in_group_6 里面有 20 根同步线，长度是 12（重连过）
            BumpToBumpNet_from_cob_1_3_to_cob_3_3_in_group_4 里面有 20 根同步线，长度是 5
            BumpToTrackNet_from_cob_1_3_to_cob_9_5_in_group_-1 长度是 12
            TrackToBumpNet_from_cob_9_5_to_cob_1_3_in_group_-1 里面有四根线，长度都是 12

    * 没有失败记录


## 2025.10.1_2 实验

- 使用 case 18 实验，跑 mode 1，自动运行 30 轮，CUT_RATE = 0.4。通过失败记录或者同步线的重布线过程反应布线顺序的调整策略是否合适

    ```C++
    PLEASE_DO_NOT_FAIL_INCRE(18, "modified case18 to with more reusable/uon-reusable nets be mixed together", 1, 30);
    ```

- 在 refinding 的过程中 vscode 崩溃，实验在第 10 轮终止

- 实验结果

    * 总线长全部是 656 ，比前一次实验好。说明 CUT_RATE 大一点可以给重布线的时候提供更多的重新连接的空间，提高重新连接的质量

    * 没有失败记录

    * 实验过程发现，在重新连接的过程中内存占用会以稳定的步长（约80M）增加，然后到一定数值的时候就迅速降低，再重复前述过程。每一次启动这个过程都会在 log 里面打印一个 refinding path ... 。这个是因为找到路径了然后返回之后一下子把整个搜索树全部清空，然后又开始重连下一根


## 2025.10.1_3 实验

- 使用 case 19 实验，跑 mode 1，自动运行 30 轮，CUT_RATE = 0.5。通过失败记录或者同步线的重布线过程反应布线顺序的调整策略是否合适

    ```C++
    PLEASE_DO_NOT_FAIL_INCRE(19, "modified case18 to with more reusable/uon-reusable nets be mixed together", 1, 30);
    ```

- 在 refinding 的过程中峰值内存占用超过 15G ，PC 机承受不了，实验在第 4 轮终止

- 实验结果

    * 每一轮都有 fail nets

    * group 9 的线网重布线可以成功，group 6 的线网重连的过程中容易一直找不到长度合适的路径导致持续搜索，使内存占用率过高。group 6 导致的问题和前两次实验一致


## 2025.10.1_4 实验

- 使用 case 19 实验（删除了 group 6 的线网），跑 mode 1，自动运行 30 轮，CUT_RATE = 0.5。通过失败记录或者同步线的重布线过程反应布线顺序的调整策略是否合适

    ```C++
    PLEASE_DO_NOT_FAIL_INCRE(19, "modified case19 to with more reusable/uon-reusable nets be mixed together", 1, 30);
    ```

- 实验正常结束

- 实验结果

    * 实验过程中发现内存占用一开始比较低且稳定，然后开始以稳定的步长逐渐升高，和前几次实验在重布线时的升高过程相同。升高到一个较高值后又保持稳定，和一开始的稳定状态差不多。推测升高原因是在重布线，重连之后没降下来是发生了内存泄露。

    * 每一次布线都有失败记录，失败率 100%


# 布线顺序策略：纯粹依靠复用频率调整

```C++
auto compare = [] (circuit::Net* n1, circuit::Net* n2) -> bool {
    return n1->modes().size() > n2->modes().size();
};
std::sort(nets.begin(), nets.end(), compare);
for (auto& net: nets) {
    net->modes().size() > 1 ? net->set_reuse_type(true) : net->set_reuse_type(false);
}
```

## 2025.10.1_5 实验

- 使用 case 19 实验（删除了 group 6 的线网），跑 mode 1，自动运行 30 轮，CUT_RATE = 0.5。通过失败记录或者同步线的重布线过程反应布线顺序的调整策略是否合适

- 实验正常结束

- 实验结果

    - 30 轮的平均全局总线长和同步线平均线长与前一次实验相比下降了一点

    - 寄存器占用情况基本相同

    - 这次实验总体比上面更优，但是由于两次都是 100% 的含失败率，所以区分不出效果


