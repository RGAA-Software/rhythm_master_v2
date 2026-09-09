# 跨作品音频淡化实施

P3.3，2026-09-09。本节是实施合同，尚不表示两端切场已实现音频淡化。

## 复用与边界

已读本地 TiXL `fbc994d923e8a0142d2ff1b772e4d12248c5b0ba`（MIT，
`https://github.com/tixl3d/tixl`）的 `Core/Audio/AudioMixerManager.cs`、
`SoundtrackClipStream.cs` 和 `SpatialOperatorAudioStream.cs::Apply3DToBuffer`：
统一 mixer/设备与正弦/余弦等功率权重可参考，其实现依赖 ManagedBass/BASS，
不能替换本项目 FFmpeg 唯一媒体后端。此次不复制其源文件。

已直接阅读 vcpkg 缓存 `ffmpeg-ffmpeg-n8.1.1.tar.gz` 中
`libavfilter/af_afade.c` 的 `fade_gain`、`activate` 与 acrossfade 配置
（FFmpeg n8.1.1，`https://github.com/FFmpeg/FFmpeg`，Paul B Mahol，LGPL-2.1-or-later）。
该 acrossfade 保留第一路末尾样本并等待 EOF，再衔接第二路；本项目需要现场触发、
量化、任意播放位置、取消和失败保留当前场。不能直接用文件拼接滤镜代替此合同。
没有自行编译 FFmpeg、没有导入该文件或新增 libavfilter 依赖。复用现有 FFmpeg
AudioDecoder、AudioMixer、文件 lease、设备队列和 AnalysisQueue，新增部分限于
两场准入、包络和交接状态；普通线性/三角函数权重不是另一个解码/播放后端。

## 第一轮合同与验证顺序

- 先以样本索引实现线性与等功率包络对照。默认线性保证两路已限幅相关信号不会
  在中点叠加超过输入峰值；等功率对于不相关信号保持能量更合适，但必须最后统一
  限幅并记录削波样本，不能宣称任意输入无削波。已知信号和实际文件听感分别记录。
- 两个来源共享 **4 个活动 PCM 解码游标**，含每份 AudioMixer 的活动片段。用同一
  RAII 预算计数，失败/取消/片段结束归还；不是每场各 4 路。沿用 AudioDecoder 精确
  seek 的原子重开语义，其单个游标重建时可短暂持有旧/新 FFmpeg context；这不算
  新播放路，但必须在准备/性能证据中区分逻辑游标与瞬时原生 context，不隐藏峰值。
- 声音混合后才送入现有设备队列和同一 AnalysisQueue，两场读取同一混合 PCM 特征。
  后台 worker 独占解码、混音和设备提交；UI/render 不读取文件，不创建第二设备时钟。
- 已排队声音不能被声称立即改变。开始/结束按设备消费确认，画面交接必须依据
  已确认的音频边界；暂停不推进淡化，seek/换源/取消清晰撤销待执行交接。
- 来场音频准备或预算失败保留当前源及画面，不能把整台 FilePlayback 标为失败后
  静音退出。过渡中途失败保留当前场，已送出声音不可逆，恢复时间需有明确证据。
- 无配乐作品保留已有音乐，两端一致；有配乐作品应用各自作者增益，设备主音量
  保持用户意图。整场导出仍在独立后续范围，不改现有单作品导出语义。

按“包络与游标预算 → 双源 PCM → 设备消费交接 → SceneDeck 准备/显示同步 →
两端操作、取消/失败、不同采样率/长度/多片段”逐个受影响模块实现和检查。

## 增量证据

2026-09-09：`media_mixer` 已加入共享 `AudioCursorBudget` 和样本索引淡化包络。
Windows `out/p3-crossfade-budget-windows-tests.log` 的 crossfade、media_audio、
audio_mixer 三项通过；Android 同源原生程序见
`out/p3-crossfade-budget-android-tests.log`。检查包含真实 FFmpeg PCM、两场四路
准入、第五路拒绝后旧场继续读取、释放后重新准入、线性相关信号峰值、等功率
不相关信号能量、削波计数、零时长、无效输入和 RAII 异常释放。
这些只证明底层混音合同，尚未经过设备声音、画面交接或两端切场 UI 验收。

双源 PCM 增量：`audio_playback` 的私有 `TransitionStream` 使用两个有界 lane，
所有文件和编排继续经 `AudioStream` / FFmpeg。以两份完整编排的最大活动片段数之和
作保守准入；峰值可能出现在不同时间也会拒绝，当前不尝试动态挤占或降低片段数量。
`StreamPcm` 分开携带来源 ID、循环次数和源内样本索引；解码块大小不同或淡化边界
不在整块位置时不会丢弃余下样本。短源在剩余淡化期间填零，循环源保留精确相位。
准备失败不移动旧游标；新源后续解码失败返回旧源下一未提交帧并保留原因。
取消仅能撤销该层尚未完成的 PCM；设备排队的回退仍由后续消费交接层处理。

Windows `out/p3-dual-pcm-windows-tests.log` 的 transition_stream、原有 audio_playback、
media_audio 和 source_boundaries 四项通过；Android
`out/p3-dual-pcm-final-android-tests.log` 的真实 FFmpeg 双源检查通过。
独立参考 PCM 逐样本比较 48/44.1 kHz 混合、作者增益、9601 帧交接、循环相位、
短源结束、延迟坏素材、取消/seek、未来四片段峰值。Windows 首次构建的测试字节
构造窄化告警已修正，失败日志保留；没有关闭告警或放宽项目规则。
Android 再次 push 后执行权限被重置；typed 日志为启动失败，恢复执行权限后才获得
上述 final 日志，不把 adb push 成功或 shell 最后一条命令成功视为检查通过。
此增量仍未改变宿主切场行为，不作为已经完成 P3.3 的证据。

消费队列增量：`AnalysisQueue::AppendHandoff` 明确准许新的来源从非零样本开始，
普通 Append 的循环必须从零开始等约束不变。排队不改变公开时间/FFT 代次，第一帧
被 Consume 后才重置到新来源已经推进的位置；旧代次和错误连续性仍拒绝。
Windows `out/p3-audible-handoff-windows-tests.log` 的 playback_analysis、audio_playback、
media_audio 通过；Android `out/p3-audible-handoff-android-tests.log` 通过相同分析队列
检查。这里只验证设备消费计数的映射，尚未接入宿主画面过渡。

回退窗口增量：PCM 已提交而设备未确认交接时，保留旧 lane 并随新场输出同步推进，
仍共享四游标预算。Cancel 或新场后续解码失败恢复到第一帧尚未提交的旧 PCM；
Confirm 才释放旧 lane 并允许下一次过渡。这样无需重新打开旧音乐或猜测循环位置，
也不会允许同时存在三场。这个 Cancel 不清除设备队列，恢复标记仍需由宿主消费层
跟踪，不能把旧声恢复的排队延迟写成零。
Windows `out/p3-queued-rollback-windows-tests.log` 和 Android
`out/p3-queued-rollback-android-tests.log` 检查通过：101 帧短淡化后取消、旧源循环
跨界、新场独占 PCM 已产生后的坏素材回退，以及确认后释放旧源。设备/宿主连接仍待完成。

双场消费位置增量：`StreamPcm` 在混音期间附带新场的循环身份和源内起点；
`PlaybackPresentation` 把同一个消费计数映射为旧/新场位置及唯一的混合 FFT。
来场循环不会重置旧场的 FFT 代次；来源交接或回退使用新的分析代次，而作品来源 ID
保持稳定。元数据按已有 32768 帧分析积压上限有界，PCM 准入失败时同步撤回元数据。
Windows `out/p3-dual-clock-windows-tests.log` 四项通过，Android
`out/p3-dual-clock-android-tests.log` 两项通过；检查对照规范 Analyzer 的完整 Features
值、不同循环位置、暂停消费不变、非零位置交接、旧源回退和拒绝后的连续性。
待接入 FilePlayback 的实际设备循环及两端场景控制，不宣称音画过渡已经交付。

设备循环增量：FilePlayback 的原有设备/PCM/分析生命周期已委托私有 PlaybackEngine，
其 mailbox、取消和单工作线程保留。引擎 Begin/Cancel 已连接两场 PCM、同一设备队列、
消费确认和恢复标记；公开异步过渡命令及宿主仍待连接。只有设备消费超过交接边界
才确认成功；排队回退在恢复标记被消费后进入 Canceled/Failed。短源都在淡化期间
结束时，用明确的 drained handoff 发布最终来源，不额外塞入假 PCM 或清零设备计数；
最后的有意静音尾段计入本次播放时间线长度，原始媒体元数据保持原样。

Windows `out/p3-device-fade-windows-tests.log` 六项通过，实际设备设主音量零；
`out/p3-device-fade-tail-windows-tests.log` 验证短源收尾时间线。Android
`out/p3-device-fade-android-dummy-tests.log` 四项及
`out/p3-device-fade-tail-android-dummy-tests.log` 通过，**这是 SDL dummy 测试设备**，
不是 APK/硬件出声验收。首次独立 native playback 测试未经过 SDL Android 入口，
失败日志为 `p3-device-engine-android-dummy-tests.log` 和 diagnostic 版本；补充
`PlaybackSnapshot.error_` 后定位为缺少 main-ready 初始化。仅测试宿主增加
SDL_SetMainReady 和显式 dummy 选择，生产宿主/后端不做 fallback。
原有实际 Windows 文件/编排播放、暂停、seek、快速替换、循环、共享文件释放和 EOF
回归继续通过；增加的引擎检查包括 101 帧淡化排队取消、60001 帧短源尾段和延迟坏素材。

异步命令增量：FilePlayback 增加 LoadSoundtrack、BeginTransition 和按 ID 的
CancelTransition。作者增益/循环随来源提交，设备主音量保持用户意图；只有听到的
交接确认后才更新被接受的来源，所以后续 seek/循环使用新音乐。一次最多一个请求，
重复请求明确拒绝；旧 worker 快照不得擦除新命令，Load/Stop/Seek 使未完成请求失效。
来场取消 token 与整体播放取消 token 在解码调用内合并，取消来场不会取消旧源。
失败细节独立于当前播放错误；排队中已产生的声音仍按恢复边界确认取消。

Windows `out/p3-async-fade-windows-tests.log` 六项通过，补充取消连接后
`out/p3-async-linked-cancel-windows-tests.log` 三项通过；Android
`out/p3-async-fade-android-dummy-tests.log` 四项及
`out/p3-async-linked-cancel-android-tests.log` 两项通过（播放接口仍为测试 dummy 设备）。
检查直接调用公开异步 API，覆盖暂停/取消、旧 epoch 保持、消费完成、作者增益在 seek
后保留且主音量仍静音、坏媒体拒绝后继续播放，以及 Stop 后不得有过期结果。
下一步是宿主的 GPU 可呈现准备与音画交接；当前 DLL/APK 尚未更新，P3.3 仍未整体验收。

宿主边界增量：没有选中旧音乐时，BeginTransition 建立显式静音旧总线（零解码游标），
仍复用同一设备/分析时钟；坏媒体不会自动替换成静音。场景宿主需在接管该零起点时
保留原有画面时间原点。两场 PCM 元数据改为主/次来源；新场独占 PCM 已排队但尚未
确认期间，次来源记录仍被保留的旧场实际位置，且在旧源循环边界拆分。公开 transition
快照可同时报告新旧位置，供宿主取消回退和场景时钟使用。
Windows `out/p3-silent-scene-windows-tests.log` 七项通过；Android
`out/p3-silent-scene-android-dummy-tests.log` 五项通过（播放检查为 dummy 设备）。
新增零音乐起始、暂停下无积压取消、静音淡入 PCM、旧源循环处的次来源位置与独立
来场取消 token 检查。仍未以这些检查替代 Player 界面、实际画面和 Android 硬件音频验收。

场景合同增量：SceneDeck 可选择音频驱动的过渡，接受不含音频服务/原生类型的
SceneAudioSample。先得到来场有效输出才报告 AudioReady；等待阶段保留旧输出，
溶解使用消费进度，只有明确 Committed 才移交作品。旧/新场循环代次分别处理。
新静音设备开始时保留旧画面原点；取消期间释放来场 GPU 会话，仍保持旧场消费映射，
直到音频恢复确认才允许下一场。资源失败与音频失败保留当前作品及具体原因。

Windows `out/p3-scene-audio-deck-windows-tests.log` 五项、Android
`out/p3-scene-audio-deck-android-tests.log` 四项通过。**这些场景检查使用 Null renderer**，
证明资源准入合同、实际 Session 当前作品/时间和交接状态，不证明 D3D/GLES 像素。
原有本地时钟溶解、队列、暂停、seek 和资源释放回归同时通过。两端 Player 入口、
实际 GPU/音频和当前 deploy/APK 尚待连接与交付，P3.3/P3.5 不据此标为完成。

宿主协调增量：新增外层 player_audio 模块，把同一帧 PlaybackSnapshot 映射为
SceneAudioSample；以独立的音频/场景 ID 绑定请求，GPU 输出准备前不启动音频，
准备期间只提交一次，取消期间等待恢复，过期音频身份不能提交场景。
Windows `out/p3-scene-audio-bridge-windows-tests.log` 三项通过；Android
`out/p3-scene-audio-bridge-android-tests.log` 原生检查通过。检查使用真实含配乐
运行包、Null renderer 和音频命令替身，覆盖准备、双时钟、明确提交、早期/排队取消、
命令拒绝与过期身份，不是 Player 界面/硬件音画验收。

两端音频宿主增量：AudioPanel / MusicPlayback 使用原子 LoadSoundtrack，作者增益
独立于用户主音量；EOF 重播通过既有来源 Seek，保留增益/循环。消费确认后的
AdoptSoundtrack 只接管选择元数据，不重新打开/seek；Android 私有音乐副本根据
来源释放确认清理，不误把分析循环代次当来源释放。Frame 附带产生该帧 FFT/时间的
同一份完整音频快照。Windows `out/p3-audio-panel-transition-tests.log` 三项通过；
Android `out/p3-android-music-host-tests.log` 通过（SDL dummy）。实际宿主 API 检查
作者增益 0.5 → 0.25 的 RMS、重播/seek、用户主音量零不变、连续设备计数与暂停取消。
Android 独立测试首次缺少显式 media_types 链接，失败保留于
`out/p3-android-music-host-build.log`，补齐依赖后构建见 linked-build 版本。

直接选中纯视觉作品时新增显式 AnchorMedia：保留既有音乐，并让新作品从自己的
零点运行；严格同位置的 AdoptMedia 合同不变。Windows
`out/p3-shared-music-anchor-tests.log` 两项通过，覆盖音乐 43 秒时选择新画面、随后
音乐 43.2 秒对应画面 0.2 秒。正式主循环连接与当前交付验证继续进行。

两端主循环已连接协调层，音频消费确认后只接管作品与配乐元数据；纯视觉作品
继续使用当前音乐。队列显示准备、恢复和具体失败；Android 记录状态变化/来源位置/
当前 SDL 音频驱动及场景提交，便于区分实际接管与保留旧输出。
Windows `out/p3-player-audio-windows-tests.log` 五项通过并部署 20 个 DLL 与资源。
新增真实 ImGui 加入/Go → AudioPanel/FilePlayback → SceneDeck → D3D 读回检查，
`out/p3-scene-audio-ui-tests.log` 三项通过，详细像素记录
`out/p3-scene-audio-ui-pixel-detail.log`：进度 0 为红 255/蓝 0，0.368667 为
红 161/蓝 94，1 为红 0/蓝 255；音频设备主音量零，不作听感结论。

真实内容审查发现整首峰值相加会错误拒绝内置三轨编排的短过渡。保留旧场完整
峰值余量，新场只预留过渡长度与一个解码块覆盖的前缀；未确认时每次读取仍检查
新场前缀，超限先释放新场、恢复旧场，不改变四个逻辑游标上限。持续到晚段的
真实超限仍在开始前拒绝，迟迟不确认则在解码将超限之前回退。
`out/p3-arrangement-window-tests.log` 五项、Android
`out/p3-arrangement-window-android-tests.log` 三项通过；后者播放宿主为 dummy，
SceneDeck 为 Null。`out/p3-arrangement-player-delivery-tests.log` 五项通过，额外
使用当前《光幕协奏》实际包，从旧场 9 秒三轨段落淡入新场开头，检查消费确认和
独立来源位置，修复后 Windows Player 已更新 deploy。Android 当前 APK 验收继续进行。

取消竞态：音频可能在 UI 取消手势与下一帧快照之间完成消费确认。现在先记录取消
意图，保留来场直到恢复开始；如果快照表明已经接管，则提交对应画面，避免旧画面
配新音乐。取消仍不能撤回已经听到的声音。Windows
`out/p3-late-audio-cancel-tests.log` 六项通过；最终宿主修订含先提交暂停意图再发起
配乐准备，Windows `out/p3-final-audio-host-windows-tests.log` 六项通过并完整部署，
Android `out/p3-final-audio-host-android-tests.log` 三项通过（桥接/场景为 Null，音频
宿主为 dummy），随后重建当前 APK 并覆盖安装。

正式 Android APK 短操作验收：新增 `tools/test-android-scene-audio.py`，复用既有
节目单 UI 重开检查，界面点击暂停状态下 Go、继续、真实音频过渡与画面接管；
证据 `out/android-scene-audio/4d19852384904cd6b6abc7fb97fc021f/`，
汇总 `out/p3-final-audio-apk-touch-tests.log`。驱动为 **AAudio**，消费计数
101376 → 111360 → 158976，旧源 3.312 秒、新源 1.008 秒时确认一秒过渡；
保存节目单字节保持不变，最后暂停。保留截图、状态日志和 AudioFlinger 信息；
这是生产音频设备/呈现估计，不是声学回录或听感、长稳、热稳定验收。APK SHA-256
`4909ac6ad1712c6714c63504220b51d02e4561705fe87d4a47c61d3b7998250f`。

常规四游标预算内的跨作品音画淡化已连接并交付两端。P3 剩余：分帧 GPU 准备/
创建峰值、量化边界与准备排队延迟、完整列表取消/失败/画布方向工作流，以及
超过双场准入时用户明确选择的零时长替换路径；当前零时长仍走双场保留准入，
不能宣称任意 4+4 编排都支持无缝硬切。P3 整体仍未关闭。


## P3.5 显式顺序硬切：音频增量（2026-09-09）

P3.4 已交付，见 [GPU 准备证据](scene_gpu_preparation.md)。本节更新上面的历史
零时长限制：音频流层与 FilePlayback 现支持在用户明确请求 0 秒时，以顺序替换
处理四路旧编排 → 四路新编排；非零过渡仍按共享四游标准入拒绝，绝不自动扩大
预算或偷偷把淡化改为硬切。复用现有 FFmpeg AudioStream、Mixer、FileBytes、
TransitionStream 的 RAII 游标和有界 PCM，不引入第二套播放器或新依赖。

顺序替换释放旧解码器，保留来源、下一段解码位置、未提交 PCM、gain/loop/iteration
等恢复信息，然后准备新源。旧来源在等待确认期间不继续解码或虚构旧 PCM 时钟；
准备失败或未确认取消后，在原音频工作线程按需重新打开旧源，从未提交位置继续。
恢复使用该工作线程的取消令牌，不使用已经取消的新场请求令牌。
已经消费的新音频不能撤回；重新打开/seek 可能产生短暂间隙，原来源必须仍可读。
不可将此路径宣传为任意 4+4 无缝淡化。GPU 双场容量不足时的顺序替换仍待实现。

Windows `out/p3-serial-cut-stream-windows-tests.log` 5 项通过：独立解码 PCM 对照、
失败/取消恢复、确认接管、seek 与峰值四游标；现有引擎/异步正常淡化回归通过。
Android `out/p3-serial-cut-stream-android-tests.log` 对应流/引擎/异步检查通过，
引擎使用原生 dummy 音频宿主，并非 APK 生产驱动验收。
新增 FilePlayback 四路对四路用例覆盖先拒绝非零淡化、暂停时准备零时长、取消、
再硬切、消费确认、不重建设备 epoch、后续 seek 保留新源增益。
Windows `out/p3-serial-cut-public-windows-tests.log` 及 Android
`out/p3-serial-cut-public-android-tests.log` 通过（Android 为 dummy）。
两端 Player 画面时钟、UI 提示与实际硬切交付尚待后续增量，不能把上述音频测试
记成完整应用硬切验收。


### 顺序硬切的画面时钟与应用验证

当串行替换只保留旧源恢复点、新 PCM 成为设备主来源而没有旧 PCM 呈现位置时，
SceneAudioClock 保持最后确认的旧画面时间；恢复到旧 PCM 后继续使用原来的画面
时间偏移。不会把新音乐零点误用到旧场。Windows
`out/p3-serial-cut-clock-windows-tests.log` 与 Android
`out/p3-serial-cut-clock-android-tests.log` 的时钟、场景和桥接检查通过。

Windows 新增 scene_audio_serial_ui：从含四路配乐的红色旧场，用实际队列 Go
运行显式 0 秒节目单条目，切入四路蓝色新场；使用实际 AudioPanel/FilePlayback
设备消费快照，读回红→蓝像素，确认接管后不重新加载媒体/设备 epoch。
`out/p3-serial-cut-player-windows-tests.log` 共 6 项通过，包含常规淡化、实际《光幕
协奏》编排与 Player deploy smoke；详情
`out/p3-serial-cut-player-windows-detail.log`。Windows deploy 已更新。

Android 的 `--hard-cut-head` 短测选中节目单第一行、通过滑块将草稿时长设为零、
应用草稿但不保存，然后预备队列和实际 Go；原保存列表逐字节保持不变。
`out/android-scene-audio/0b76fd13c358400da95f537daf2d237f/` 与
`out/android-program-ui/775149a2c57d434bae875231e50ed2da/` 通过，使用生产 AAudio，
确认 elapsed=0 的零时长接管后暂停。此 APK 用例是内置《光幕协奏》，不是四路对
四路压力编排；四路预算的 Android 证据是前述 native/dummy 检查，Windows 有
实际 UI/设备/像素检查，不混淆三者。
当前覆盖安装 APK SHA256 `33e7842d159f91f784de56beb609bf52ee2381f5148f3f413acd9ff2cb1fe357`。

两端遇到 audio.transition_cursor_budget 会提示可将过渡时长设为 0，并说明可能
有短暂间隙。GPU 双场不足的顺序替换、失败队列重试闭环与完整连续演出还在 P3.5。
