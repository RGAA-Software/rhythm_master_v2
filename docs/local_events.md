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
