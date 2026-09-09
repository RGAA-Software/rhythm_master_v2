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
