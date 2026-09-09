# 节拍网格与量化控制

2026-09-09，P1 第一个增量已完成纯合同和 Windows/USB Android 检查。
P1.1 已接入图编译、工程存储和运行包；控件和量化动作执行继续按整体计划推进。

`parameters::BeatGrid` 是不可变数值网格，不拥有时钟。BPM 固定表示每分钟四分音符数，
范围 20–600；每小节 1–32 拍，拍号分母为 1/2/4/8/16/32。比如 120 BPM、6/8 的
记谱拍为 0.25 秒，小节为 1.5 秒，不把敲击八分音符估成两倍的四分音符 BPM。

原点是小节 0/拍 0，可在 ±1e9 秒内偏移。网格可向原点之前延伸，位置采用数学
向下取整；查询时间为 0–1e9 秒。`NextAfter` 的立即模式返回当前时间，下一拍/小节
严格取未来边界；预算末尾无下一边界时返回空值。比较实际可表示的边界，避免用
固定 epsilon 把刚好在边界前后的请求提前执行或错误跳过一拍。

手动 `TapTempo` 接收调用者给定的单调时间，最多记 8 次有效敲击，4 次才输出估计。
快速误触不加入，长间隔、倒退时间或拍号分母变化重置。基于本地 TiXL 的敲击间隔
平均做聚焦适配，来源与 MIT 通知记录在 `provenance/beat_grid.json`。
不导入其静态 Playback/BeatSynchronizer 或墙钟推进；不声称提供自动拍点锁定。

验证包含 4/4、6/8、偏移原点前的负拍号、下一拍/小节、严格边界前后一 ULP、
20/97/120/600 BPM、±1e9 原点、非法输入、预算末端、敲击误触/重置和八分音符定速。
Windows `beat_grid` 及 USB `e2b3b128` 原生测试通过；日志
`out/p1-beat-grid-tests.log`、`out/p1-beat-grid-android-tests.log`。
这不是 Android UI 交付，未因此重装应用。

## 工程与运行包兼容

`Document::beat_grid_` 和 `ExecutionPlan::beat_grid_` 使用可选值。未配置时保留原来的
schema 1–5 读取与 schema 2–5 写入规则，以及 ABI 1–3；显式配置（包括默认值）时
工程使用 schema 6，运行程序及 manifest 使用 ABI 4。新版本标识与网格消息必须同时
存在；拒绝缺失、伪造降级、不合法数值和重复的网格消息。旧读者已有的版本上限检查
会拒绝新格式，不允许静默丢弃。未配置不会隐式启用手动网格。

根图展开组件时继承该网格，组件定义不另设全局节拍。模板替换保留来源网格，撤销
恢复先前网格。保存/重开保留嵌套未知字段；删除网格时也删除对应扩展消息。
无宏节点、有宏无 Cue、有 Cue 三种作品均验证，发布音乐包复用同一 ABI 编码路径。

2026-09-09 验证：Windows `control_graph`、`components`、`control_codec`、
`program_contracts`、`package_contracts`、`persistence_contracts`、`editor_contracts`、
`template_contracts` 通过；USB Android 原生 `control_codec_tests`、
`editor_contract_tests` 通过。文件级检查覆盖保存→重开→发布→读取实际文件。
日志为 `out/p1-beat-{graph,codec,persistence,template}-tests.log` 与
`out/p1-beat-codec-android-tests.log`。这些是合同/文件路径检查，尚非量化 UI 验收。

## 有界量化请求（P1.2）

`player::PerformanceActions` 由宿主线程持有，复用 `PlaybackSample` 和 `BeatGrid`。
快照与下一场各一个槽位；相同未完成目标/模式重复点击返回原请求 ID，不移动目标拍。
不同请求替换该类槽位并保留被替换 ID。状态区分等待、已派发、已完成、取消和失败；
派发后的宿主实际操作成功才确认完成，失败报告目标不可用。每个请求最多派发一次。
暂停/挂起（包括立即模式）等待；换工程/播放代次变化/倒退时间取消，网格变化另报原因。
无网格不能量化，预算末尾无后续拍点时明确失败。没有线程、时钟外推或资源所有权。

已查阅本地 TiXL `Core/Animation`、`Editor/Gui/Interaction/Timing` 的时间与敲击
控制；其静态播放/同步状态不能直接承载本项目的来源代次与有界请求语义。本增量
复用既有项目时间/网格合同，新增宿主适配逻辑，不引入第三方调度服务。

Windows 与 USB Android 原生 `performance_action_tests` 通过；日志
`out/p1-performance-actions-tests.log`、`out/p1-performance-actions-android-tests.log`。
检查覆盖两种并行请求、边界一 ULP 前、恰在边界、重复派发、替换、取消、失败确认、
暂停/挂起、来源/网格失效、非法输入不修改现状和 30/60/144 Hz 首个跨界帧。
目前只是共享状态机；实际按钮到渲染/切场接入属于紧接着的 P1.3/P1.4。

元数据交付补充：Windows Studio/Player 自动部署各 20 个 DLL 与完整资源；四项强制
模板检查通过，日志 `out/p1-beat-metadata-delivery.log`。

### Player 帧边界接入

`SceneDeck` 的主播放时钟推进后、当前场渲染前派发快照；过渡在同一边界开始。
下一场请求只记稳定队列项 ID，准备包仍归队列所有，目标时间才取出。删项、准备失败、
忙碌或目标不再是队首时失败，不能误消费后续项。过渡请求只有首次有效渲染通过后
才确认完成；GPU 预算拒绝保留当前场。新作品恢复自己的节拍与宏值。

Windows 与 Android 原生 `scene_deck_tests`、`scene_queue_tests` 通过，覆盖实际
`SceneDeck::Tick` 的空渲染器路径、快照当帧值、暂停/恢复、手动覆盖、改 BPM 取消、
量化队列切场与删除目标。日志 `out/p1-performance-deck-tests.log` 和
`out/p1-performance-deck-android-tests.log`。这尚不是 GPU/UI 或 APK 控件验收。
