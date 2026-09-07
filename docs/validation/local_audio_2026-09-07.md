# 本地音频驱动：首个应用增量

通信开发已停止。本轮只推进本地创作/播放：音频分析、节点输入和 Windows 系统声音。

## 当前可用行为

- Studio 检查器增加“音频输入”，用户点击后才监听默认播放设备；显示响度和 63 带频谱。
- 新增 `audio.feature` / `audio.band`，分别读取响度/RMS/起音/BPM/重心与指定声道频带。
- “音乐渐变”模板将响度经范围映射连接到渐变，保存/发布使用同一图与运行包格式。
- 独立 Windows Player 可显式启用系统声音，播放发布的音频响应图；不依赖 Studio。
- 窗口挂起时释放采集，恢复后按用户原选择重开；没有新数据超过 250 ms 清除旧响度。
- 默认不打开采集，不保存 PCM，不上传声音。

## 实现和来源

`src/audio_analysis` 迁移旧项目自有频带映射、起音、BPM 与规范频谱归一化行为，
来源提交 `118dbc811718836ffb4ca62eec7384605df5be32`。FFT 参考链中的 MilkdropFFT
文件带 Nullsoft BSD-3-Clause 许可，完整保留于源码及 `third_party/notices/audio_fft`，
Windows/Android 打包脚本会携带通知。清单为 `provenance/audio_analysis.json`。
projectM 衍生的相对响度包络和波形相位对齐未导入；不能将当前字段假称为那两个算法。

单个分析器处理规范 PCM 并发布值帧。4096 点周期 Hann 窗、63 个 20 Hz–16 kHz
对数频带、RMS/峰值混合聚合与有界增益沿用已有规范；低采样率的 Nyquist 以上输出零。
分析按采样数推进，约 60 Hz，采样块分割不改变结果；非整除采样率按整数 hop 计算实际 dt。
起音峰值确认修正旧代码历史索引差一帧的问题，事件保留其峰值采样时间。
FFT 重用预分配工作区，移除每次变换的临时复数数组。

系统声音使用私有 WASAPI shared loopback，音频引擎转换为 48 kHz / float / stereo。
原生缓冲在同一个同步读取作用域复制后释放，再进入分析；COM、设备和线程均 RAII。
实现参考 [Microsoft loopback 文档](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording)。
当前设备改变/失效会报告失败并由用户重开，尚无自动热插拔恢复。

## 已执行验证

- Windows / USB Android：FFT 对独立直接 DFT；44.1/48 kHz 的 110/1000/8000 Hz
  左声道信号、两档幅度、右声道隔离、分块不变性、静音、非法 PCM、seek generation、
  样本时钟、单帧起音确认和 120 BPM 合成起音序列。
- 运行时：缺失音频归零、指定频带/响度消费、无关频带不触发重算、非法帧拒绝。
- 本机 WASAPI：默认播放设备打开、三次启停、幂等停止、启动中析构；未保存 PCM。
  这只是设备生命周期检查，不是完整真实音乐听感或音画同步验收。
- 完整 Windows 构建通过 39 个测试组，含部署目录启动；Android 通过 31 个 native 组。
  设备目录 `/data/local/tmp/rhythm-master-phase-a-20260907004750211`。

证据：`out/audio-analysis-validation.json`、`out/audio-capture-device.log`、
`out/audio-studio-build.log`、`out/audio-android-device.log`。

仍待完成：FFmpeg 文件音乐播放、暂停/seek/loop 和设备输出时钟；麦克风与 Android
音频宿主/权限；完整音频包络/波形；真实音乐素材测试；真实 APK 验收。
当前音频模板在无来源的 Player 中显示静态底色，不假称 Android 已具备系统采集能力。

## 频谱视觉增量

`texture.spectrum` 支持直线和环形、8–256 柱、声道选择、增益、间距和双色渐变。
所有柱形合并为一个绘制命令，使用统一 63 带分析数据，不重复 FFT。
透明纹理可叠加到其他画面；节点预览复用纹理。“霓虹频谱环”模板使用此路径。
当前共 24 个预设、5 个模板，尚未达到最终内容数量要求。

Windows 完整 40 组测试通过；Android 完整 32 组 native 测试通过，包括
GLES 实际像素检查：两种布局可见、透明区域保留绿色底图、声音停止后旧柱形清除。
每个图形场景跨两次设备生命周期执行。几何检查覆盖 8/63/256 柱、边界、索引和静音。
这是合成频带测试，不替代真实音乐或 APK 交互验收。

证据：`out/audio-spectrum-build-windows.log`、`out/audio-spectrum-android-device.log`。
真机目录 `/data/local/tmp/rhythm-master-phase-a-20260907005925109`。
