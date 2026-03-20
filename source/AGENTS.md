# Kiwi /source 工程指南（面向 AI Agent）

本文件是 Kiwi 工程的说明书，AI 阅读本文件后可以理解项目的核心对象模型、算法流程、关键不变量以及常见修改点，从而在理解代码文件之前可以对核心内容有大概的认识。

Kiwi 是面向 chiplet interposer 的布局布线工具：输入一份系统配置（topdie/topdieinst/external ports/connections），在 interposer 的资源模型上完成放置与布线，并输出硬件可用的 controlbits（寄存器配置比特）。

---

## 1. 快速上手（构建 / 运行 / 测试）

构建系统使用 xmake，主要入口在仓库根目录的 `xmake.lua`。

常用目标：

- `kiwi`：主程序（支持 CLI 与 GUI）
- `view2d` / `view3d`：加载配置并执行 P&R，然后用 2D/3D 视图展示结果
- `module_test` / `regression_test`：测试目标（用例与说明见仓库根目录 README）

典型命令（在仓库根目录）：

```bash
xmake build kiwi
xmake run kiwi <config_folder> [OPTIONS]

xmake build regression_test
xmake run regression_test
```

语言标准：`xmake.lua` 将 C++ 语言设置为 `c++23`（Linux 平台额外设置 `-std=c++2b`）。

---

## 2. 总体数据流（从配置到 controlbits）

CLI 模式的端到端流程在 `source/app/cli/cli.cc`：

1) `parse::read_config(config_folder, mode)`
   - 解析输入配置到 `parse::Config`  
   - 构建 `hardware::Interposer` 与 `circuit::BaseDie`

2) `algo::build_nets(basedie, interposer)`（`source/algo/netbuilder/`）  
   - 将 `BaseDie` 中的 `Connection`（按 mode/sync 分组）转为 `circuit::Net`/`SyncNet`

3) 可选布局（`-p/--placement`）：`algo::place(...)`（`source/algo/placer/`）  
   - 当前默认策略：模拟退火 `SAPlaceStrategy`

4) 布线（`source/app/cli/cli.cc`）  
   - 单模式分支：`route_single_mode(...)` → `algo::route_nets(...)`  
   - 双模式分支（`--multi-mode`）：`basedie->merge_same_mode_nets()` → `basedie->merge_same_nonsync_nets_across_modes()` → `algo::route_multi_mode(...)`

5) 输出 controlbits（`parse::output_from_routing_results`，`source/parse/writer/`）  
   - `PathPackage::connect_all()` 将路径绑定到 TOB/COB 寄存器对象  
   - `parse::Writer` 从硬件对象抓取寄存器值并写入 `controlbits_<mode>.txt`

---

## 3. 核心概念与术语（必须先理解）

### 3.1 物理模型：hardware/

核心对象是 `hardware::Interposer`（`source/hardware/interposer.hh`）：

- interposer 由 TOB/COB/Track/Bump 构成
- `Track`（`source/hardware/track/track.hh`）是布线图的节点（路由在 track 图上搜索）
- `COBConnector`（`source/hardware/cob/cobconnector.hh`）描述 Track ↔ Track 的可编程连接（对应 COB 的开关/选择寄存器）
- `TOBConnector`（`source/hardware/tob/tobconnector.hh`）描述 Bump ↔ Track 的可编程连接（bump → hori → vert → track 的 mux 链 + 方向寄存器）

邻接关系：

- `Interposer::adjacent_idle_tracks(track)`：返回可走的邻接 Track 及其需要使用的 `COBConnector`
- Maze/增量路由都以此作为图遍历的“边”

坐标体系（常见混淆点）：

- `hardware::Coord(row,col)`：基础 2D 坐标（`source/hardware/coord.hh`）
- `hardware::TOBCoord(row,col)`：TOB 阵列坐标（不是 interposer 的绝对坐标）
- `hardware::TrackCoord(row,col,dir,index)`：Track 坐标（`source/hardware/track/trackcoord.hh`）

### 3.2 逻辑模型：circuit/

`circuit::BaseDie`（`source/circuit/basedie.hh`）是逻辑设计容器：

- `topdies`：芯粒类型（pin_map：pin 名 → bump index）
- `topdie_insts`：芯粒实例（绑定到某个 TOB）
- `external_ports`：外部端口（绑定到某个 TrackCoord）
- `connections`：原始连接（按 `mode -> sync -> vector<Connection>` 组织）
- `nets`：由 `NetBuilder` 构建出的线网集合（按 mode 存储）

Pin/Connection（`source/circuit/connection/`）：

- `Connection` 由 input_pin/output_pin 组成
- `Pin` 可能指向：
  - topdieinst 的某个 bump（通过 pin 名映射到 bump index）
  - external port（对应 interposer 某个 track）
  - 固定电源/地（以 `pose`/`nege` 后缀区分）

Net（`source/circuit/net/net.hh`）是布线抽象接口，核心职责：

- `route(interposer, strategy)`：非增量布线
- `incremental_route(interposer, incre_strategy, engine, shared)`：增量布线
- `check_accessable_cobunit()` / `accessable_cobunit()`：资源可达性分析（用于资源管理与匹配）
- `coords()`：用于布局评估的坐标集合（HPWL）
- `pathpackage()`：保存当前路由结果（见下一节）
- `modes()`：此 net 属于哪些 mode（多 mode => 可复用线网）
- `reuse_type()`：增量路由代价模型中的“复用/非复用”标记

### 3.3 路径表示：PathPackage（输出 controlbits 的基石）

`circuit::PathPackage`（`source/circuit/path/pathpackage.hh`）是路由结果的唯一真源：

- `_regular_path`：`[(Track*, optional<COBConnector>), ...]`，正向顺序保存轨道与 COB 连接器
- `_tob_to_track`：bump 作为源端时的 TOB 连接器（Bump → Track）
- `_track_to_tob`：bump 作为汇端时的 TOB 连接器（Track → Bump）
- `_length`：包含 TOB 端点与 Track 段的长度计数（不同 net 类型会有不同加法）

连接器状态机（非常关键）：

- 路由阶段通常只占用/预留连接器：  
  - MazeRouteStrategy / IncreRouting 会对路径中的 `COBConnector` 调用 `suspend()`  
  - 对端点 `TOBConnector` 调用 `give_out()` 并写入 package
- 输出阶段才真正连通：  
  - `PathPackage::connect_all()` 将所有 connector `connect()`，并在 Track/Bump 上建立连接关系
- 清理/回滚必须同步恢复寄存器状态：  
  - `PathPackage::reset_all()` / `clear_all()` / `occupy_all()` 负责批量状态维护
- 双模式输出防泄漏：  
  - `parse::output_two_modes_from_routing_results(...)` 会按 mode 顺序输出，并在两次输出之间执行 `interposer->reset_regs()` + `interposer->reset_connectivity()`

---

## 4. 输入解析与 controlbits（parse/）

### 4.1 读取配置（Config → Interposer/BaseDie）

入口：`parse::read_config`（`source/parse/reader/module.cc`）

- `load_config`：`source/parse/reader/config/config.cc`（serde/json 驱动）
- `Reader::build()`：`source/parse/reader/reader.cc`

Pin 名的解析规则（`Reader::parse_connection_pin`）：

- 以 `pose` 结尾：VDD 固定端口
- 以 `nege` 结尾：GND 固定端口
- 不包含 `.`：external port（在 `basedie->external_ports` 中查找）
- `topdieinst.pin` 形式：芯粒实例 pin（先找 topdieinst，再用 topdie 的 pin_map 映射到 bump index）

`ports_01`（`Reader::add_01ports_to_basedie`）：

- 配置中必须有 `pose`/`nege` 两组 TrackCoord
- Reader 会校验这些 TrackCoord 满足 `Interposer::is_external_port_coord`

### 4.2 读取已有 controlbits（用于跳过布线或增量 warm-start）

入口：`parse::read_controlbits`（`source/parse/reader/module.cc`）

- 若存在 `controlbits_<mode>.txt`：
  - `load_controlbits` 解析到 `parse::Controlbits`（`source/parse/reader/controlbits/controlbits.hh`）
  - `bits_to_paths(interposer, basedie, controlbits, mode)` 将 bit 反推回 `PathPackage`
- 增量模式且当前 mode 没有 bit：会按 net 规模从大到小尝试加载其它 mode 的 controlbits，并把这些路径作为旧路径

`read_controlbits` 返回值语义（见实现）：

- 第一个 bool：当前 mode 的 controlbits 是否存在并被加载
- 第二个 bool：是否加载到了任意旧路径（可能是当前 mode，也可能是其它 mode）

### 4.3 写出 controlbits（从 PathPackage 到寄存器文件）

入口：`parse::output_from_routing_results`（`source/parse/writer/module.cc`）

- `connect_registers(interposer, basedie, mode)`：
  - 遍历 `basedie->nets_to_vector()`
  - 对属于该 mode 的 net 调用 `net->pathpackage().connect_all()`
- `write_control_bits(interposer, output_path, mode)`：
  - `parse::Writer::fetch_and_write(...)`（`source/parse/writer/writer.hh`）
  - Writer 会从 interposer 的寄存器对象抓取 bit，并写成文本格式

---

## 5. Net 构建（algo/netbuilder）

入口：`algo::build_nets`（`source/algo/netbuilder/netbuilder.cc`）

输入：`basedie->connections()`（按 mode、sync 分组）  
输出：`basedie->nets()[mode]`（`std::Rc<Net>` 集合），并把 net 指针挂到相关 `TopDieInstance::_nets` 上

### 5.1 非同步连接（sync == -1）

按驱动端节点聚合为一个 net：

- Track → Bump：`TrackToBumpNet` / `TrackToBumpsNet`
- Bump → Bump：`BumpToBumpNet` / `BumpToBumpsNet`
- Bump → Track：`BumpToTrackNet` / `BumpToTracksNet`
- Track → Track：非法，直接报错

### 5.2 同步连接（sync != -1）

同一个 sync group 内的多条连接会被包装为一个 `SyncNet`（`source/circuit/net/types/syncnet.hh`）：

- 内部持有三个列表：BTB/BTT/TTB 三类二端网
- `SyncNet::route` 会先对三类网预布线，再通过 reroute 拉齐长度（保证同步）

### 5.3 固定电源/地（VDD/GND）

`build_fixed_nets` 会将所有被标记为 `pose/nege` 的 bump 收集起来，构造：

- `TracksToBumpsNet(ports_01.pose → bumps_with_pose)`
- `TracksToBumpsNet(ports_01.nege → bumps_with_nege)`

---

## 6. 放置（algo/placer）

入口：`algo::place`（`source/algo/placer/place.hh`）

当前策略：`SAPlaceStrategy`（`source/algo/placer/sa/saplacestrategy.cc`）

算法要点：

- 状态：每个 `TopDieInstance` 绑定一个 `hardware::TOB*`
- move：随机选一个 topdieinst 移动到随机 idle TOB
- swap：随机选两个 topdieinst 交换 TOB
- 代价：
  - 线长：对每条 net 计算 HPWL（`Net::coords()` → bbox）
  - 热/功耗：对高功耗芯粒施加距离惩罚（实现目前主要用 thermal_cost）
- 约束：
  - TOB 不能被多个 chip 占用
  - `SAPlaceStrategy::is_changable` 禁止把某个 inst 放到其线网端口已包含的 TOB 上

---

## 7. 布线（algo/router）

### 7.1 命令链框架（command_mode）

`route_nets`（`source/algo/router/route_nets.cc`）内部使用 `Invoker` 执行一串命令（`source/algo/router/command_mode/invoker.cc`）。

非增量（当前默认）：

1) `Sort`：按 `Net::port_number()` 归一化并提升 priority，然后按 priority 降序排序  
2) `Resources`：对每条 net 调用 `check_accessable_cobunit()`，并由 interposer 汇总管理 cobunit 资源  
3) `Route`：逐条调用 `Net::route(...)`（通常是 MazeRouteStrategy）

增量：

1) `Set_reuse_type`：`net->modes().size() > 1` 视为 reuse  
2) `Sort`：命令链中存在该步骤，但当前增量排序主要在 `Incre_route::sort_incre` 内部完成  
3) `Resources`  
4) `Init_recorder`：若加载了旧路径则初始化 cost/占用状态  
5) `Incre_route`：迭代增量布线

`RouteEngine`（`source/algo/router/routeengine.hh`）是命令链的上下文：

- 管理 nets 的视图（按 mode 过滤、按是否存在旧路径过滤）
- 跟踪当前 routing position（用于失败恢复/回退）
- 持有 `HardwareRecorder`、`RouteData` 等统计对象

### 7.2 非增量 MazeRouteStrategy（common/maze）

`MazeRouteStrategy`（`source/algo/router/common/maze/mazeroutestrategy.cc`）的 `maze_search` 是 BFS（队列 + prev map），并非 A*：

- 输入：begin_tracks（候选起点轨道集合）、end_tracks（目标轨道集合）、occupied_tracks（不可用轨道集合）
- 扩展：使用 `Interposer::adjacent_idle_tracks`
- 输出：`[(Track*, optional<COBConnector>), ...]`
- 对路径中的 `COBConnector` 调用 `suspend()` 以预留资源
- 对端点 TOB 的 `TOBConnector` 调用 `give_out()` 并写入 `PathPackage`

同步网（`SyncNet`）：

- 先分别对三类子网预布线，得到当前 max_length
- 再调用 `MazeRerouter::bus_reroute` 对短路径做 reroute 拉齐长度

`MazeRerouter`（`source/algo/router/common/maze/mazererouter.cc`）使用优先队列扩展（实现中标注为 A*），用于“在不短于 max_length 的前提下找更长或等长路径”。

### 7.3 资源分配（common/allocate）

项目实现了基于 Hopcroft–Karp 的 bump→track 最大二分匹配（`source/algo/router/common/allocate/hopcroft_karp.cc`），但默认命令链中 Allocate 当前被注释掉。

基本思想：

- 对每个 TOB 独立构造二分图：左侧 bump index，右侧 track index
- 边：由 `Net::accessable_cobunit()` 推导“该 bump 可到达的 track indexes”
- 通过 HK 求完美匹配后：
  - 在 bump 上记录 allocated_track
  - 为对应 net 的 `PathPackage` 写入 TOBConnector（bump ↔ track 的 mux 链）

### 7.4 增量路由（incremental）

增量路由由两部分组成：

1) `Incre_route` 命令：负责多 cycle 迭代、收敛判断、失败回退  
   - 入口：`source/algo/router/command_mode/command/incremental_route.cc`  
   - 每个 cycle：
     - 把当前包保存到 history（`net->set_history_pathpackage()`）
     - 清空当前包（`net->reset_pathpackage()`）
     - 逐 net 调 `Net::incremental_route(...)`
     - `HardwareRecorder` 更新 current/history 记录并重新计算 cost
   - 收敛条件：`fabs(cost_nonreuse - last_cost_nonreuse) < bound`
   - 若某 cycle 失败：返回上一 cycle 的 history 路径作为最终结果

2) `IncreRouting` 策略：负责单条 net 的增量搜索  
   - 入口：`source/algo/router/incremental/maze/routing.cc`  
   - begin/end 搜索点会合并“当前可用资源 + 与该端点共享节点的已布线路径”（`existing_path_vec`）  
   - `maze_search` 使用 min-heap，以 `HardwareRecorder::expand_cost(...)` 作为扩展代价  
   - 注意：实现中只记录首次到达的 prev，不做传统 Dijkstra 的松弛更新；修改代价模型时需验证这一点是否满足预期

`HardwareRecorder` 与代价模型（`source/algo/router/incremental/recorders/`）：

- `TypeRecorder` 记录某个资源单元的 reuse/non-reuse 历史次数，并据此动态更新两类 cost
- `HardwareRecorder` 以 Track/TOB/COB 为粒度管理 recorder，并提供：
  - `track_cost(...)` / `bump_to_track_cost(...)` / `expand_cost(...)`
  - update_current/update_history/clear_history 等迭代维护接口

`bound_bits`（`source/algo/router/incremental/bound_bits/`）：

- 将 Track/COB/TOB 的资源使用按组聚合统计（`BitsGroup<N>`）
- `GlobalBoundBits::get_rate()` 用于输出 “monopolized_by_reuse / has_nonreuse” 等指标

### 7.5 双模式联合路由（multi_mode）

入口与参数：

- 入口：`algo::route_multi_mode(...)`（`source/algo/router/multi_mode/route_multi_mode.cc`）
- CLI：
  - `-m, --multi-mode` 开启双模式联合布线
  - `--mm-k <K>`：每对 net 的入口/出口 COB 候选并行尝试数
  - `--mm-threshold <EPS>`：配对迭代收敛阈值
- 默认参数：`source/algo/router/multi_mode/params.hh`

主流程（按实现顺序）：

1) 分类：shared / mode1-only / mode2-only  
2) shared 先布（非增量 Maze）并写入 mode1+mode2 占用视图，更新共享类型 recorder  
3) 非 shared 计算 bbox（普通二端线 + SyncNet）  
4) 匈牙利最大权匹配（权重为 overlap 区域 COB 数）  
5) 对每个配对 net 选择 `(entryCob, exitCob)` 候选，执行 k 线程并行、线程内迭代收敛  
6) winner 单点提交：写 `PathPackage`、占用硬件状态、更新外部 `OccupancyView` 与全局 recorder  
7) 剩余网（无 bbox / 未配对）收尾布线  
8) 输出 `controlbits_1.txt` + `controlbits_2.txt`

关键新增组件：

- `OccupancyView`（`occupancy_view.hh/.cc`）  
  - 按 mode 隔离 COB/TOB 占用，支持 shared 锁定，避免跨 mode 假冲突
- `Region` / `BoundingBox`（`source/circuit/path/region.hh`、`bounding_box.hh`）  
  - 支持 bbox 匹配与重叠区域记录
- `ThreeSegmentDeferredPath`（`source/circuit/path/deferred_path.hh/.cc`）  
  - 保存三段式强制过点路径，并可转 `HistoryPathPackage`
- `constrained_maze`（`constrained_maze.hh/.cc`）  
  - 支持以 COB 邻接规则作为搜索终点
- `hungarian`（`hungarian.hh/.cc`）  
  - 用于 mode1/mode2 bbox 最大权匹配

不变量与注意事项：

- 并行尝试阶段不得改写全局硬件状态；只允许 winner 在主线程提交  
- shared 资源在后续流程中应保持不可拆  
- 双模式输出必须执行 `reset_connectivity()`，否则 Track/Bump 连通指针会跨 mode 泄漏

---

## 8. 目录与关键文件索引（修改时优先查这些）

### app/

- `source/app/kiwi.cc`：命令行参数解析与模式选择（GUI/CLI/增量/compare/placement）
- `source/app/cli/cli.cc`：端到端主流程（read_config → build_nets → place → route → write）
- `source/app/gui/gui.cc`：GUI 入口（Qt window）

### circuit/

- `source/circuit/basedie.hh`：逻辑设计容器（topdies/topdieinst/external_ports/connections/nets）
- `source/circuit/net/net.hh`：Net 抽象接口（route / incremental_route / resource analysis / pathpackage）
- `source/circuit/net/types/*.hh`：具体 net 类型（BB/BT/TB/多端/Sync）
- `source/circuit/path/pathpackage.hh`：路径与 connector 状态机
- `source/circuit/topdieinst/topdieinst.hh`：placement 的 move/swap

### hardware/

- `source/hardware/interposer.hh`：硬件图与资源查询入口（available_tracks / adjacent_idle_tracks / reset_regs）
- `source/hardware/track/track.hh`：Track 连通关系（prev/next/connected_bump）
- `source/hardware/cob/cobconnector.hh`：COB 连接器（connect/suspend/disconnect）
- `source/hardware/tob/tobconnector.hh`：TOB 连接器（give_out/stay_inside/connect）

### algo/

- `source/algo/netbuilder/netbuilder.cc`：Connection → Net 的分类与构建
- `source/algo/placer/sa/saplacestrategy.cc`：模拟退火放置
- `source/algo/router/route_nets.cc`：布线总入口（Invoker + RouteEngine + analyze_results）
- `source/algo/router/routeengine.*`：net 视图与旧路径存在性策略
- `source/algo/router/common/maze/*`：非增量 BFS maze + reroute
- `source/algo/router/incremental/*`：增量迭代路由与代价模型
- `source/algo/router/multi_mode/*`：双模式联合路由（占用视图、bbox 匹配、三段式约束、配对迭代）

### parse/

- `source/parse/reader/reader.cc`：配置到 BaseDie/Interposer 的落地逻辑（Pin 名解析规则在这里）
- `source/parse/reader/controlbits/controlbits.*`：controlbits 文本格式解析与反推 PathPackage
- `source/parse/writer/module.cc`：PathPackage::connect_all + Writer 输出
- `source/parse/writer/writer.*`：寄存器抓取与文件输出格式

---

## 9. 常见修改指南（避免踩坑）

### 9.1 修改/扩展输入配置格式

需要同时更新：

- `parse/reader/config/*`：Config 结构与 load_config 的解析逻辑
- `serde/de.hh` + 对应类型的 `DESERIALIZE_STRUCT/ENUM` 宏展开
- `Reader::build()`：把新增字段落到 `BaseDie/Interposer`

### 9.2 新增一种 Net 类型

必须同步更新这些层：

1) `circuit/net/types/`：新增 Net 子类并实现 `Net` 虚函数  
2) `algo/router/common/routestrategy.hh`：为新 net 增加虚接口  
3) `MazeRouteStrategy`：实现对应的 route_*  
4) `IncreRouting`：实现对应的 route_*（如果增量模式需要支持）  
5) `algo/netbuilder`：把 Connection 分类到新的 net 类型  
6) `PathPackage`：确认新 net 的端点/长度计数与 connect/reset 逻辑一致

### 9.3 修改路由算法/代价模型

高风险点：

- 连接器状态机必须一致：route 阶段 suspend/give_out，输出阶段 connect_all
- 增量路由依赖 `HardwareRecorder` 的 cost 更新；修改 cost 需验证收敛条件与失败回退逻辑
- `RouteEngine::nets()` 在存在旧路径时会过滤 shared nets；改动筛选规则会影响增量复用策略
- 双模式下新增约束：
  - `OccupancyView` 的 mode 隔离不能破坏同 mode 冲突检测
  - 线程内尝试不得写全局 `Net::pathpackage()` 或硬件寄存器状态
  - winner 提交必须发生在主线程，避免竞态写入

### 9.4 修改 controlbits 输出格式

需要同时改：

- `parse/writer/writer.*`：写出格式
- `parse/reader/controlbits/*`：解析格式 + bits_to_paths 反推逻辑
- 回归测试：确保 `read_controlbits → bits_to_paths → connect_all → write_control_bits` 能闭环

---

## 10. 代码风格与约定

### 10.1 命名约定

- 命名空间：snake_case（`kiwi::hardware`, `kiwi::algo`），所有工程代码都在 `namespace kiwi` 下
- 类/结构体：PascalCase（`RouteEngine`, `TopDieInstance`）
- 函数：snake_case（`check_accessable_cobunit`）
- 成员变量：`_snake_case`（带前导 `_`）

### 10.2 日志与异常

- 统一使用 `kiwi::debug`（`source/global/debug/`）
- 典型用法：`debug::info_fmt("...", ...)`、`debug::fatal(...)`、`THROW_UP_WITH("context")`

### 10.3 标准库封装（强约束）

工程核心代码倾向使用 `source/global/std/` 中的封装类型：

- `std::Vector`, `std::String`, `std::HashMap`, `std::HashSet`, `std::Option`, `std::Rc`, `std::Box` 等
- 头文件中尽量避免直接暴露原生 `std::vector/std::string/...`

### 10.4 序列化（serde/）

- serde 宏位于 `source/serde/ser.hh` 与 `source/serde/de.hh`
- 常用：`DESERIALIZE_STRUCT`, `DESERIALIZE_ENUM`, `FORMAT_STRUCT`, `FORMAT_ENUM`
