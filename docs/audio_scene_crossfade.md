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
