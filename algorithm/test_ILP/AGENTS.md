# PR_tool /algorithm/test_ILP 工程指南（面向 AI Agent）

本文件是 `algorithm/test_ILP/` 子工程的入口说明。目标是：在不破坏现有 ILP 功能的前提下，理解并扩展“**ILP 分配 + MCF 全局布线**”实验链路。

该目录是一个独立的算法验证入口，不直接替代 `source/algo/router/` 的正式路由流程。它强调：

- 快速构建可复现的数学模型（ILP / MCF）
- 用 HiGHS 求解并导出可解释结果
- 在 `test_ILP` 范围内隔离实验逻辑，避免污染主流程

---

## 1. 快速上手（构建 / 运行）

`test_ILP` 目标由仓库根目录 `xmake.lua` 配置，构建时会编译：

- `algorithm/test_ILP/main.cc`
- `algorithm/test_ILP/tob_ilp_model.cc`
- `algorithm/test_ILP/highs.cc`
- `algorithm/test_ILP/ilp_speedup.cc`
- `algorithm/test_ILP/cob_mcf_router.cc`

常用命令（仓库根目录）：

```bash
xmake build test_ILP
./output/test_ILP <config_path> [output_mps_path] [--enable-objective] [--enable-ilp-parallel] [--enable-mcf-global-routing] [--enable-mcf-parallel]
```

参数语义（以 `main.cc` 为准）：

- `config_path`：配置目录（例如 `algorithm/test_ILP/case1`）
- `output_mps_path`：可选，导出 ILP MPS 文件
- `--enable-objective`：启用 ILP 目标项
- `--enable-ilp-parallel`：HiGHS 并行求解 ILP
- `--enable-mcf-global-routing`：在 ILP 分配成功后继续执行 MCF 阶段
- `--enable-mcf-parallel`：在已启用 MCF 阶段时，对 16 个 `COBUnit` 的 HiGHS 求解使用 `std::async` 并发调度（每个 unit 独立模型与 `Highs` 实例）。未同时打开 `--enable-mcf-global-routing` 时该开关无效

---

## 2. 总体数据流（两阶段）

入口：`algorithm/test_ILP/main.cc` 的 `run_main()`

1) `parse::read_config` + `algo::build_nets`  
2) `build_records()`：将 `circuit::Net` 展平为 2-pin 级 `Net_cost_record`  
3) `build_cost_matrix()`：按负载估计构造成本矩阵  
4) `solve_tob_ilp_with_highs()`：ILP 求解，输出每条 2-pin net 的 `COBUnit` 分配  
5) 若启用 `--enable-mcf-global-routing`：  
   `run_mcf_global_routing_cob_units()`，按 cobunit 做 MCF 求解  

建议把该链路理解为：

- **阶段 A（ILP）**：先决定每条 2-pin net 落在哪个 `COBUnit`
- **阶段 B（MCF）**：在 COB 阵列图上按 unit/commodity 做容量约束流路由

---

## 3. 关键文件与职责

### 3.1 入口与数据预处理

- `algorithm/test_ILP/main.cc`
  - CLI 参数解析
  - 2-pin 记录构建与类型拆分
  - ILP 求解调用
  - 可选 MCF 阶段调度

### 3.2 ILP 类型与元数据

- `algorithm/test_ILP/ilp_types.hh`
  - `Bump_coord`、`Net_type`
  - `Net_cost_record`（包含 ILP 与 MCF 共用字段）
  - `map_track()`（track -> cobunit 映射）

注意：`Net_cost_record` 里新增了 MCF 依赖信息，例如：

- `origin_key`：把拆分后的 2-pin 子网回并到原始 net
- `power_kind`：`Pose / Nege / None`
- `mcf_start_kind / mcf_end_kind`
- `mcf_start_track / mcf_end_track`
- `mcf_has_start_track / mcf_has_end_track`

### 3.3 ILP 模型构建

- `algorithm/test_ILP/tob_ilp_model.hh/.cc`
  - `TobIlpModel`：行列式模型拼装
  - `build_tob_ilp_model()`：约束与目标构建
  - `to_highs_lp()`：转换为 HiGHS LP 对象

### 3.4 ILP 求解

- `algorithm/test_ILP/highs.hh/.cc`
  - `solve_tob_ilp_with_highs()`：设置 HiGHS 选项、运行求解、解析结果
  - 支持并行配置与线程信息输出

### 3.5 MCF 路由

- `algorithm/test_ILP/cob_mcf_router.hh/.cc`
  - 负责 per-cobunit MCF 的完整流水线
  - 包含 commodity 构造、图边构造、路径类别约束、HiGHS 求解

- `algorithm/test_ILP/mcf_hw_map.hh`
  - COB/TOB 线性编号与坐标映射
  - `track_to_cob()` 规则封装（用于将 track 端点映射到 COB 图节点）

---

## 4. Net 拆分与语义（必须先理解）

`build_records()` 是该目录最关键的数据标准化步骤。

### 4.1 基本类型

- `Bnet`：bump -> bump
- `Tnet`：bump -> track 或 track -> bump（都归入 Tnet）
- `PNnet`：由 `TracksToBumpsNet` 拆分得到（通常来自 pose/nege 固定网）

### 4.2 SyncNet 展平

`SyncNet` 会被拆成多条 2-pin 记录：

- `btb` -> `Bnet`
- `btt` -> `Tnet`（含 end_track）
- `ttb` -> `Tnet`（含 start_track）

并统一写入 `origin_key = 原始 SyncNet 名`，用于后续 MCF 回并。

### 4.3 bits 语义

- ILP 阶段：`bits` 仍是记录级负载权重输入
- MCF 阶段：可能按类型重新解释（例如 PN 按 TOB 分组计数）

改动时不要混淆“ILP 的成本权重”与“MCF 的 commodity demand”。

---

## 5. MCF 建模框架（当前实现）

流水线分两段（与「先 prepare 再 solve」一致）：

1) **准备（对每个 `cobunit` 一次）**：`prepare_cob_unit_commodities()` — 从 ILP assignments 筛选该 unit 的 2-pin records，用 `mcf_merge_key(origin_key + Net_type)` 分组回并，再 `build_commodities()` 生成 `McfK` 列表  
2) **求解**：拷贝 `build_base_arcs()` 得到的无端口底图，再按当前 `cobunit` 调用 `add_port_arcs()` 追加 pose/nege 到 `V_P`/`V_N` 的虚边，最后 `solve_mcf_lp()` 调 HiGHS；求解后会从整数流解中提取每个 commodity 的节点路径用于日志输出  

`add_port_arcs()` 仅接入 **`map_track(track.index) == 当前 cobunit`** 的 pose/nege 端口（与 ILP 的 COBUnit 划分一致）；`p_cob` / `n_cob` 集合与这些端口对应 COB 一致。

路径约束（模型第 4 类）：除仅允许 P-class commodity 使用 `V_P` 关联弧、N-class 使用 `V_N` 关联弧外，**非 P-class 的 commodity 禁止占用任一端点在 `p_cob` 上的边；非 N-class 禁止占用任一端点在 `n_cob` 上的边**（`arc_class_ok`）。

图节点固定为：

- COB 节点：`9 * 12 = 108`
- TOB 节点：`16`（偏移从 108 开始）
- 虚拟节点：`V_P = 124`，`V_N = 125`

边容量：

- 普通边（COB-COB / TOB-COB）：`8`
- 虚拟节点相关边：不进容量聚合行，等价无界

日志输出（便于诊断）：

- **ILP 路由细节**：`main.cc` 中明确 `j=horizontal line`、`k=vertical line`
- **MCF 按 net 视角汇总**：`run_mcf_global_routing_cob_units()` 会打印每个原始 `origin_key` net 使用到的 COBUnit 列表
- **每个 unit 的路径明细**：按 `commodity -> path#i` 打印完整节点链（`TOB* / COB(r,c) / V_P / V_N`）
- **路径长度定义**：`length` 按路径节点计数；每个 `TOB/COB` 记 `+1`，`V_P/V_N` 不计长度

---

## 6. 与主工程的边界

该目录用于算法验证，不直接承担 `source/algo/router/` 的线上职责。

强约束：

- 尽量不改 `source/` 下主流程接口语义
- 新实验字段优先放在 `algorithm/test_ILP` 内
- CLI 行为变更先在 `test_ILP` 自洽，再考虑迁移到主入口

---

## 7. 常见改动场景与建议

### 7.1 想改 ILP 目标/约束

优先修改：

- `tob_ilp_model.cc` 的 `build_tob_ilp_model()`
- 必要时同步 `main.cc` 的 record/cost 生成逻辑

### 7.2 想改 MCF 图拓扑或路径规则

优先修改：

- `cob_mcf_router.cc`（`build_base_arcs` / `add_port_arcs`（按 `cob_unit` 过滤端口）/ `arc_class_ok` / `prepare_cob_unit_commodities`）
- `mcf_hw_map.hh`（track / TOB 映射规则）

### 7.3 想改 net 回并策略

优先修改：

- `main.cc` 的 `build_records()`（保证 `origin_key` 正确）
- `cob_mcf_router.cc` 的 `mcf_merge_key()` 与 `build_commodities()`

---

## 8. 关键不变量（修改前后都要守住）

1. **ILP 回归不破坏**  
   不加 `--enable-mcf-global-routing` 时，流程与结果应可独立成功。

2. **record 与 assignment 数量一致**  
   MCF 假设 `records.size() == result.assignments.size()`。

3. **类型内语义一致**  
   同一回并组不应混合 `Bnet/Tnet/PNnet`，否则应显式报错。

4. **track 端点映射可复现**  
   `track_to_cob()` 规则必须稳定、可解释，不能引入随机性。

5. **日志可诊断**  
   每个 cobunit 的 commodity 数、可行性、目标值、耗时应有日志。

---

## 9. 最小验证清单（每次改动后）

1) `xmake build test_ILP` 成功  
2) ILP-only：

```bash
./output/test_ILP <config_path>
```

3) ILP + MCF：

```bash
./output/test_ILP <config_path> --enable-mcf-global-routing
```

可选并行验证：

```bash
./output/test_ILP <config_path> --enable-mcf-global-routing --enable-mcf-parallel
```

4) 若改了模型结构，建议附带：

- MPS/LP 导出样例
- 至少一个 case 的前后对比日志

---

## 10. 术语约定（本目录）

- **record**：`Net_cost_record`，2-pin 粒度建模单元
- **origin_key**：原始 net 标识，用于回并
- **assignment**：ILP 输出的 record -> cobunit 结果
- **commodity**：MCF 中单一供需流对象
- **cobunit**：16 个布线资源分区之一（由 `map_track()` 规则定义）

术语尽量统一，不要在同一文档或代码注释里混用“子网/边/commodity/net”而不加限定。
