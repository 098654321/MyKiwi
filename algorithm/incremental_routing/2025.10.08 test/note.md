## 检查 has_nonreuse 和实际修改寄存器的数量之间的关系

- 运行 20 轮测试

```C++
PLEASE_DO_NOT_FAIL_INCRE(18, "modified case18 to with more reusable/uon-reusable nets be mixed together", 1, 1);
PLEASE_DO_NOT_FAIL_INCRE(18, "modified case19 to with more reusable/uon-reusable nets be mixed together", 2, 1);
parse::compare("controlbits_1.txt", "controlbits_2.txt");
```

    mode1_has_nonreuse: [186, 228, 204, 220, 227, 222, 212, 216, 208, 223, 201, 223, 235, 211, 225, 215, 239, 186, 251, 239]
    mode2_has_nonreuse: [260, 234, 218, 201, 239, 199, 229, 205, 200, 199, 232, 186, 213, 245, 222, 202, 234, 206, 209, 213] 
    修改的寄存器数量: [451, 591, 482, 473, 542, 444, 489, 670, 451, 468, 472, 432, 452, 427, 467, 449, 420, 459, 443, 447]

- 发现nonreuse 数据和修改的寄存器数量之间的线性相关性很弱，所以不适合用 nonreuse 衡量最终的布线结果

- 
