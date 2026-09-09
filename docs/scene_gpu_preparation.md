# P3.4 分帧 GPU 准备

状态：基线已测，准备调度尚未实现。2026-09-09，基线实现 `f7b8011`。

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
