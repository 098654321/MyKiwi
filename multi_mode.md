## 调整需求

原来的增量布线算法是在一个布线模式下有两个工作模式，配置文件描述了两个模式的布线需求，其中一个模式已经完成布线，然后对另一个模式进行布线。我希望让这一部分的功能不动，在这个基础上加一个功能，也就是直接加载配置文件后直接对两个模式布线，使布线结果能够减少在两个模式之间切换带来的寄存器修改的量，同时尽可能减少布线长度。



## 根据输入信息构造net

对所有net分类，分为1、2两个模式共有的net，和每个模式单独的net（分别记为集合Net_1, Net_2）。每一个模式下的net都有同步总线、非同步的普通线、连接到上拉/下拉端口的线。普通线和上拉/下拉线有可能是多扇出的，也有可能是一对一连接的，而同步总线总是包含多根一对一连接的单bit线。



## 算法流程

1. 使用非增量布线的方法，按照net的类型，连接所有共有的net，然后存储这些net的布线结果（后续2～6步迭代当中不允许拆除这些共有的net），并且存储的时候，两个模式的这些net都需要存入这个布线结果。然后根据布线结果更新寄存器的代价信息

2. 以下3～6步对非共用的net进行布线。在布线过程中如果遇到布线失败的线可以先收集起来

3. 对于Net_1, Net_2当中的net，如果是一条多扇出的普通net，就先放着不连；如果不是多扇出的，就用下面说的方法布线：找到每一条net的bounding box，这个box描述了这一条net在连接的时候大概率会走的路径的范围。如果net是一条普通线，就以这条线的起点、终点为界找bounding box；如果net是一条同步总线，可以把其中所有的单bit线看作一个整体然后找整体的bounding box。bounding box的边界选择具体分为六种情况：

   - 第一种：两个芯粒之间相互连接（两个端点都是bump），两个TOB放置在对角线上。bounding box的左右边界坐标与左右两个TOB的列坐标对齐，上边界比上方TOB的行坐标小1，下边界与下方TOB的行坐标一样

   - 第二种情况：两个芯粒之间相互连接（两个端点都是bump），两个TOB放置在同一条水平线上。bounding box的左右边界坐标与左右两个TOB的列坐标对齐，上边界与两个TOB的行坐标对齐，下边界是两个TOB行坐标减1

   - 第三种：一个芯粒连接到一个左侧IO端口(一个端点是track，一个端点是bump)。bounding box的左右边界分别是IO端口的列坐标和芯粒的列坐标。如果芯粒在IO端口下方，那么上边界是IO端口的行坐标，下边界是芯粒的行坐标。如果芯粒在IO端口的上方，那么上边界是芯粒的行坐标减1，下边界是IO端口的行坐标。

   - 第四种：一个芯粒连接到一个右侧IO端口(一个端点是track，一个端点是bump)。bounding box的左右边界分别是芯粒的列坐标和IO端口的列坐标。如果芯粒在IO端口下方，那么上边界是IO端口的行坐标，下边界是芯粒的行坐标。如果芯粒在IO端口的上方，那么上边界是芯粒的行坐标减1，下边界是IO端口的行坐标。

   - 第五种：一个芯粒连接到上方的IO端口(一个端点是track，一个端点是bump)。bounding box的上边界是IO端口的行坐标，下边界是芯粒的行坐标。如果芯粒在IO端口的左边，左右边界分别是芯粒的列坐标和IO端口的列坐标。如果芯粒在IO端口的右边，左右边界分别是IO端口的列坐标和芯粒的列坐标。

   - 第六种：一个芯粒连接到下方的IO端口(一个端点是track，一个端点是bump)。bounding box的上边界是芯粒的行坐标减1，下边界是IO端口的行坐标。如果芯粒在IO端口的左边，左右边界分别是芯粒的列坐标和IO端口的列坐标。如果芯粒在IO端口的右边，左右边界分别是IO端口的列坐标和芯粒的列坐标。

4. 然后将Net_1和Net_2的bounding box两两配对（匈牙利算法，实现全局的最大权匹配），配对产生的权重按照重合度计算。这个重合度可以用两个bounding box重合的cob数量衡量（后期还可以加入拥塞的估计，但目前不考虑）。重合度越高，说明共享的区域越多。如果Net_1有a条net，Net_2有b条net，当a和b相同的时候可以直接配对，当a和b不同的时候，对于少的那一边，先构造一些虚拟bounding box，这些bounidng box与其它bounding box的重合度总是为0，用于辅助匹配。完成匹配之后，忽略这些虚拟的bounding box。对于那些匹配到虚拟bounding box的真实box，就视为无法匹配，单独存在。

5. 配对的每一对net都进行如下布线：

   - 先找这对net当中两条线的bounding box的重叠区域，在区域内找一对入口cob、出口cob。这两个cob的作用是辅助这一对net当中两条线的布线过程。布线可以拆解成从起点连到入口cob、从入口cob连到出口cob、从出口cob连到终点这三个过程，这样一条线的连接可以分为三段。选择的方法见下方实现细节
   - 开k个线程，并行的尝试这k个组合
   - 每一个进程利用入口cob、出口cob分别对两条net进行三段式布线，把入口cob、出口cob设为从起点到终点的路径上必须经过的点。注意两条net分别属于不同的布线模式，因此二者各自的布线资源不相互冲突。这两条线的布线过程采用迭代的方式：先给其中一条net完成三段布线，布线结束后更新代价信息(update_current_cost)，这个代价信息与另一条net共享。然后布另一条线，利用新的代价完成布线，布线结束后也update_current_cost，然后两条线都update_history_cost。每一轮迭代都重复上述过程，直到前后两轮迭代中两条线的路径走过的总代价之和的差值小于一个阈值，就视为收敛。收敛之后保存两条线的布线结果，然后清空迭代过程中产生的代价信息
   - 某一条线布线的过程中，如果是同步线，并且发生了长度不一样、需要reroute的情况，可以选择从起点到入口cob和从出口cob到终点这两段当中最长的一段，然后运行之前的bus_reroute()，拆出末尾的一段并重连。

6. 最后对剩下的b-a条无法配对的线以及第3步没有连接的多扇出线进行布线。使用之前的增量布线算法完成布线即可。

7. 最终展示布线结果与相关统计数据

   


## 输出

需要根据两个布线模式各自的布线结果，分别输出两个配置文件。



## 修改细节与实现方法

基于上方的算法思路，下方给出实现方法，以及如何根据已有的代码进行修改

### 1) 需要把多模式布线的功能单独抽一个模块

新增模块 ：

- 在kiwi.cc当中加一个multi_mode变量，然后新加入一种输入格式，仅当识别到“--multi-mode”，或者“-m”的时候，将multi_mode变量设为true，表示进入多模式布线的分支。
- 在cli.cc当中加入一个route_multi_mode函数用于实现多模式布线的流程。这个函数的流程框架如下：
  - 直接调用现有的merge_same_mode_nets，合并两个布线模式共有的net。Net自带\_modes成员变量，利用\_modes可以识别出一个Net是共享的还是属于某一个模式的
  - 调用route_multi_mode函数，完成布线算法
  - 调用output功能的函数，write每个模式的controlbits

### 2) algo/router 目录：框架与数据结构调整清单

#### 2.1 新增一个“多模式路由”入口层（不碰旧 command_mode）

- 在algo/router下新开一个目录multi_mode，负责新的模式下的布线过程，需要包含的内容如下：
  - 构建一个总的调度函数route_multi_mode
  - 对所有net分类存储，得到模式之间共享的net，以及每个模式内部各自的net。分别存储，用于后续分开提取并布线。
  - 构建hardware interposer的OccupancyView和Recorders
  - 对共享的net布线
  - 计算bounding box
  - 匈牙利匹配
  - 对每对 net 做“重叠区入口/出口 COB 选择 + 三段式强制过点布线 + k 候选并行尝试”
  - 展示布线结果与相关统计数据


#### 2.2 构建hardware interposer的OccupancyView

现有代码的实现方法会把占用写进硬件对象状态：

- IncreRouting::route_path() 会对路径里每条边 cobconnector.suspend() ，对每个TOB mux give_out()（ routing.cc ）

- maze 扩展只走 interposer->adjacent_idle_tracks(track) （ routing.cc ）


这导致： 1 模式占用会让 2 模式搜索“看起来不可用” ，与新算法允许跨模式复用相冲突。所以这里需要给硬件对象套一层占用视图，区分开两个模式下的占用情况。需要在algo/router/multi_mode目录下建立一个类叫OccupancyView

- 分模式判断布线资源（TOB里面的mux、COB的switch）是否被占用。同一 mode下严格不允许冲突，不同 mode下允许共用。原来的COBSwState(在文件cobregister.hh里)和TOBMuxRegState(在tobregister.hh里)分别指定了底层硬件的状态，而现在不能直接使用原来的记录，而是需要分别记录每个模式下整个interposer当中所有tobmux、cobswitch的状态，，所以OccupancyView需要定义新的记录方式，能够分别记录每个模式当中原来的COBSwState和TOBMuxRegState能够记录的信息。原来的具体记录方式你可以学习hardware/cob和hardware/tob目录下的文件。
- maze路由搜索需要能根据布线资源的模式占用情况，可以得到当前模式下没有被占用的所有邻居而不受其它模式影响。原来方法当中的maze路由搜索你可以学习algo/router/single_mode/incremental/maze/routing.cc当中maze_search()，interposer.cc当中的adjacent_idle_tracks()，tob.hh当中的available_connectors_bump/track_to_track/bump()。
- 对于非共享的net布线在搜索并构造硬件的时候，不能和原来一样直接搜索connectors，而是要先记录下搜索到的信息（例如用pathpackage.hh的COBConnectorInfor & TOBConectorInfo），等最后布线结果确定了提交到pathpackage到时候才能构造connectors

recorders可以沿用原来的，并且两个模式共用一套全局的recorders

#### 2.3 对共享的net布线

提取记录好的共享的net，使用非incremental的普通布线方法进行布线，你可以学习algo/router/single_mode/common/maze.routing.cc。由于共享的net在两个场景里面使用的布线资源一样，所以不用区分，只需要在原有的布线方法当中增加一个步骤，把布线结果记录到两个模式的OccupancyView当中，并存储到两个net的pathpackage里面。这里可以直接遵照原来的流程，给路径使用的相关hardware资源设置底层硬件的状态，也就是COBSwState.suspend()和TOBMuxRegState.give_out()定义的状态。最后更新这些路径使用的布线资源的cost，调用全局recorders的update_recorders_current()/update_recorders_history()（共用的标记）。具体的更新过程你可以学习hardware_recorder.cc文件当中的这两个函数

#### 2.4 计算bounding box

- 在circuit/path当中构建region结构体，包含row_min,row_max,col_min,col_max信息 （统一“闭区间”定义）。

- 在circuit/path当中构建bounding box类，包含：

  - 一个region对象，表示自己的范围

  - 一个mode表示所属模式
  - 一个成员变量记录自己是那个net的bounding box
  - 一个预留的成员变量，用于记录之后匹配到的box
  - 一个预留的region对象，用于记录与匹配到的box之间重叠的区域

- 在所有net 类型中添加bounding box的成员变量。然后给net添加一个纯虚函数，每个子类型net计算自己的bounding box

- 计算所有同步总线的bounding box，以及所有非同步线当中一对一连接的线的bounding box。多扇出的线不用计算bounding box

#### 2.5 匈牙利匹配

- 计算模式1、2当中各自非共享net的数量，记为num_1, num_2，取min(num_1, num_2)作为配对的数量。
- 如果某一个模式非共享net的数量比配对的数量小，就补充一些虚拟bounding box用于满足匈牙利算法的条件
- 考虑模式1和模式2的bounding box两两配对，计算所有配对可能重叠区域大小，重叠区域大小使用重叠部分的cob数量计算。与虚拟bounding box配对时，重叠区域大小记为0
- 使用匈牙利算法配对，保存配对结果，需要记录清楚每一个bounding box的配对目标，以及与该目标的重叠区域范围。那些匹配到虚拟bounding box到真实box就是无法配对的。
- 最后把配对的每一对bounding box对应的每一对net记录起来，用于后一步提取

#### 2.6 对每对 net 做“重叠区入口/出口 COB 选择 + 三段式强制过点布线 + k 候选并行尝试”

这一步总体先对上述配对了bounding box的net布线。最后再对剩下无法配对的线，以及多扇出的线布线。

1. 处理配对了bounding box的线，提取所有配对的对子：

   - 给对子排序：原来每一条net都有一个priority值，值越小优先级越高。现在把这个对子里面的两个net的priority加起来，作为这个对子的priority，同样是值越小，优先级越高。
   - 遍历排序后的每一个对子：
     - 得到模式1、模式2的两条net信息（net_1, net_2），以及bounding box重叠区域的范围
     - 选择k对（入口cob，出口cob），选择的时候：
       - 第一步找到重叠区域内所有可能的(入口cob，出口cob)组合，对于每一对可能的组合使用曼哈顿距离计算两个cob之间的距离、入口cob分别到net_1和net_2起点的距离、出口cob分别到net_1和net_2终点的距离，再把这五段距离求和得到这对组合下net_1, net_2布线的整体预估距离distance_cob
       - 第二步用曼哈顿距离计算两条net各自的起点到终点之间的距离，然后求和，得到net本身的预估布线距离distance_net。这是能够达到的最短布线距离
       - 第三部筛选出所有distance_cob == distance_net的（入口cob，出口cob）组合，并按照第一步计算好的两个cob之间的距离降序排列
       - 第四步取前k个组合，作为需要尝试的（入口cob，出口cob组合）
     - 开k个线程，对k个组合并行的尝试布线，每个线程进行一个循环（框架模仿原来incremental_route.cc当中的iterate_routing，但是不完全一样）直到收敛：
       - 将这一对net当前的布线记录保留为历史记录，清空当前的记录
         - 选择两条net当中priority更高的一条net线布线，完成三段式的连接，连接的结果存在circuit/path里面的新数据结构当中,然后update_recorders_current（非共用的标记）
       - 利用更新好的cost信息，对另一条net进行布线，完成三段式的连接，连接的结果存在circuit/path里面的新数据结构当中，然后update_recorders_current（非共用的标记）
       - 给两条线update_recorders_history（非共用的标记）
       - 展示信息，信息内容与原来一致
       - 计算这两条net走过的路径占据的寄存器的cost总和，得到与上一轮迭代的总和的差值，如果差值小于给定的阈值thereshold，就视为收敛
     - 多线程并行布线的过程有以下注意点：
       - 每个线程自己布线的时候，如果布线对象是同步线，并且出现了长度不一致的问题，那么选择三段路径当中最长的那一段，对这段调用之前的bus_reroute()。你可以学习之前的sync_incremental_reroute（）及其中bus_reroute（）的流程。
       - 在开始并行布线的时候，每个线程都必须复制一份OccupancyView和Recorders，作为本线程独立使用的本地变量。布线过程中任何需要用到OccupancyView和Recorders的地方都只能用本地的变量，也不允许改写任何全局的变量（例如不能调用COBConnector::suspend() TOBConnector::give_out()修改全局硬件状态、不能把临时路径塞进了 Net::pathpackage()）
       - 只要有一个线程的布线结果先收敛，就终止其他线程，直接丢弃其它线程内部的OccupancyView/Recorders。将胜出线程的两条线的布线结果记录进外部OccupancyView当中，并更新外部的Recorders的update_recorders_current/history()。其中（入口cob，出口cob）之间的路径使用共用标记更新，其余路径使用非共用标记更新。最后合并两条线各自的三段路径，并提交到各自的path_package当中，设置底层hardware当中cobswstate&tobmuxregstate的相关状态
       - 如果有多个线程严格的同时收敛，采用“竞态选举+协作式终止+单点提交”的模型决定唯一的winner

2. 剩下无法配对的线，以及多扇出的线布线

   这里可以用全局的Recorders和每个模式的OccupancyView，完全遵循iterate_routing()的框架，对每个模式下剩余的线分别布线。

#### 2.7 “必须经过入口/出口 COB” 的三段式路由：需要新的路径约束接口

现有路由接口是：给 begin_tracks / end_tracks / occupied_tracks 做一次 maze（BFS/heap），并没有“必经点”概念。三段式可以通过组合三次搜索实现，但需要：

- 更新maze函数，支持以某个cob作为终点。支持的方法是，在搜索过程中，如果遇到一条track是这个cob可用的track，那么就视为搜索成功。这里可用的track指的是，首先track的行列坐标与cob相同，或者行坐标比cob行坐标多1且列坐标一样，或者列坐标比cob列坐标多1且行坐标一样。然后还需要用Interposer的is_idle_tracks判断是否是可用的track。除了新增这个支持方法以外，其它和之前的maze一样。
- 在circuit/path当中添加一个数据结构，需要能存下三段路径的信息、确保首尾连接准确，能够便于提交到path_package当中构造最终的路径，注意PathPackage::_regular_path 里每个元素是 (track, connector_before_track) 。我目前的想法是在这个数据结构当中加入三个结构体，第一个结构体表示从起点到入口cob到路径，包含一个tob_to_track和一段regular_path。第二个结构体表示从入口cob到出口cob，包含一个regular_path，起点与第一个结构体的regular_path当中最后一条track重合。第三个结构体表示从出口cob到终点的路径，包含一个regular_path和一个track_to_tob，regular_path的起点与第一个结构体的regular_path当中最后一条track重合。这里的regular_path, tob_to_track, track_to_tob都参考HistoryPathPackage当中的结构进行构造。最后这个数据结构里面还需要一个变量存储整条路径长度，长度计算方法与之前一样，track构成的路径可以用path_length()函数计算，一个bump算长度是1，注意后两段路径开头重合了一个track，在计算长度的时候不要重复算
- 这个新的数据结构需要有拼接三段路径并将拼接结构提交到pathpackage的功能。拼接的规则可以参考：
  - 拼接两段时，首先需要校验后一段首个track是否和前一段最后一个track相同，然后去掉后一段首个重复 track（通常其 connector 是 nullopt ）
  - 保留每段内部 connector 顺序，不重算
  - _tob_to_track 只来自第一个结构体， _track_to_tob 只来自最后一个结构体

### 3) output功能

#### 3.1 output流程（参考module.cc的output_from_routing_results()）

- 先获取模式1的所有net，调用connect_registers()，然后调用write_control_bits
- 清除所有的registers
- 再获取模式2的所有net，调用connect_registers()，然后调用write_control_bits

#### 3.2 connect_registers

原来的PathPackage::connect_all()过程中还需要加入两步，根据路径的前后连接关系设置track的prev_track, next_track和connected_bump指针，以及设置bump的connected_track指针，这样可以确保这些信息是当前布线结果connect之后得到的信息。

#### 3.3 write_control_bits的文件名

输出controlbits文件的时候，模式1、2分别对应同一个输出目录下的文件名controlbits_1.txt, controlbits_2.txt

#### 3.4 清除registers的方法

 interposer->reset_regs() 只重置寄存器，不一定清理 Track/Bump 的 connected 指针。在第二个模式输出前，Track/Bump 可能还挂着第一个模式的 connected_track，影响后续逻辑/调试/校验（哪怕 writer 只读寄存器，这也是“状态泄漏”风险）。所以除了使用interposer->reset_regs()，还需要在Interposer 上提供 reset_connectivity()，清空track的prev_track, next_track和connected_bump，清空bump的connected_track

### 4) 一些额外的注意点

#### 4.1 parse/reader：关于双模式布线的读取文件注意点

需要注意，虽然现在是双模式布线，但是仍然只需要读入一个folder的配置文件，因为配置文件的connections.json当中就描述了两个模式的布线需求。

#### 4.2 指针/敏感数据结构风险点（需要特别列入清单）

RouteEngine 内部保存的是 Net* （从 Rc<Net> 的 .get() 来），只要 BaseDie::_nets 里的 Rc 生命周期在就安全；但要避免在 engine 构造后再做 merge_same_mode_nets() 这种会替换 Rc 的操作。现在顺序是 merge 后才 route（ cli.cc ），新模块也要保持这个不变量。

#### 4.3 我希望上面提到的并行参数k、非共有net的迭代收敛阈值这两个参数可以在一个比较清楚的地方定义并修改

