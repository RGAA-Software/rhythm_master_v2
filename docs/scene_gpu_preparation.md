# P3.4 分帧 GPU 准备

状态：Runtime 分步执行已在 Windows D3D11 / Android GLES 验证；Session、队列和
Player 调度正在接入，尚未作为应用功能交付。2026-09-09，基线实现 `f7b8011`。

## 复用与实施约束

复用本项目 Runtime、TextureLifetimes/TexturePool、PreparedPackage、Session 和
SceneDeck；GPU 调用始终在宿主渲染线程，工作线程只发布不可变 CPU 资源。
已阅读本地 TiXL（https://github.com/tixl3d/tixl，MIT，
`fbc994d923e8a0142d2ff1b772e4d12248c5b0ba`）的
`Core/Utils/AsyncComputation.cs`、`Core/Resource/TempResourceConsumer.cs` 和
`Core/Resource/ResourceManager.Graphics.cs`。其输入版本/取消/完成发布作为行为
参考；Task/Slot/SharpDX 资源不能直接作为本项目 bgfx 渲染线程上的分帧执行器。
现有 PackageLoader 已负责相同的后台版本/取消责任，不新增另一条后台加载链。
没有复制外部源码或冻结新依赖。

## 基线证据

共享 `VerifySceneDeck` 运行 100 帧《光幕协奏》→《墨潮》，Balanced 960×540，
第 10 帧开始来场，第 40/99 帧读回真实像素并确认当前作品身份。
计时分别记录 Tick/提交和包括 EndFrame 的宿主耗时；读回等待帧不计分位数。
它们不是硬件 GPU 时间戳，也不是 Studio UI FPS。Windows 可能受垂直同步等待影响。
Android 独立 GLES 探针在应用停止后运行；不把此探针当 APK 控件/音频设备验收。

| 指标 | Windows D3D11 | Android GLES（Redmi K40S） |
| --- | ---: | ---: |
| 首个当前场 Tick/提交 | 0.868 ms | 0.925 ms |
| 首个当前场包括 EndFrame | 15.06 ms | 251.59 ms |
| 来场首次 Tick/提交 | 0.640 ms | 0.673 ms |
| 来场首次包括 EndFrame | 18.34 ms | 22.24 ms |
| 双场包括 EndFrame p50 / p95 | 16.65 / 16.82 ms | 5.00 / 11.71 ms |
| 双场峰值纹理 / pass | 40,323,464 B / 37 | 40,323,464 B / 37 |

日志：`out/p3-creation-frame-boundary-windows-tests.log`、
`out/p3-creation-frame-boundary-windows-detail.log`、
`out/p3-creation-frame-boundary-android-tests.log`。早期仅 Tick 计时的日志
`out/p3-creation-baseline-*` 保留，不能用其约 1 ms 结果宣称没有首次帧停顿。
首帧包含后端冷启动/驱动工作；本测量不能把全部等待都归因于某个具体节点或上传。

## 下一实施增量

1. Runtime 接受固定计划/固定零点输入，按节点数量和协作式时间额度分步准备；
   中间结果不可呈现，不提交动画/反馈时间推进，保留跨步纹理生命周期。
2. 视频首帧上传与图执行一起分步，取消/资源拒绝释放所有部分资源；
   单个不可拆分后端操作允许超过时间额度，必须单独报告，不能宣传硬实时上限。
3. Session / SceneDeck 把 CPU 已准备、GPU 准备中、可呈现与正式过渡分开；
   准备下一场时当前场继续运行，尚未准备完的场不能抢占当前输出。
4. 队列/量化在可呈现条件满足后执行，明确处理准备和音频队列造成的延迟；
   不把“按钮在整拍发出请求”当“画面和声音已经在该整拍到达设备”。
5. 在同一探针比较创建峰值、稳定帧、取消/失败/资源释放和最终像素，
   然后更新 Windows deploy / Android APK，继续 P3.5 综合演出工作流。

## Runtime 增量证据（2026-09-09）

`BeginPreparation` 捕获计划与固定输入，`PrepareNext` 默认每步最多 8 个节点、
2 ms 协作式 CPU/提交额度；宿主仍负责 BeginFrame/EndFrame。只在节点边界让出，
单次节点/驱动操作及 EndFrame 可以超过额度，记录最大节点耗时，不承诺硬实时。
视频按节点上传，跨步保留纹理生命周期；准备期间不推进反馈历史、不暴露部分输出。
取消、设备变化和预算失败释放候选资源；普通 Evaluate 不得混入未完成的准备。
累计离屏 pass 仍受共享 240 上限约束，分帧不能使无法正常播放的图通过准入。

Windows `out/p3-staged-pass-budget-windows-tests.log`：runtime_preparation、
preparation_gpu、render_contracts、source_boundaries 4 项通过。
Android `out/p3-staged-pass-budget-android-tests.log`：相同 Runtime/渲染合同与
真实 GLES `--preparation` 通过。250 节点链超限拒绝；视频分步上传、取消释放、
已有场景资源受保护均有检查。共享 GPU 用例分 8 步完成视频/变换/反馈图，准备后
像素及随后 3 个动态帧与一次执行逐像素相同，覆盖动态纹理复用与冻结反馈历史。
这组 Android 证据是独立原生探针，不是 APK 交互；首帧准备也不代表预加载了
未来所有视频片段或任意动态分支。

### Session 接入

`Session::PrepareGraphics` 固定场景时间/输入并绑定随后的播放时钟代次；异步等待
当前时间所有活动视频首帧（含裁剪与偏移），最多等待 10 秒，再执行 Runtime 分步。
未到齐时不创建候选图 GPU 输出；失败保留诊断，显式释放/重新加载后才能重试。
Windows `out/p3-session-preparation-windows-tests.log` 的 player_contracts、video_player、
视频 fixture 与 source_boundaries 通过；Android
`out/p3-session-preparation-android-tests.log` 同样通过原生 Session/视频检查。
25 步图准备期间时间为零，首次暂停呈现复用原句柄且不新增 pass；两路重叠裁剪
片段均就绪后才完成准备，取消释放部分资源。SceneDeck/队列交付仍待后续增量。
