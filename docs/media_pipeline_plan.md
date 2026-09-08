# FFmpeg 统一媒体方案

> 2026-09-06：用户已确认 FFmpeg 为新项目唯一媒体后端。
> 2026-09-08：Windows 音视频播放与 Studio MP4 音画导出已接通，Android 原生
> 解码/编码/读回已验证。应用安装、生命周期、媒体编排及其他未验证能力仍按专项记录推进。
> 见 [Studio 导出验收](validation/studio_export_2026-09-08.md) 与
> [Android 音乐应用记录](validation/android_music_application_2026-09-08.md)。

2026-09-08 作品交付补充：音乐资产、音量与循环设置已接入工程保存及运行包，
共享 FFmpeg 解码支持包内不可变字节。Windows Studio 按钮和 Android 原生音乐/GLES
验证见 [作品音乐记录](validation/work_soundtrack_2026-09-08.md)。当前为单曲、有界资产
规格；多轨编排及实际 APK 生命周期继续推进。

大音乐容器前置验证已加入共享文件切片和同一 FFmpeg 自定义 I/O，
见 [文件音频范围验证](validation/file_audio_ranges_2026-09-08.md)。后续已完成
`music-performance-v2`：单首音乐最多 256 MiB，普通素材仍共用 8 MiB，归档最多
272 MiB。Studio 绑定、保存、发布、重开及 Windows/Android 原生播放验证见
[大音乐运行包记录](validation/large_music_packages_2026-09-08.md)。沿用同一 FFmpeg
解码器和 miniz，音频工作线程持有文件范围；不把整首音乐读入内存。

## 1. 决策和范围

媒体能力服务于[音乐可视化产品主线](product_scope.md)：音乐文件/实时输入、
音频分析与视觉参数驱动、音乐和动画同步控制，以及作品音画导出。图像与视频是
可组合的作品素材；媒体设计不承担桌面背景或系统壁纸管理。

2026-09-07 补充：FFmpeg 使用 `C:/source/vcpkg` 提供的目标平台库，
不再自行维护 configure/make 源码构建。已停止 8.1.2 自建路线；
当前安装版本为 Windows 6.1.1、Android 6.1，分别验证其能力与配置后接入。
包管理来源不替代许可证和实际构建特性的核验。

Studio（Windows/macOS）与 Player（Windows/macOS/Android/iOS）共用一套媒体抽象，
由 FFmpeg 实现解封装、解码、媒体转换以及需要时的编码/封装。
不迁移 VLC/libVLC、Qt Multimedia，不引入第二套高级播放器兜底。
本决策不删除旧仓库代码，不改变其他尚未冻结的库选型。

FFmpeg 不是整个播放器：播放状态、媒体时钟、seek/loop、设备切换、渲染提交和
权限由项目相应模块负责。音频设备层不是第二套媒体解码/播放引擎。

## 2. FFmpeg 库的职责

| 库 | 项目用途 |
| --- | --- |
| libavformat | 媒体探测、解封装；导出时封装音视频 |
| libavcodec | 音视频解码；导出时按 profile 编码 |
| libavutil | 时间基、帧/缓冲等底层支撑，类型限制在 adapter 内 |
| libswresample | PCM 格式、声道和采样率转换 |
| libswscale | CPU 像素格式/尺寸转换与兼容路径，不强制每帧 CPU 转 RGBA |
| libavfilter | 可选媒体过滤需求，默认不引入另一套音乐可视化 filter 语义 |
| libavdevice | 仅明确设备需求经验证后启用，不假设它解决四平台系统声音捕获 |

直接链接受控版本的库。正常播放不调用 ffplay、不嵌入外部播放器窗口、
不通过启动 ffmpeg 子进程并解析控制台维持播放；命令行工具可用于独立测试。

## 3. 数据流与设备边界

```text
媒体资产 -> FFmpeg 解封装/解码 -> 带时间戳的 PCM -> 有界输出队列 -> 音频设备
                                      |
                                      +-> 唯一音频特征分析 -> 节点 / 集群输入

麦克风/系统声音 -> 平台采集 -> PCM 规范化 -> 同一个音频特征分析器

媒体资产 -> FFmpeg 视频帧 -> 受控上传/硬件帧桥接 -> RhythmRender 纹理 -> 场景
```

设备输出/采集实现尚未冻结，可以选择 SDL 音频或原生适配，但只能有明确的一套
设备管理策略。不默认新增 miniaudio 解码器或其高级播放引擎；若以后选择它作为
纯设备 adapter，需单独确认范围，不改变 FFmpeg 唯一媒体后端的决定。
Windows 系统回环采集可复用审查过的 WASAPI 代码；其他平台分别验证可用 API、
权限、系统版本和后台策略，不承诺 iOS 任意捕获其他应用声音。

复用现有独立音频分析模块。重采样和声道策略显式配置、避免重复转换；
分析帧标记源媒体时间与分析延迟，不能把解码完成时间当作实际听到的时间。

## 4. 播放时钟和并发

- 统一项目播放协调器负责状态和时间；每个媒体实例可有自己的 source/local time，
  不能用一个全局可变媒体游标控制所有视频节点。
- 有音频输出时依据实际消费样本和设备延迟估计主时钟；无音频时使用单调时钟。
  离线导出使用确定性时间，集群模式接入会话时钟，不各自重建不同同步机制。
- seek 使用 generation 并清理旧队列，过期回调不能重新填入旧帧；正常 drain 与
  立即取消区分处理。整曲循环提前用同一 FFmpeg 解码器准备下一轮，保留同一个
  输出流及其重采样连续性；播放消费到边界才切换分析/时间代次，不在正常循环处
  flush 或重建输出设备。停止循环时已提交的音频按有界队列约定播放完当前轮次。
- 正确处理时间基、可变帧率、解码重排、EOF drain、无效时间戳和音频首尾 padding。
- 解封装/解码/转换在后台执行；队列按字节/时长限额，不阻塞 UI、渲染或音频回调。
- 设备实时回调只消费预备 PCM；不读文件、不解码、不写日志、不做网络操作。
- 设备丢失、暂停/恢复、蓝牙切换和移动端中断均进入同一明确的状态机。

## 5. 视频与 GPU

公共接口使用项目定义的帧描述、平面布局、时间戳、颜色元数据和受控所有权。
不向图、UI 或 render 公共接口暴露 AVFrame、AVPacket 或平台解码器指针。
FFmpeg native 对象由带正确释放器的 RAII 对象管理，遵循初始化和智能所有权规则。

优先验证软件解码的正确结果及有界上传，再逐平台验证 FFmpeg 硬件解码与 bgfx
互操作。硬件解码成功不等于零拷贝；必须检查设备一致性、同步、帧寿命和格式。
硬件路径不可用时回退同一 FFmpeg 软件后端，并报告成本/限制，不回退 VLC。

保持色彩范围/矩阵/transfer、alpha、旋转、像素宽高比和 PTS 语义；
节点预览与最终输出复用 GPU 纹理，不为每个预览读回或重新解码。
导出通过独立有界队列将渲染结果送入编码/封装服务，不阻塞实时播放。

## 6. 模块拆分与分发

按媒体源/解封装、解码会话、PCM 转换、视频帧桥接、播放时间协调、导出分别组织。
协调器组合服务，不把 UI、设备、codec、资源下载和渲染都放进巨型 MediaPlayer。
FFmpeg include/link 仅进入媒体实现目标；Player 不默认携带所有导出编码器和编译工具。
网络媒体协议按白名单启用，包内资源访问受资产/路径策略限制，不能任意访问 URL。

固定版本、配置、codec/profile 和源码来源，四平台分别记录二进制依赖与测试。
开源项目可评估 GPL 组件，但依然按实际组合许可执行；不使用无法再分发的
nonfree 构建作为发布产物，也不把采用 FFmpeg 等同于拥有任意 codec 的分发权利。
依照 `third_party_reuse_policy.md` 处理来源、通知和构建材料，项目总许可仍待选择。

## 7. 实施与验收顺序

1. 固定首轮格式/profile，建立最小 FFmpeg wrapper target 和 RAII/取消测试。
2. Windows 音频解码 -> 输出 -> 特征分析闭环；比较原分析器结果，不改频谱语义。
3. Windows 视频解码 -> 纹理 -> 多节点预览；验证时钟、画面、颜色、比例和寻址。
4. seek/loop/多源/损坏文件/长时运行/设备切换测试，确认队列内存有界。
5. 编码导出、硬件加速互操作和失败降级；性能用实际测量而非“FFmpeg 很快”验收。
6. Windows 门通过后，先完成 Android 设备/权限/生命周期验收；Apple 平台最后实施。

2026-09-08：音乐消费时钟已接通 Studio/Player 的动画、视频请求和 seek/loop
状态重置，见 [共同播放时间验证](validation/music_transport_2026-09-08.md)。
Android APK 已接入相同 vcpkg FFmpeg 播放核心，实际应用验收与原生测试分别
记录在 [Android 音乐应用验证](validation/android_music_application_2026-09-08.md)。

验收包含低音量和不同乐器、不同采样率/声道、VFR 视频、快速连续 seek、
多路同时播放、软硬解画面一致性、导出音画同步及异常包输入。
版本/profile、时间漂移、掉帧、队列高水位和软硬解路径进入节流诊断，不记录原始音频。

## 8. 官方资料

R5 实施增量见 [视频片段源区间与时间线](media_clip_arrangement.md)：四路视频图合成、
trim/摆放/淡入淡出与严格源出点已通过两端功能检查；多轨音频混合继续开发。

- [libavformat](https://ffmpeg.org/libavformat.html)
- [libavcodec](https://ffmpeg.org/libavcodec.html)
- [libswresample](https://ffmpeg.org/libswresample.html)
- [libswscale](https://ffmpeg.org/libswscale.html)
- [FFmpeg 许可与分发说明](https://ffmpeg.org/legal.html)
