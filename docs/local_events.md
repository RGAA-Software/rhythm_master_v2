# 有界本地事件与动作重放

对应总体计划 P2；不恢复通信开发。连续数值仍使用现有标量/信号/宏合同；一次性事件
单独携带时间和身份，不能用“某帧 bool 为真”代替可追溯的触发。

## P2.1 身份与准入

`parameters::Event` 是不含外部类型的值：媒体秒数、来源（节拍/Cue/音频/手动/算子）、
源节点 ID、组件实例作用域、源内单调序号、播放代次、动作类型及有限标量载荷。
根作用域为 0；源节点、序号及代次必须非零。首批类型为脉冲、门控与重置；门控只接受
0/1，重置载荷固定 1，脉冲范围 ±1e6，时间范围沿用现有 0–1e9 秒。

身份由来源（含作用域）+代次+序号确定。同源序号必须递增、时间不能倒退；已执行的
旧序号仍拒绝。因此无须无限保存去重集合。相同时间按作用域、源节点、来源类型、
序号稳定排序，不依赖宿主回调到达顺序。不同作用域内同一个定义节点不是同一来源。

`EventQueue` 由一个求值所有者使用，构造时预留容量；不是多线程消息中间件。
宿主只提交值，调用一次 Drain 后把同一个不可变批次交给本帧各消费者/预览。
暂停不派发；seek/循环/换工程由既有播放代次显式 Reset，清除旧队列，不补发跳过的
现场副作用。无效输入、旧代次、迟到、重复/旧序号、来源数满、队列满分别返回原因，
拒绝不消耗序号，不能默默把未受理操作标为完成。

## 初始预算探针

候选预算为总队列 1024、来源 128、每帧派发 256。超出单帧的到期项保留在队列，
返回 due_remaining；全部按原时间和顺序继续派发，UI/诊断必须显示积压，不能把
延后触发声称为准确到拍。队列满拒绝新事件，不能为了留新事件丢掉旧事件。
这些仅是本地事件合同预算，不增加图渲染 pass、音频线程或解码资源预算。
原生探针以 128 个逆序交错来源、每轮 1024 事件运行 200 轮，记录实际均值；
算子传播/音频接入后的总帧成本还需另测，不用队列微基准代表复杂图性能。

2026-09-09 原生合同与探针均通过：Windows Release 每轮均值 0.447 ms，USB Redmi
K40S/Android 14 每轮均值 0.814 ms。暂采用上述预算用于首批集成；该均值不是单帧
最坏延迟保证。日志 `out/p2-events-windows-tests.log` 和
`out/p2-events-android-executable-tests.log`；首次 adb 推送后缺可执行权限的失败
保留在 `out/p2-events-android-tests.log`，chmod 后实际执行通过。
测试覆盖暂停/恢复、消费后去重、源作用域排序、非法输入不变更、代次重置、来源和
队列上限、超单帧积压及满队列拒绝不消耗序号。

## 复用核对

已只读检查本地 TiXL `fbc994d923e8a0142d2ff1b772e4d12248c5b0ba`：
`Operators/Lib/Symbols/numbers/bool/logic/Trigger.cs`、
`Operators/Lib/Symbols/flow/ResetSubtreeTrigger.cs`、
`Core/Audio/AdsrCalculator.cs`。来源 `https://github.com/tixl3d/tixl`，MIT；既有
许可位于 `third_party/notices/tixl-effects/LICENSE.txt`。

Trigger 按图求值时间去重布尔边沿，但不提供持久序号、代次、组件作用域与队列
准入；ResetSubtreeTrigger 递归修改 Slot/DirtyFlag，依赖其 C# 图对象且无本项目的
单帧传播预算。此增量不复制这些文件，保留项目专属值合同和宿主适配，复用标准库
有序容器操作及现有 BeatGrid/ControlSequence/PlaybackClock 时间约定。后续包络
优先适配独立 AdsrCalculator 的计算部分，适配时单独记录文件及修改；不导入整个
TiXL 操作器系统，不引入新第三方依赖。

## 后续增量

P2.2 接节拍、Cue 越界和音频瞬态，加入包络、计数/步进、门控/锁存、指定状态重置；
无延迟图环继续在编译阶段拒绝，跨帧反馈必须显式。P2.3 一起处理事件端口、预览、
组件实例化、复制/拆分的 ID 重映射以及 schema/ABI。P2.4 才接现场录制和可编辑动作轨；
记录后的媒体时间和事件身份用于离线重放，未录制输入不承诺可复现。
当前队列文件不是 P2.2–P2.4 已完成的证明。

### 首批包络计算

`EventEnvelope` 已适配 TiXL 的线性 ADSR 与 gate/pulse 释放行为，出处及文件哈希见
`provenance/event_envelope.json`。不同于上游帧差累加及 60 Hz 回退，使用精确事件
时间计算阶段；零时长阶段直接越过，脉冲总时长可在 attack 中途释放，gate-off
保留当时幅值，重复 gate-off 不延长 release，reset 清零。求值为只读，不随预览
次数推进时间。脉冲载荷作为输出幅度；错误代次/时间/载荷在修改状态前拒绝。

Windows 与 USB Android `event_envelope_tests` 通过，覆盖完整阶段、持续 gate、
中途释放、重置、零时长、幅度、非法输入和 30/60/144 Hz 采样一致性。日志
`out/p2-envelope-windows-tests.log`、`out/p2-envelope-android-tests.log`。
首次 MSVC 测试文件缺直接 `<string>` 包含已修复（Clang 的传递包含未暴露问题），
失败日志 `out/p2-envelope-windows-build.log` 保留。这仍是计算合同，节点端口接入继续。

### 图类型增量

图注册表新增独立 Event 类型，定义 beat/Cue/audio-onset/edge 来源，以及 merge、
envelope、step、gate、latch、reset 算子合同。连续数值与事件不能隐式互连；
组件输入从内部端口推导事件类型。被求值的节拍源必须有工程网格，音频频段范围
和初始步数在编译前校验。无延迟事件环沿用现有全图拓扑检查而拒绝。

Windows/Android `event_graph_tests` 通过，日志
`out/p2-event-graph-windows-tests.log`、`out/p2-event-graph-android-fixed-tests.log`。
测试最初误认为组件输出会分配更大 ID；实际合同规定输出保留实例 ID，内部其他节点
才重新分配。已按此合同核对展开后事件输入仍指向外部源；失败记录保留在
`out/p2-event-graph-android-tests.log`。当前只完成图模块，运行执行、UI 标签/预览、
资产版本与应用流程尚未交付，不把可编译的节点声明当成功能可用。

### Runtime 接入

`runtime::detail::EventNode` 独立管理来源与消费者状态，图按既有拓扑顺序传递不可变
EventBatch；同一输出可以分支，合流及消费者按身份去重。序号由 Runtime 统一发放，
节点属性/类型编辑及资源重建不会复用先前的事件 ID；事件节点才分配状态，普通节点
不增加事件数组。快照、音频设备、UI 和图编译线程不直接修改此状态。

首批 beat、Cue、audio-onset、带迟滞的 scalar-edge、merge、ADSR、step、gate、
latch、reset 转换已在真实 Runtime 空渲染器路径检查。gate 的 pulse 切换开关，gate
事件显式设开/关；latch 在触发帧捕获输入，gate-off 不清空，reset 恢复初值。step
按有界整数模运算步进。reset 转换也可接到具有可选 reset 端口的状态节点。

节拍/Cue 使用配置的媒体时间。音频 FFT 瞬态有检测延迟，来源在首次观察到新的
onset_id 的显示帧发出事件，时间记录该显示帧，不冒充在过去的 onset 拍点已经显示。
音频代次变化不补发旧瞬态；band_first..band_last 的峰值筛选触发，峰值为载荷幅度。
首次求值/seek 只建立当前时间基线，不回补跳过的 Cue。Cue 来源还显式要求编译器
保留其快照所需宏节点，避免不连画面的宏被裁剪后连同 Cue 丢失。

原生图批次上限为 256，合并临时集合上限 512，消费者去重来源 128。超限报告
FrameResult/NodeOutput.rejected_events，不默默声称所有触发都已执行；来源在异常
大时间跳变时截断本帧剩余事件，下一帧不补发被拒绝部分。该图内传播策略与宿主
EventQueue（1024 等待、256 派发并保留积压）分别适用，不把队列预算扩张到每条图边。
主机 UI 的可见提示、总图传播成本、记录重放以及完整作品验收仍在下一增量。

Windows/Android Runtime 检查通过：日志
`out/p2-event-runtime-identity-windows-tests.log`、
`out/p2-event-runtime-identity-android-tests.log`。覆盖合流去重、逐帧清除脉冲、暂停、
来源属性变化、Runtime 重建不复用序号、包络、音频重启/静音过滤、门控、锁存、
reset 转换、无连续宏连接的 Cue、显式重启与异常跳变预算。不等于 GPU/UI 完成。

### 指定状态重置增量

CPU/GPU 粒子、物理、拖尾、反馈追加可选 Event reset 输入，保留原有端口索引；
脉冲、gate-on 或 reset 清除该节点状态，gate-off 不重置。消费序号防止同帧重复
求值再次重建，暂停不消费。反馈的图像输入仍是跨帧依赖，但 reset 输入是当前帧
拓扑依赖，不能连同图像输入一起跳过环检查与排序。旧运行包自动补齐缺少的可选端口。

Windows `event_reset/event_runtime/program_contracts` 通过，日志
`out/p2-event-reset-windows-tests.log`；USB Android 定向 CPU/GPU 重置和图排序通过，
见 `out/p2-event-reset-android-tests.log`。该日志也保留了旧 emitter 合同断言失败：
追加 reset 后，测试夹具只删最后一个输入已不再模拟旧的两个端口。修正夹具同时删去
flow/reset，检查恢复两个空可选输入后，Android 运行包合同通过，日志
`out/p2-event-reset-compat-android-tests.log`。

此项验证使用空渲染器，实际检查 CPU 粒子清空、GPU 资源句柄失效/重建、独立发射器
状态保留及重复帧/暂停不重复重置；不是实际 GPU 像素验收。反馈/拖尾像素、物理状态
重置以及完整 UI 作品的证据继续随 P2 交付补齐。

### 事件观察与编辑器交互

事件输出保留累计产生次数、最后序号和时间，低频预览从相邻观测差值绘制柱状图，
避免 15 Hz 预览漏掉图在中间帧产生的脉冲。首次加入观察只显示当前批次，不把此前
所有历史塞入一个采样柱；隐藏后重新观察有同样语义。暂停不滚动或重复计数，seek/
运行时重建清除历史。原有 8 个共享活动预览预算不变，组件内使用展开 ID 路由回本地 ID。

编辑器加入独立事件分类、橙色端口、中文/英文标签与帮助；事件直方图仍是只读展示，
鼠标从图上拖动移动节点，保留连线。事件拒绝累计报告在主画布工具栏下可见，诊断面板
也显示；报告数可能表示截断区间，不能解释成精确丢失次数。运行时重置清零报告。
Windows/Android `signal_previews/event_runtime` 通过，含预览间单次/多次触发、重复采样、
重启和瞬时超限报告保留：`out/p2-event-observation-windows-tests.log`、
`out/p2-event-observation-android-tests.log`。真实 ImGui 输入的事件预览拖动、既有千节点
画布回归、组件路由及 Event 类型帮助通过：`out/p2-event-ui-tests.log`。
这里仍不是 Studio 安装包/GPU 完整作品交付证据；随版本与作品增量一起执行交付回归。

### 保存、组件与发布版本

含 event.* 根节点或组件定义的工程使用 schema 7；节拍网格在 schema 7 可选，
schema 6 仍要求存在。包含事件指令的发布程序及 manifest 使用 ABI 5；无事件的
既有工程/包沿用原版本。事件冒充较低版本在读取时明确拒绝。未连接的状态 reset
端口不写入旧算子的尾部，保持普通作品原有 wire shape；读取补齐可选端口。

`event_io_tests` 实际执行：封装事件包络/图像为带事件输入的组件、模板应用分配新 ID、
单事务撤销重做、工程 Save/Load、展开组件、EncodePackage/DecodePackage，再由真实
Runtime 在 .5 秒触发并在 .505 秒验证包络值 .5。另检查未使用的事件组件仍要求
schema 7、无网格可读、普通图版本不变、最低版本拒绝。Windows 与 USB Android
通过，连同旧程序与宏/Cue/网格合同回归，日志 `out/p2-event-io-windows-tests.log`、
`out/p2-event-io-android-tests.log`。这是原生存储/执行路径；完整 Studio 模板 UI/GPU
及事件作品仍随本轮交付验收，不用该测试替代。

### 两端 GPU 状态与可见拒绝报告

ADSR 定时输出、反馈/拖尾独立重置与 GPU 粒子清空已经完成实际像素检查；物理重置
完成原生位置/身份检查。期间发现并修复 GLES 计算视图继承旧绘图状态的问题，详见
[失效路径与永久回归](validation/compute_view_reuse_2026-09-09.md)。Windows Player
和 Android 状态区域也接入事件拒绝累计报告；本轮 Windows Player 启动通过
`out/p2-player-event-diagnostics-windows-tests.log`，Android 编译打包并 install -r 成功。
事件完整作品与 Studio 模板应用仍在下一次交付中验收；该安装包还未包含后续计算视图修复。
