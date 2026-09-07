# 音画编码后端验证（2026-09-08）

## 范围与状态

已实现 `media_av_writer`：FFmpeg 编码/MP4 封装、RGBA8 SDR Rec.709 转换、
48 kHz 双声道 AAC、取消、显式完成，以及独立 staging 文件 I/O。
只使用现有隔离 vcpkg LGPL SDK，没有重新编译 FFmpeg 或升级共享依赖。
这一步是导出后端；Studio 导出界面、离线 GPU 渲染和最终文件发布尚未接通。

输入内存有界：音频单块最多 4096 个双声道采样帧，编码缓存最多一个 AAC 帧；
音视频提交领先不能超过一秒。支持 24/25/30/60 fps、偶数尺寸且最多 1080p 像素数、
最长一小时。调用者在工作线程串行提交，完成失败或取消后不得继续写入。
析构不隐式写 trailer，不把不完整文件当成成功导出；最终发布由上层负责。

## 发现并修复

- MP4 默认 1 kHz movie timescale 会把单帧 60 fps 的音乐长度取整为 768 个采样。
  封装时使用 48 kHz movie timescale，所有支持的帧率与音乐采样均能准确表达。
- FFmpeg 6.1 的 AAC 解码会返回完整末帧，包括 MP4 最后 packet duration 排除的填充。
  AudioDecoder 只对 MOV/AAC 的准确末包区间裁去填充，并设置 packet time base；
  不按文件级估计时长截断一般音频，也不改 WAV/FLAC/MP3 的解码规则。
- H264/MF 使用 camera_record 场景以请求恒定帧率。当前验证是 Windows 软件 MFT，
  不能据此声称硬件编码、零拷贝或 Android H264 已支持。

## 结果与证据

Windows `out/media-writer-tests.log`：source_boundaries、media_av_writer 通过。
MPEG-4/AAC 与 H264/AAC 的 75 帧回读保留所有帧，PTS、上下方向、颜色断言通过；
2.5 秒音乐恰为 120000 个采样帧，左右声道相位/频率回读均方误差约 0.00000574。
额外检查 60 fps 单帧（800 samples）、24 fps/29 帧（58000）、25 fps/31 帧（59520）、
AAC 对齐长度（102400），均准确。准确 seek 到最后一个采样/EOF，EOF 后 seek 拒绝。
已有 media_audio_tests 的 PCM、44.1/48 kHz、FLAC、重采样 drain/seek/取消回归通过。
还覆盖无音轨、拒绝已有文件、取消、非法尺寸、提交过量、失配时长与失败后禁止继续写。

USB `e2b3b128` 原生测试 `out/media-writer-android-tests.log`：
media_audio_tests 与 MPEG-4/AAC 的同一组回读测试通过。
证据目录 `/data/local/tmp/rhythm-encoding-6d2810e3d1314d74a6b357e8f4cf34e3`。
这是原生媒体契约测试，不是 APK 安装/音频焦点/生命周期或长时运行验收。

复用 FFmpeg n6.1.1 MIT 编码示例的发送/排空/时间基顺序，保留 Fabrice Bellard 通知；
精确文件哈希、改动与研究范围见 `provenance/media_encoding.json`。
