# 连续改速：节点、播放时钟与 Windows 验证

2026-09-10。Windows 性能增量已提交并推送 `2c4293d`。
本增量已接入 `time.phase`、Studio、Player 和四件 0.4.0 作品。
底层与真实音频消费路径已通过定向测试；Windows 图像与交付证据见下文。

## 实现与复用判断

核对现有 `FrameClock`、`PlaybackClock`、`EvaluateScalar`：`time.local` 与表达式
均直接映射时间，不能保留改速前的运动量。保留这些节点现有确定性语义。

阅读本地 `C:/source/rh_reference/tixl`，版本
`fbc994d923e8a0142d2ff1b772e4d12248c5b0ba`：

- [Accumulator.cs](https://github.com/tixl3d/tixl/blob/fbc994d923e8a0142d2ff1b772e4d12248c5b0ba/Operators/Lib/Symbols/numbers/float/process/Accumulator.cs)：支持按秒累加和模运算，但用当前增量乘已过去的区间，未提供本项目的暂停预计算、显式定位和输入边界契约。
- [RunTime.cs](https://github.com/tixl3d/tixl/blob/fbc994d923e8a0142d2ff1b772e4d12248c5b0ba/Operators/Lib/Symbols/numbers/anim/time/RunTime.cs)：读取宿主运行时间，不能直接替代音频消费时间。

源许可为已有快照记录的 MIT。此次只研究行为，无新增导入或翻译文件；项目自行实现
窄时间契约，复用现有播放时钟提供输入，不引入 C#、TiXL 宿主或新依赖。

## 基础契约

- 输入是非负播放秒数，速度范围 ±1e6，周期范围 1e-6 至 1e6 秒，均须有限。
- 初始状态按 `seconds * rate mod period` 定位；积累前先缩减时间，避免巨大时间溢出。
- 已观察到的速度用于其后的时间区间；新速度从当前时刻开始生效，不重算过去。
  同一时刻反复求值或改速不移动相位，速度 0 停止，负速度反向。
- 恒速区间按锚点求值，不逐帧累计浮点误差，避免 30 / 120 FPS 的周期边界漂移。
- `advance=false` 保持现有相位并观察时间锚点及速度；调用者必须提供播放时间，
  不能把暂停期间未观察的墙钟间隔作为恢复后的运动时间。
- 倒退或周期改变重新定位；向前显式 seek 由调用者重置该值。它不猜测 seek 意图。
- 对相同时间戳的速度变化，零阶保持积分可重放。不同帧率采样平滑曲线不保证逐像素
  相等；实时手动操作未记录，也不能由导出凭空重建。

## 已做验证与限制

`tools/build-windows.py --target motion_phase_tests` 使用 20 worker 增量构建。
`tools/verify_windows.py --log out/motion-phase-tests.log -- ctest --test-dir
out/windows-release -R '^(motion_phase|playback_clock)$' --output-on-failure` 验证：

- 改速前后与重复时刻、停止和反向、暂停观察/恢复、倒退定位；
- 30 / 120 FPS 同时间戳分段速度重放，周期边界与反向越界；
- 无效输入拒绝且不污染状态、极大时间保持有限；
- 原有播放时钟的音频主时钟、暂停、seek、loop、挂起和断开回退回归。

日志为 `out/motion-phase-build.log`、`out/motion-phase-tests.log`。
以上是 CPU 时间契约测试，不是 GPU 画面、实际滑块、媒体循环或有声导出验收。
未构建或验证 Android。

## 已接入的路径

1. `PlaybackSample.continuous_` 传值类型 `MotionTime`。音频宿主直接复用既有
   `consumed_frames_ / kAudioSampleRate` 和 `source_generation_`：消费计数跨循环
   单调，load/seek 的源确认改变 epoch。不新增解码、线程、设备或另一套音频时间。
   原有 generation 仍重置事件历史；确认连续的循环保留运动、反馈、粒子与纹理历史。
   主动重置、seek 或新 epoch 仍重建渲染/模拟历史，不能借循环标记绕过重置。
   没有提供连续时间的旧调用方保持原有重置行为，不靠时间倒退猜自然循环。
2. Studio 本地时间轴 Loop 使用重基准，主动 Seek 使用新历史。
   Player Session 与演出双场景时钟沿相同值契约传递运动时间。
   `time.phase` 输入为可选标量 `speed`，属性为备用速度与 `duration`；输出为秒。
   图未删除该节点且运动 epoch 未变时，修改控制、渲染尺寸和反馈重置保留 CPU 相位。
   明确释放整个 Runtime、替换作品或设备恢复重建仍开始新历史。
3. 四件作品使用 16 秒闭合相位，不改粒子、材质、相机和几何轨迹。
   每件减少一个旧时间表达式节点，数量分别为 161 / 205 / 115 / 702。
   运动速度从 Cue 快照中省略，复用快照缺省值解析：保存后的速度不会被各段 Cue
   强制覆盖成 1。音乐响应与曝光仍按原有 Cue 淡化。
4. 现有导出只从零开始、固定 FPS 顺序求值共享 Runtime，本节点自动走相同积分路径。
   保存的恒速可确定重放；未来给速度接曲线时，同时间戳采样才代表同一输入。
   未录制的现场调速过程不等于保存后的恒速作品，不承诺导出重放未保存的操作历史。
   不新增不存在的非零起点导出、预滚或现场录制功能。

## 接入回归

- `out/motion-node-tests.log`：graph、scalar、phase、playback_clock 4 项通过。
  实际图编译、同刻改速、前进、新周期、暂停、尺寸变化与显式定位均检查相位输出。
- `out/motion-host-tests.log`：Player 包、场景双时钟、真实音频宿主及媒体 fixture
  共 5 项通过。真实播放越过循环仍保持运动 epoch；seek 更换 epoch 并重置消费计数。
- `out/motion-anchor-tests.log`：锚点积分最终版本 4 项通过，增加恒速 961 帧
  周期精度检查，保留已有 30 / 120 FPS 分段改速重放检查。
- `control_delivery_checks.cpp` 永久检查实际鼠标改速→保存→重开→发布后的 Cue
  求值仍使用保存速度；不是只检查 JSON 默认值。
- `music_gpu_tests --pace` 在真实渲染路径中于 6 / 14 / 22 秒改速到 2 / 0.5 / 1.25，
  播放时间在 16 / 32 秒循环；逐帧核对实际相位，并分别提供音乐与零音频输入。

最终 Windows 交付使用 `tools/build-windows.py --target studio_deploy --target
player_deploy --target export_ui_gpu_tests --target soundtrack_studio_gpu_tests`：九项检查
全部通过（editor、soundtrack、template、large music、content、双语言模板切换、公开
控件、large soundtrack），总计 113.36 秒，记录于
`out/windows-release/studio-delivery-tests.log.runs/1789033316251692300.log`。

随后从当前源码绑定的四件 0.4.0 作品实际点击 Studio 导出有声 H.264 MP4：每件 480 帧
均通过，源哈希和结果位于 `out/concept-review/e17ef312e9c0428495137351e2691fee/`。
再以 `quiet.wav` 执行绑定、保存、发布、清空和重开，四件均恢复同一音乐 SHA-256、
3000 个波形 bin 和当前图的 161 / 205 / 115 / 702 条指令；结果位于
`out/concept-review/6cbaac3fa79c41c59bd466e27ef9efe2/`。这两批检查与此前 32 秒音乐/
静音相位 GPU 证据共同构成 Windows 当前源的功能验收，不代替用户审美验收。

Android 和长时间性能/耐久测试仍不在本轮范围。

首次交付的内容完整性检查发现 `Missing default preset: time.phase`。
失败记录保留在 `out/motion-delivery.log` 和
`out/windows-release/studio-delivery-tests.log.runs/1789032166210999800.log`。
已在统一预设目录补充与注册表默认值严格一致的 `official.motion_phase.default`；
它是必需默认项，不计为新增视觉预设或作品。保留强制检查，不跳过失败用例。

首轮变速 GPU 检查在第 480 帧报 `music.texture_growth`。诊断复跑记录
`out/motion-pace-diagnostic.log` 表明占用从 127180804 降到 119808004 字节，
差值恰为一张 1280×720 RGBA16F 中间纹理。自然循环重置旧反馈池，首图尚未
建立稳态复用余量；相位在该帧仍准确为 7 秒。临时允许边界下降后继续调查，
确认原路径还会重新初始化粒子。最终改为在确认连续的边界保留模拟与渲染历史，
模拟使用连续消费时间，事件仍使用节目时间。已撤销临时例外，恢复每一帧严格稳态检查。
原失败证据保留在 `out/concept-motion/05f870249c954c589c0046ce7ae9784a/`
和 `out/concept-motion/7955545ce0c14fb1ab8358fb7b94e2aa/`。

图像回归还发现原 seam 检查把第 32 秒的 1.25 倍速与第 16 秒的 0.5 倍速比较。
`out/concept-motion/7d0e1a75f6744c9e85924eacb2e6eca0/` 保留该失败。
增加第 958 帧，分别用 479→480 / 480→481 和 959→960 / 958→959 比较；
维持原来的相邻帧两倍阈值，比较同一速度附近的位移，不扩大阈值。

`out/motion-history-tests.log` 最终模拟历史修复的 7 项回归通过，包括：
自然循环沿用 GPU buffer 并执行两个固定模拟步、主动反馈重置销毁旧 buffer、
新 motion epoch 即使错误请求保留也不能沿用旧历史，以及事件、CPU 点、拖尾和 Player。

最终变速图像证据：`out/concept-motion/cbe8d80578c349c497ff0bd4e096eea2/`，
四件作品音乐/静音各 32 秒全部通过。每路 961 个相位样本，三次速度变化、两次
音乐循环；相机轨迹、循环邻帧和纹理稳态检查通过。已审看四件作品的多时刻画面，
保留金/青粒子旋臂、瓷瓣、墨流与光门视觉结构；用户最终审美验收仍待确认。
