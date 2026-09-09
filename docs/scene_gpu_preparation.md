# P3.4 分帧 GPU 准备

状态：Runtime、Session、队首预备与两端 Player 已接入并短操作交付；
正在补齐逐准备帧性能记录，P3.5 连续演出与资源不足策略尚未完成。2026-09-09，基线实现 `f7b8011`。

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

### SceneDeck 分步切场

SceneDeck 来场使用 Session 准备，正式淡化计时在准备完成后开始；共享纹理准入
及已观测旧场 + 候选累计 pass + 合成 pass 准入生效。动态分支后续成本仍由后端
逐帧限制，冻结首帧成本不是全作品上界。33 节点链覆盖旧场连续播放、隐藏中间
输出、取消释放、重新准备和最终接管；音频必须等 Graphics Ready 才发出请求。
Windows `out/p3-deck-preparation-windows-tests.log` 6 项通过，详情留在
`out/p3-deck-preparation-windows-detail.log`。Android scene_deck / scene_audio_deck
通过；首次调用 bridge 探针参数错误（要求 fixture 目录），失败日志保留在
`out/p3-deck-preparation-android-tests.log`，修正参数后的 bridge 与真实 GLES 切场
通过，见 `out/p3-deck-preparation-android-corrected-tests.log`。

本次 Android 来场第 10 帧仅增加 4 B 白纹理，包含 EndFrame 13.53 ms；100 帧范围
峰值纹理仍为 40,323,464 B，34 pass。双场范围 EndFrame p95 17.46 / max 28.93 ms，
没有比基线全面改善；不能仅凭第 10 帧下降就宣布卡顿已解决。接下来记录每一个
准备帧，并将 GPU 准备提前到队首等待阶段，避免在 Go 后才开始准备。
尚未更新两端应用 UI 或交付 APK，本增量是 SceneDeck 原生/实际像素验证。

### 队首准备与两端交付

队首状态为 Queued → Loading → CPU Ready → GPU Preparing → Presentable；素材
已准备不再等同可切场。SceneDeck 在旧场正常运行期间准备唯一候选场，行/稳定 ID
保留到合法 Go。GPU 失败保留原行和确切错误，可重试；删除、清空、列表替换
丢弃旧候选，表面释放与画质变化撤销 GPU 就绪并重新准备。Go 仅消费匹配的
Presentable 行；量化动作在候选可呈现后由用户发出，准备不再挤占 Go 后的首帧。
这仍不消除音频设备队列延迟，不把量化请求时间声称为实际可听开始时间。

Windows 核心 `out/p3-queue-preparation-core-tests.log` 首次失败为压力 fixture 按
1280×720 估算，而实际默认画布为 640×360，未造成预期资源拒绝；修正 fixture
压力后 `out/p3-queue-preparation-pressure-tests.log` 通过。其余核心检查及 Android
`out/p3-queue-preparation-android-core-tests.log` 通过，覆盖失败行、诊断、重试、
旧场保留、稳定 ID、表面重建和量化接管。

Windows `out/p3-queue-preparation-player-tests.log` 的 scene_queue_ui、scene_audio_ui、
windows_player_smoke、source_boundaries 及媒体 fixture 5 项通过。
`out/windows-release/src/windows_player/deploy/rhythm_player.exe` 已携 20 DLL/资源部署。
Android 覆盖安装 APK SHA256
`6b2ee4ba68c5ae97f2bea981cd797a03df86e790bc7c66baf5020def4f4a3e40`，真实触摸证据
`out/android-scene-audio/d77d874cbbd94851bda1f3ef08a37835/`，列表重开证据
`out/android-program-ui/462579d0fb8149ea9fd7c07f6493a281/`。
实际 AAudio 消费帧 107520 → 119040 → 167424，完成切场，保存列表逐字节未变；
检查结束保留应用并暂停。不含声学回录、听感或长时间验收。
