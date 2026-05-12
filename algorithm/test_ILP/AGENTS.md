# PR_tool /algorithm/test_ILP 工程指南（面向 AI Agent）

本文件是 `algorithm/test_ILP/` 子工程的入口说明。目标是：在不破坏现有 ILP 功能的前提下，理解并扩展"**ILP 分配 + MCF 全局布线**"实验链路。

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
- `algorithm/test_ILP/ilp_reach_precompute.cc`
- `algorithm/test_ILP/cob_mcf_router.cc`
- `algorithm/test_ILP/ilp_maze_search.cc`
- `algorithm/test_ILP/ilp_maze_finalize.cc`

常用命令（仓库根目录）：

```bash
xmake build test_ILP
./output/test_ILP <config_path> [output_mps_path] [-v|-vv|...] [--enable-ilp-parallel] [--cob-rows N --cob-cols M] [--enable-mcf-routing] [--enable-mcf-parallel] [--enable-maze]
```

参数语义（以 `main.cc` 为准）：

- `config_path`：配置目录（例如 `algorithm/test_ILP/case1`）
- `output_mps_path`：可选，导出 ILP MPS 文件
- `-v` / `-vv` / …：设置 verbose 模式（`-v` 计数越多越详细），启用后将 debug level 设为 `Debug`
- `--enable-ilp-parallel`：HiGHS 并行求解 ILP
- `--cob-rows N` / `--cob-cols M`：可选，**必须成对出现或均省略**。省略时 MCF 构图使用 `hardware::Interposer::COB_ARRAY_HEIGHT` 与 `COB_ARRAY_WIDTH`。若显式传入，数值必须与上述常量完全一致，否则程序报错退出（保证 `track_to_cob`、Bump/TOB 坐标与 MCF 物理假设一致）
- `--enable-mcf-routing`：在 ILP 分配成功后继续执行 MCF 阶段
- `--enable-mcf-parallel`：CLI 仍接受此开关，但当前实现中 `run_mcf_global_routing_cob_units()` 内部 `(void)enable_mcf_parallel;`，即**并行未生效**
- `--enable-maze`：**当前已禁用**（代码中 `enable_maze = true` 被注释掉，仅打印 `"enable_maze is disabled"`）。设计上该阶段在 ILP+MCF 均成功后按 bit 粒度执行受 MCF 路径走廊约束的 track 层 BFS maze；必须与 `--enable-mcf-routing` 同时使用

---

## 2. 总体数据流（两阶段）

入口：`algorithm/test_ILP/main.cc` 的 `run_main()`

1) `parse::read_config` + `algo::build_nets`  
2) `build_records()`：将 `circuit::Net` 展平为 2-pin 级 `Net_cost_record`，并分配 `record_id` 和 `bit_id`  
3) `precompute_reach_for_records()`：为每条 record 预计算可达 end_track / start_track 边集及 Wilton 转弯步序列（`IlpReachStep`），返回 `IlpReachPrecomputeStats` 统计信息  
4) 可选 `write_mps_file()`：导出 MPS 文件  
5) `solve_tob_ilp_with_highs()`：ILP 求解，输出每条 2-pin net 的 `COBUnit` 分配、W/S/QS/QW 决策变量值、`record_track_endpoints`（每条 record 对应的 start_track / end_track）  
6) 若启用 `--enable-mcf-routing`：  
   `run_mcf_global_routing_cob_units()`，在 track 级全局图上做两阶段 MCF 求解  

建议把该链路理解为：

- **阶段 A（ILP）**：先决定每条 2-pin net 落在哪个 `COBUnit`，同时确定每个 bump 对应的 track
- **阶段 B（MCF）**：在 track 级全局图上按 commodity 做容量约束整数流路由，分 BusMCF（等长约束）和 SimpleMCF（残余容量）两阶段

---

## 3. 关键文件与职责

### 3.1 入口与数据预处理

- `algorithm/test_ILP/main.cc`
  - CLI 参数解析（含 verbose `-v` 计数）
  - 2-pin 记录构建（`build_records`）与类型拆分（`classify_net`）
  - 可达性预计算调度（`precompute_reach_for_records`）
  - ILP 求解调用
  - ILP 结果输出（`route_details`、`active_w`、`active_s`）
  - 可选 MCF 阶段调度

### 3.2 ILP 类型与元数据

- `algorithm/test_ILP/ilp_types.hh`
  - `Bump_coord`、`Net_type`（`Bnet` / `Tnet` / `PNnet`）
  - `IlpPowerKind`（`None` / `Pose` / `Nege`）、`IlpEndpointKind`（`Bump` / `Track`）
  - `IlpReachStep`（Wilton 转弯步：`from_dir`、`to_dir`、`index_in`、`index_out`）
  - `Net_cost_record`（包含 ILP 与 MCF 共用字段）
  - `map_track()`（track -> cobunit 映射）

`Net_cost_record` 关键字段：

- `origin_key`：把拆分后的 2-pin 子网回并到原始 net
- `record_id`：`build_records` 输出序中的全局唯一 id（用于 ILP/MCF/maze 对齐）
- `bit_id`：同一 `origin_key` 内的位序号
- `power_kind`：`Pose / Nege / None`
- `mcf_start_kind / mcf_end_kind`
- `mcf_start_track / mcf_end_track`
- `mcf_has_start_track / mcf_has_end_track`
- `pn_end_tracks` / `pn_end_track_coord_by_index`：PNnet 的所有 0/1 端口 track
- `end_tracks`：可达性预计算后的 end track 列表
- `starttrack_by_endtrack`：每个 end_track 对应的可达 start_track 列表
- `reach_by_end_start`：每个 `(end_track, start_track)` 对对应的 Wilton 转弯步序列

### 3.3 ILP 模型构建

- `algorithm/test_ILP/tob_ilp_model.hh/.cc`
  - `TobIlpModel`：行列式模型拼装
  - `build_tob_ilp_model()`：约束与目标构建
  - `to_highs_lp()`：转换为 HiGHS LP 对象

ILP 变量体系：
- `W(bump, j, k)`：bump 到 (j, k) 的分配决策
- `S(tob, v)`：TOB 的 s 寄存器决策（`v = j*8 + k`）
- `QS(bump, j, k)` = W ∧ S（线性化乘积，对应 straight 路径）
- `QW(bump, j, k)` = W ∧ ¬S（线性化乘积，对应 wrap 路径）
- `Y(n, r_end)`：PNnet n 选择 end_track r_end 的决策

ILP 约束组：
- 约束 1（`R_WONE`）：每个 active bump 恰好选一个 (j, k)
- 约束 2（`R_HORI`）：同一 (TOB, Bank, Group) 内的水平线 j 至多被 1 个 Index 使用
- 约束 3（`R_VERT`）：同一 (TOB, Bank) 内的 (j, k) 组合至多被 1 个 Group 使用
- QS/QW 线性化约束（`R_QS1/2/3`、`R_QW1/2/3`）
- Bnet 可达性（`R_BEND0`、`R_BREACH`）：基于 `starttrack_by_endtrack` 预计算结果
- Tnet 可达性（`R_TREACH0`）：基于固定 end_track 的 start_track 可达集
- PNnet 端口选择（`R_PNYSUM`、`R_PNREACH`）：恰好选 1 个 end_track，且 start_track 必须在可达集内

### 3.4 ILP 求解

- `algorithm/test_ILP/highs.hh/.cc`
  - `TobIlpResult`：包含 `assignments`、`active_w`、`active_s`、`route_details`、`record_track_endpoints`
  - `TobIlpRecordTrackEndpoint`：每条 record 的 `(record_id, cob_unit, start_track, end_track)` 元组
  - `solve_tob_ilp_with_highs()`：设置 HiGHS 选项、运行求解、解析 W/S/QS/QW 决策变量、推导 track 和 cobunit、构建 `record_track_endpoints`
  - 支持并行配置与线程信息输出

### 3.5 MCF 路由

- `algorithm/test_ILP/cob_mcf_router.hh/.cc`
  - 负责 track 级全局 MCF 的完整流水线
  - 入口函数 `run_mcf_global_routing_cob_units()`
  - 内部关键步骤：`build_track_graph()` → `prepare_commodities()` → `solve_stage()` × 2（Bus + Simple）
  - `GlobalGraph`：track 级全局路由图（节点 = `(unit, cob, track)` 三元组 + VP/VN 虚拟节点）
  - `PreparedCommodity`：每个 commodity 的源/汇节点、类别（`Plain`/`P`/`N`）、bus 标识、reach 步序列、bbox
  - `StageSolveResult`：单阶段求解结果（已用边/节点、路径）
  - `arc_usable_for_class()`：按 `unit` 和 P/N 类别过滤 arc
  - `extract_path()`：从整数流解中通过 BFS 提取单 commodity 路径

- `algorithm/test_ILP/mcf_hw_map.hh`
  - COB/TOB 线性编号与坐标映射
  - `track_to_cob()` 规则封装（用于将 track 端点映射到 COB 图节点）
  - `tob_pair_cob_coords()`：TOB 对应的两个相邻 COB 坐标

### 3.6 ILP 加速辅助

- `algorithm/test_ILP/ilp_speedup.hh/.cc`
  - `cobunit_to_tracks()`：给定 cobunit 返回其包含的 8 条 track 列表（`bank*64 + g*8 + unit_local`，`g` ∈ 0..7）
  - `track_to_jk()`：track -> `(j, k)` 坐标映射

### 3.7 可达性预计算

- `algorithm/test_ILP/ilp_reach_precompute.hh/.cc`
  - `precompute_reach_for_records()`：在 ILP 求解前为每条 record 预计算可达 end_track / start_track 边集，并通过 Wilton 转弯映射（`hardware::COBUnit::index_map`）生成 `IlpReachStep` 序列
  - 分三种空间关系处理：vertical（同列直通）、horizontal（水平两步转弯）、diagonal（对角多步转弯）
  - 预计算结果写入 `record.starttrack_by_endtrack` 和 `record.reach_by_end_start`，供 ILP 约束和 MCF reach 约束共同使用
  - 返回 `IlpReachPrecomputeStats` 统计信息

### 3.8 Maze 搜索与终局（当前禁用）

- `algorithm/test_ILP/ilp_maze_search.hh/.cc`
  - `collect_accessible_cobs()`：从 MCF 节点路径提取可访问 COB 坐标集
  - `maze_search_accessible()`：在受 MCF 路径走廊约束的 COB 可达范围内执行 track 层 BFS maze

- `algorithm/test_ILP/ilp_maze_finalize.hh/.cc`
  - `run_ilp_maze_finalize()`：ILP + MCF 均成功后的终局入口；按 bit 粒度调度 maze 搜索并输出最终 track 级路径
  - 注意：当前 `main.cc` 中 `--enable-maze` 已被禁用，MCF 阶段本身已直接输出 track 级路径

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

### 4.3 TracksToBumpsNet 拆分

`TracksToBumpsNet` 按 end_bump 拆成多条 `PNnet`：每条 PNnet 共享所有 begin_tracks 作为 `pn_end_tracks`，`power_kind` 由 net 名称推断（`"Pose nets"` → `Pose`，其余 → `Nege`）。

### 4.4 record_id 与 bit_id

`build_records()` 末尾为每条 record 赋值：
- `record_id`：按输出顺序的全局唯一 id（0, 1, 2, …）
- `bit_id`：同一 `origin_key` 内的位序号（0, 1, 2, …），用于按 bit 粒度对齐

### 4.5 bits 语义

- ILP 阶段：`bits` 仍是记录级负载权重输入
- MCF 阶段：可能按类型重新解释（例如 PN 按 TOB 分组计数）

改动时不要混淆"ILP 的成本权重"与"MCF 的 commodity demand"。

---

## 5. MCF 建模框架（当前实现：track 级全局图 + 两阶段求解）

当前 MCF 实现已从 COB 级粗粒度图重构为 **track 级全局图**，并采用 **两阶段求解**策略。

### 5.1 Track 级全局图构建（`build_track_graph()`）

图节点（`NodeMeta`）：

- **物理节点**：每个节点对应 `(unit, cob, track)` 三元组。16 个 COBUnit × 每 unit 8 条 track = 每个 COB tile 128 个节点。总物理节点数 = `128 × num_cob`
- **虚拟节点**：`V_P`（`virtual_kind=1`）和 `V_N`（`virtual_kind=2`），用于 pose/nege commodity 的汇聚

图边（`Arc`）：

- **Mesh 直通边**（`is_turn=false`）：相邻 COB tile 之间、同一条 track 的双向连接。同一 unit 内，垂直相邻 `(r, c)↔(r+1, c)` 和水平相邻 `(r, c)↔(r, c+1)` 各一对有向边
- **Wilton 转弯边**（`is_turn=true`）：同一 COB 内部不同方向 track 之间的有向连接，通过 `hardware::COBUnit::index_map(from, inner, to)` 计算映射。转弯只改变 inner index，不改变 bank 和 unit_local
- **虚拟边**（`is_virtual=true`）：由 `prepare_commodities()` 按需添加，连接 PNnet 的 end_track 节点到 `V_P`/`V_N`

边去重：`directed_arc_set` 保证同一 `(u, v)` 有向边不重复添加。

### 5.2 Commodity 准备（`prepare_commodities()`）

从 ILP 的 `record_track_endpoints` 和 `records` 构建 `PreparedCommodity` 列表：

- **src 节点**：若 record 有 `mcf_start_track`，通过 `node_from_track_coord()` 定位；否则通过 `node_from_bump_track()` 从 start_bump 的 TOB 位置定位
- **snk 节点**：PNnet 连到 `V_P`/`V_N`；Tnet 有 `mcf_end_track` 时直接定位，否则通过 end_bump 定位；Bnet 通过 end_bump 定位
- **类别（McfClass）**：PNnet Pose → `P`，PNnet Nege → `N`，其余 → `Plain`
- **bus 标识**：同一 `origin_key` 下有多条非 PNnet 记录的 commodity 标记为 `is_bus=true`，共享 `bus_key`
- **reach_steps**：从 `record.reach_by_end_start` 提取该 commodity 对应的 Wilton 转弯约束
- **bbox_cobs**：src 和 snk 的 COB 坐标构成的矩形范围内的 COB 列表

### 5.3 两阶段求解（`solve_stage()`）

`run_mcf_global_routing_cob_units()` 在同一个 `GlobalGraph` 上先后调用两次 `solve_stage()`：

1. **BusMCF 阶段**：求解所有 `is_bus=true` 的 commodity，`enforce_bus_equal_length=true`（同一 bus_key 内的 commodity 物理边数相等）。边/节点容量均为默认值 1
2. **SimpleMCF 阶段**：求解所有 `is_bus=false` 的 commodity。边/节点容量取 BusMCF 的残余（`max(0, 1 - used)`）

`solve_stage()` 内部构建 ILP 模型：

- **决策变量**：`x[k][a]`（commodity k 在 arc a 上的整数流量，0/1）和 `o[k][n]`（commodity k 占用物理节点 n 的指示变量）
- **arc 过滤**：`arc_usable_for_class()` 确保每个 commodity 只能使用属于其 `cob_unit` 的 arc，且只有 P-class 可使用 VP 关联弧、N-class 可使用 VN 关联弧
- **流守恒约束**：每个 `(k, node)` 对一个等式行，src 行 = demand，snk 行 = -demand，中间节点 = 0
- **边容量约束**：每对 `(u, v)`（取 min/max 归一化）的所有 commodity 流量之和 ≤ 容量（默认 1）
- **节点容量约束**：每个物理节点被占用次数 ≤ 容量（默认 1）；通过 `o` 变量和 link 行实现
- **reach 步约束**：对有 `reach_steps` 的 commodity，要求指定的 Wilton 转弯边恰好被使用 1 次
- **bus 等长约束**（仅 BusMCF）：同一 bus_key 内所有 commodity 的非虚拟 arc 使用数相等

求解后通过 `extract_path()` 从整数流解中 BFS 提取每个 commodity 的节点路径。

### 5.4 路径输出

求解结果存入 `CobMcfFullResult.paths_by_unit[16]`，每个 `McfPathInfo` 包含：
- `unit_paths`：节点 id 序列（可用 `node_text()` 格式化为 `"U{unit} COB(r,c) T{track}"` 或 `"V_P"` / `"V_N"`）
- `track_paths`：从节点路径提取的去重 track index 序列
- `record_id`、`start_track`、`end_track`、`cob_unit`：关联信息

### 5.5 日志输出（便于诊断）

- **ILP 路由细节**：`main.cc` 中通过 `result.route_details` 输出每条 net 的完整分配信息（bump 坐标、j/k 线、s、orient、track、COBUnit），格式示例：`net "...": bump(T0,B0,G1,I1) -> j=1 (horizontal line), k=4 (vertical line), s=12, orient=straight(QS), track=12, COBUnit=4`
- **ILP W 变量明细**：输出所有 active W 及其对应 bump、j、k、orient、track
- **ILP S 变量明细**：输出所有 active S 及其对应 TOB、v、j、k
- **MCF 按 unit 汇总**：每个 unit 的 bus/simple commodity 数量及求解状态
- **每个 commodity 的路径明细**：按 `commodity -> path#i` 打印完整节点链（`U{unit} COB(r,c) T{track}` / `V_P` / `V_N`）

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

- `tob_ilp_model.cc` 的 `build_tob_ilp_model()`（约束与变量定义）
- `ilp_reach_precompute.cc` 的 `fill_*_case()` 系列（可达性预计算逻辑）
- 必要时同步 `main.cc` 的 record/cost 生成逻辑

### 7.2 想改 MCF 图拓扑或路径规则

优先修改：

- `cob_mcf_router.cc`（`build_track_graph()` 图拓扑 / `prepare_commodities()` commodity 构建 / `arc_usable_for_class()` 类别过滤 / `solve_stage()` 模型约束）
- `mcf_hw_map.hh`（track / COB / TOB 映射规则）

### 7.3 想改 net 回并策略

优先修改：

- `main.cc` 的 `build_records()`（保证 `origin_key` 正确）
- `cob_mcf_router.cc` 的 `prepare_commodities()`（bus 分组与 commodity 构建）

---

## 8. 关键不变量（修改前后都要守住）

1. **ILP 回归不破坏**  
   不加 `--enable-mcf-routing` 时，流程与结果应可独立成功。

2. **record 与 assignment 数量一致**  
   MCF 假设 `records.size() == result.assignments.size()` 且 `records.size() == result.record_track_endpoints.size()`。

3. **类型内语义一致**  
   同一回并组不应混合 `Bnet/Tnet/PNnet`，否则应显式报错。

4. **track 端点映射可复现**  
   `track_to_cob()` 规则必须稳定、可解释，不能引入随机性。

5. **日志可诊断**  
   每个 unit 的 bus/simple commodity 数、可行性、目标值、耗时应有日志。

6. **record_id 全局唯一**  
   `record_id` 由 `build_records()` 按输出顺序分配，ILP/MCF/maze 各阶段通过 `record_id` 对齐数据。

---

## 9. 最小验证清单（每次改动后）

1) `xmake build test_ILP` 成功  
2) ILP-only：

```bash
./output/test_ILP <config_path>
```

3) ILP + MCF：

```bash
./output/test_ILP <config_path> --enable-mcf-routing
```

可选：显式传入与 Interposer 一致的 COB 行列（行为应与省略该参数相同）：

```bash
./output/test_ILP <config_path> --cob-rows <H> --cob-cols <W> --enable-mcf-routing
```

（将 `<H>`/`<W>` 替换为当前 `Interposer::COB_ARRAY_HEIGHT` / `COB_ARRAY_WIDTH` 的数值。）

注意：`--enable-maze` 当前已禁用，传入该参数仅打印提示信息。`--enable-mcf-parallel` 可传入但并行未实际生效。

4) 若改了模型结构，建议附带：

- MPS/LP 导出样例
- 至少一个 case 的前后对比日志

---

## 10. 术语约定（本目录）

- **record**：`Net_cost_record`，2-pin 粒度建模单元
- **record_id**：record 在 `build_records` 输出中的全局唯一序号
- **bit_id**：同一 `origin_key` 内的位序号
- **origin_key**：原始 net 标识，用于回并
- **assignment**：ILP 输出的 record -> cobunit 结果
- **record_track_endpoint**：ILP 输出的 record -> `(cob_unit, start_track, end_track)` 元组
- **commodity**：MCF 中单一供需流对象（`PreparedCommodity`）
- **cobunit**：16 个布线资源分区之一（由 `map_track()` 规则定义）
- **track graph**：track 级全局路由图（`GlobalGraph`），节点粒度为 `(unit, cob, track)`
- **mesh 边**：相邻 COB 之间同 track 的直通连接
- **Wilton 转弯边**：同一 COB 内部不同方向 track 之间的转弯连接
- **BusMCF**：第一阶段求解，处理 bus commodity（同 origin_key 下多条非 PNnet 记录），带等长约束
- **SimpleMCF**：第二阶段求解，处理非 bus commodity，使用 BusMCF 的残余容量
- **reach_steps**：Wilton 转弯步序列（`IlpReachStep`），描述 end_track 到 start_track 的转弯路径

术语尽量统一，不要在同一文档或代码注释里混用"子网/边/commodity/net"而不加限定。
