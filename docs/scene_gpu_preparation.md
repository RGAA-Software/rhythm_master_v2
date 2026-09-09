# P3.4 分帧 GPU 准备

状态：Runtime、Session、队首预备与两端 Player 已接入并短操作交付；
逐准备帧性能记录已补齐，P3.4 已交付；P3.5 连续演出与资源不足策略尚未完成。2026-09-09，基线实现 `f7b8011`。

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

### 逐准备帧与合成目标收口

同一 100 帧探针的 34 节点《墨潮》在第 10–14 帧按 8/16/24/32/34 节点推进。
新增纹理分别为 4、2,073,600、6,220,800、4,147,200、2,073,600 B；首个来场
单帧约 14.5 MB 的分配分散到 5 帧，最大单帧约 6.2 MB。Windows 包含 EndFrame
的准备帧最大 17.05 ms；Android 为 4.52、4.98、21.60、5.03、5.01 ms。
CPU 最大单节点约 0.035 ms 并不等于 GPU 驱动工作耗时；Android 第 12 帧仍超过
16.7 ms。此短测说明资源创建被分散，不证明所有作品达到 60 FPS 或彻底无卡顿。
日志 `out/p3-preparation-frame-metrics-windows-detail.log`、
`out/p3-preparation-frame-metrics-android-tests.log` 保留逐帧数据与实际像素检查。

队首准备还预先创建淡化合成目标/程序，其私有预热结果不替换当前输出；合成目标
资源拒绝同样保留旧场和失败队列行。scene_queue 检查一秒过渡 Go 当帧纹理分配
不增加，Windows 的真实 UI、音频/像素、完整 deploy 6 项检查通过，见
`out/p3-compositor-preparation-windows-tests.log`；Android 原生队列检查
`out/p3-compositor-preparation-android-tests.log` 通过。
最终 APK 再次覆盖安装，触摸/暂停 Go/AAudio 消费接管/保存列表不变检查通过，
证据 `out/android-scene-audio/8fcb29cadffc4fc5941a94d6cf55fef1/`，
汇总 `out/p3-compositor-preparation-apk-touch-tests.log`。
APK SHA256 `e95aec2ef7016f925c75c4f0e722088fbb8f8b839d1c1a006d8f0d00b33976f1`。


### P3.5：活动队列行的最终结果

Go 现在把行标为 Transitioning，并保留稳定 ID；只有 SceneDeck 确认成功接管才
移除它。运行中资源拒绝、音频失败、取消或被新作品替换，都保留失败行与具体
原因供重试。队列删除/清空仍不替代显式取消已发出的切场命令。
此更新替代上文“Go 消费队列行”的旧行为；预备源文件也随行存活到最终结果。

核心回归覆盖：Go 后强制耗尽离屏 pass，旧场与失败行保留；重试原 ID、取消、
最终成功才移除，以及准备/合成目标复用。首个 build 的测试 DrawList 聚合字段
顺序写错，日志 `out/p3-active-queue-core-build.log` 保留，改为显式宽高字段后
`out/p3-active-queue-core-corrected-build.log` 构建成功。
Windows `out/p3-active-queue-core-tests.log` 6 项与 Android
`out/p3-active-queue-android-core-tests.log` 4 项通过。
Windows `out/p3-active-queue-player-windows-tests.log` 6 项通过，含实际队列 UI、
普通淡化、四路对四路显式硬切 UI/像素和完整 deploy smoke。

Android 覆盖安装后实际 AAudio 淡化与队列触摸通过，
`out/android-scene-audio/ef835ee1de1e49f59fb78384d2f09a97/`；脚本新增完成后打开
队列检查，确认 3 行恰好剩 2 行、下一行身份正确，保存列表逐字节不变。
当前 APK SHA256 `8400aeab7952fae839579a711884a0851f3f0b8fa373920b17a065040542c537`。


### P3.5 GPU 顺序替换的输出保留基础

`Renderer::RetainTexture` 返回宿主线程 RAII 持有权，复用 ResourceTable/Texture
现有句柄与后端资源；同一纹理只计一次字节/slot，最后一个持有者释放时才销毁。
保留不会复制像素，也不会禁止写入；冻结画面的调用方必须停止原生产者。
此公共合同只暴露项目 Texture/TextureHandle，不新增原生/backend 类型或依赖。
用于后续保留旧场最后一张输出、释放其余 GPU 资源后准备新场，尚未接入播放器。

Windows `out/p3-output-lease-windows-tests.log` 的 render_contracts、readback_gpu、
source_boundaries 通过；Android `out/p3-output-lease-android-tests.log` 的同类
合同与实际 GLES 读回通过。检查多持有者、不重复预算、外设备/无效/过期句柄
拒绝、原持有者释放后蓝色目标的逐像素读回、最后持有者释放后资源失效。
它们不是完整 GPU 硬切或 APK 功能交付，后续还需验证旧场冻结、新场准入与失败恢复。


### P3.5：明确选择的 GPU 串行硬切（已接入两端）

双场 GPU 预算失败时，队列保留失败行及已校验的 CPU 包，释放来场的部分 GPU
资源。Windows/Android 显示“立即硬切”：本次覆盖条目的时机与时长，按零时长
串行替换；不会擅自降低画质或扩大 256 MiB/240 pass 上限。正常 Go 仍需预备就绪。
硬切先保留旧场最后一张输出的 RAII 所有权，再释放旧图，其余节点输出句柄不再
公开。下一宿主帧才开始分帧准备来场；不额外创建淡化合成目标。配乐仍等待新场
GPU 就绪并由同一音频设备的消费确认完成接管，四路对四路使用已有串行游标路径。

取消/来场失败时，独立 SceneReplacement 组件在最新媒体时间分帧重建旧图；身份
不变，模拟历史重新开始，不能复原已释放的反馈历史。恢复失败保留最后图像和
确切错误，不逐帧重试；提供显式恢复重试，画质尺寸变化也可重新尝试。真实设备/
surface 丢失不能保存已失效 GPU 图像，恢复期间可能暂时无输出。旧图本身加保留
图像仍可能超预算；此时需降低画质或释放其他资源，不能承诺任意临界容量都可恢复。
准备期间画面定格、媒体重开可能有间隙；这不是无缝淡化，也不承诺声音采样精确
对齐节拍。量化命令到期、提交音频、设备消费三者有不同时间，现有消费帧估计不
等于声学测量。用户操作提示明确说明本次立即硬切覆盖条目的量化/时长。

验证记录：

- Windows `out/p3-serial-deck-tests.log` 7 项通过；旧场预算、队列取消/恢复/重试、
  接管才移除、桥接和音频既有合同通过。恢复预算失败不重复创建和显式恢复重试
  已纳入 scene_replacement。surface 释放后失败条目不会误报正常 Go 就绪。
- 实际 GPU `out/p3-serial-gpu-identity-windows-tests.log`、手机 GLES
  `out/p3-serial-gpu-android-tests.log`：60 个静态纹理目标，两套图不能共存；
  保留红色输出、取消重建红色、再次明确硬切输出蓝色，逐像素核对及资源归还。
- 第一次 GPU 探针误用每个 transform 默认 scale=0.95，59 次缩放后满屏红色
  断言失败，保留 `out/p3-serial-gpu-windows-tests.log`；测试显式 scale=1 后通过。
  Null 预算检查不会验证颜色，因此不能单独代替实际像素证据。
- Windows 交付 `out/p3-serial-player-windows-tests.log` 中普通淡化、四对四音频
  硬切、队列 UI、deploy smoke、核心和边界通过；新增 GPU+音频同时超预算 UI
  用例初次断言早于 ImGui 下一帧消费激活请求而失败，诊断日志保留。修正按实际
  TransitionId 检查后 `out/p3-serial-player-ui-dispatch-tests.log` 通过，真实 D3D
  红/蓝像素、同一设备 epoch、消费完成和队列移除均检查。详细输出保留
  `out/p3-serial-player-ui-dispatch-detail.log`。没有把失败日志改写成成功。
- Windows Player 已通过 Python 自动 deploy，包含 20 DLL 和资源。
- Android APK 已覆盖安装，SHA256
  `3cb55047ee5bfd1ec4e854e1aa21a6e4e57e54ae86950bfaf2d4d5618606f980`。
  实际 UI 证据 `out/android-scene-audio/99ca4d926d3643d7957d8fddfed7e4e5/`：暂停
  Go、恢复、AAudio 消费 106752→119040→167424、成功切场后恰剩 2 行，保存列表
  未变。此 APK 触摸检查是常规一秒淡化；Android 超 GPU 预算硬切的像素证据是
  独立 GLES 探针，不能把它描述成同一 APK 的压力按钮触摸或声学验收。

P3.5 还需同一演出列表连续切换不同画布/配乐，串联方向、暂停/seek/恢复检查。
长时间稳定性仍留 P9。此增量不关闭 P3 整体，更不关闭 P4–P9。
