# 离线音画渲染验证（2026-09-08）

`export_core::RenderExport` 已把同一图 Runtime、准备资产、真实 PCM 特征、
按需视频解码、有界异步 GPU 读回与 FFmpeg MP4 编码接通。它供独立离线 host/worker
调用，不在 Studio 实时呈现线程中运行。随后已接入 UI、后台进程管理和最终文件发布，
见 [Studio 导出验收](studio_export_2026-09-08.md)。下方保留核心阶段的验证记录。

## 时间、容量与复用

每个输出时刻为 `frame / fps`，只求值一次；推进 GPU 完成所需的空帧不推进音乐或模拟。
分析只使用该时刻之前的源 PCM；与 Player 一样，输出音量不改变源音乐的特征值。
每帧 PCM 与读回票据配对，按原顺序编码，GPU 票据为三帧上限。后续专属编码线程
另有两帧队列和一帧正在编码的容量；完整边界见 Studio 导出验收。音乐按需读取，不能先解码整首。
曲目结束后补静音，动画持续到请求结束；这一步没有新增循环编排/多轨混音。
视频沿用现有每节点速度、偏移和循环规则，通过新增 `Playback::Resolve` 等待准确请求。
常规 `Request/Snapshot/Streams::Update` 仍不等待，离线接口有取消与请求被替换的诊断。

复用本项目视频 worker 和运行时，无新增第三方源码或解码/播放器后端。
导出 staging 文件由调用者管理；返回成功才允许上层发布。图/编码错误和取消均抛出，
取消时挂起 GPU 存储由既有 Readback 后端保留至完成或设备退出。

## 实测

`out/export-core-final-tests.log` 共 8 项通过，包含视频 worker、视频 D3D11、
导出 D3D11、源码边界、Studio 音乐启动和 Python 部署检查及媒体 fixture。

使用 Resonance Live（197 个可达指令）、真实解码 demo PCM，输出 640x360 /30fps /4秒：

- 两次 MPEG-4/AAC 导出的 120 帧逐帧解码像素哈希完全一致；超过 50 帧互不相同。
- 静音版本与音乐版本的画面不同，最后一帧亮度检查与人工图像检查通过。
- 音轨恰为 192000 个采样帧，与源音乐的 AAC 回读均方误差小于 0.002。
- progress 单调、共享 256 MiB 预算有效、成功返回后 graph/staging 纹理全部释放。
- 第 10 帧请求取消后准确抛出取消错误，挂起资源随离线设备安全退出。
- 独立 ffprobe 确认视频和音频均为 4.000000 秒、30/1 fps、120 视频帧。

首轮实际文件 `out/windows-release/export-gpu/463553303916700/music.mp4`，
查看帧 `review.png`；后续测试生成独立目录保留证据。
上述一致性是同一 Windows/D3D11 环境的短片测试，不是跨 GPU 逐像素保证。

Android：export_core 编译通过；`out/video-resolve-android-tests.log` 记录手机原生
VFR/B 帧 drain、独立媒体、seek、循环和离线 Resolve 测试通过。未据此声称 Android
应用内导出或生命周期已验收；Android 当前产品重点仍为 Player。
