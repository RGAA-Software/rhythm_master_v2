# Android 节拍量化 UI 检查（2026-09-09）

目标设备：USB `e2b3b128`，Redmi K40S，Android 14。仅 `adb install -r` 覆盖安装，
未卸载、清空应用数据或改变系统方向规则。仍按当前场景画布选择方向。

## 原生与界面边界

共用 `SceneDeck`、`PerformanceActions` 和 `BeatGrid`。JNI 只在互斥锁下提交有界值命令，
渲染线程才修改场景、控件和队列。发布新作品时原子更新元数据、网格与显示值，清空
旧待处理命令并更新页面代次；旧页面编辑被拒绝。执行时机在两个原生页面之间共用，
场景页可查看/取消待切场请求。日志 tag `RhythmPerformance` 记录请求 ID、状态、
目标时间和实际媒体时间，不把 UI 按钮受理当作已经执行。

USB 原生 `android_control_bridge_tests` 已通过：真实 JNI 数值入口→宿主应用命令→
`SceneDeck::Tick` 空渲染器检查，覆盖召回、暂停、取消、手动覆盖、网格、敲击和
非法/过期目标。日志 `out/p1-android-control-bridge-tests.log`。

## 实机发现的刷新竞争

第一次 APK 实操，用户选择“下一拍”后，现场请求仍是“立即”，暂停时目标显示
`0.000 s`。`out/p1-android-beat-pending.png` 与
`out/p1-android-beat-device.log` 保留了该失败，原生合同通过不能替代 UI 操作。

BeatControls 的 200 ms 刷新轮询把 native 中上一次的 mode 写回 Spinner；Spinner
选择通知可能在后续布局时才派发，旧轮询状态会覆盖尚未提交的新选择。修复后只在
页面初始化、显式关闭网格及拒绝选择时恢复模式，不对用户正在编辑的选择反复赋值。
模式仍由原生宿主保留，重新打开页面从原生读取。

新增 `tools/test-android-beat-ui.py` 永久回归：启动已安装应用→暂停/归零→启用网格→
选择下一拍→跨多次刷新并重开页面→暂停召回→取消→恢复执行。保存每步 UI 层级、
关键截图和当前进程请求日志；不依赖重跑一次掩盖失败。要求选中横屏且有快照的作品，
不改应用数据、不会安装或选择文件。该脚本不是热测试或长稳测试。

实际 APK 检查通过，证据目录
`out/android-beat-ui/57e7b43970504dde8e6ff28085da185e`：

- 选择 Chorus（与起始 Breeze 不同），重复请求只有一次待执行记录；取消后再请求，
  目标为 0.500 秒，实际在 0.512 秒完成一次。
- 界面从原生帧读取的 Swing pace 从 0.650 变成 1.250，Exposure 从 0.450 变成
  0.600；截图 `changed-runtime-values.png`。不是只检查“已执行”标签。
- 模式跨多次轮询和重开页面保留，暂停期间不执行；日志无当前进程 Java 异常。

追加实际切场：Daylight Mobile 暂停在 1.120 秒，通过队列的内置目录添加
Luminous Concerto，点击下一场后目标为 1.500 秒；取消仍保留准备包。再次请求后
退到桌面并返回，媒体仍为 1.120 秒，待执行 ID 和目标不变。恢复播放后请求 #4 在
1.504 秒首次有效渲染完成，最终画面和标题切为 Luminous Concerto，配乐接管。
证据：`out/p1-queue-{pending,cancelled,background-valid}.xml`、
`out/p1-queue-device.log`、`out/p1-queue-takeover.png`。

一次返回前台后的 UI dump 未达到 idle，不能读取此前残留 XML 当作新证据；该次
`out/p1-queue-background.xml` 作废，实际页面稳定后重新捕获到 `background-valid`。
永久脚本增加 dump 完成标记检查，失败直接停止，不复用旧层级。
以上是短功能检查；跨画布方向和更新后的示例验收继续随 P1.5 补充。
