# 时间线逐片段源波形

2026-09-09，R5 创作体验补全。在 Studio 打开含多轨配乐的作品并展开时间线，
每条音频片段显示自己的源包络。同一音源的多个片段共享扫描结果；移动、裁剪、
复制和循环不重新解码。普通单文件配乐的完整波形与点击跳转保留。

## 显示与资源合同

- 复用现有 FFmpeg `WaveformScanner`、不可变资产库及 `ClipInterval`。素材完整性
  检查和解码在已有工作线程模型中进行，没有引入播放器、设备服务或第二个播放时钟。
- 最多 32 个源，每源最多 4096 个峰值区间。不可变区间索引约 64 KiB/源，
  32 份约 2 MiB（不含容器、资产映射和临时扫描开销）。不缓存完整解码 PCM。
- 缓存只持有一个临时扫描器；完成或取消并收到结果后释放工作线程。完成后的缓存
  不占后台线程池名额。扫描沿用 256 MiB 输入和 120 秒工作上限。
- 只绘制可见片段行，每行最多 1024 列。每列使用对数复杂度的峰值查询；跨越多次
  源循环时查询整个裁剪区间一次，不按循环次数重复遍历。
- 包络保留双声道正负峰值，避免反相抵消。源入出点、摆放和循环映射遵循片段合同；
  以显示列中点的增益/淡入淡出近似缩放，静音使用弱化颜色。它不是逐采样精度编辑器，
  也不代表经过左右平衡、叠加及限幅后的最终混音 PCM。
- 关闭时间线或切换工程清除需求并取消扫描。已取消的结果不能重新填充缓存，也不能
  将随后重新打开的同源请求标成失败。加载失败可显式重试。

本地参考 TiXL 音频片段绘制及波形处理代码，具体文件摘要与 MIT 来源见
`provenance/clip_waveforms.json`。没有复制第三方源码；其 BASS/SharpDX 依赖不符合
本项目 FFmpeg 与平台边界，实际复用本项目已有模块。

## 验证路径

1. `waveform`：128 秒真实 PCM 文件全量解码，逐采样检查包络；验证源裁剪、空白
   尾段、循环跨界、多次循环、资产元数据错误、重复音源共享、缓存清理和工作线程释放。
   增加“开始扫描→清除→立即重新请求同一源”的测试，修复前明确失败；取消状态隔离后通过。
2. 中英文 `audio_clip_ui`：先比较无波形与有波形的实际绘制数据，随后在显示波形时
   执行鼠标移动、吸附、改长、撤销、复制、重叠预算拒绝、移除和撤销恢复。
   单文件 `waveform_ui` 的扫描、取消、切源和跳转检查保留。
3. 中英文真实 `template_switch_gpu`：通过 Studio 模板窗口依次应用墨潮、织光机、
   晶瓣合唱，检查当前编译计划、保存与发布，并展开时间线。前两者的 4 条片段共用
   2 个已扫描源；切换到单曲配乐后，逐片段缓存计数回到 0。
   实际查看中文截图，墨潮最终输出为墨绘效果，片段波形可见。
4. Android arm64 以现有 NDK/vcpkg 增量构建共享模块测试，在 USB 设备 `e2b3b128`
   上执行相同 128 秒文件、映射、缓存和取消检查，通过。这不是 Android 时间线 UI；
   Player 没有因此增加编辑器或重装 APK。

短测试日志：`out/r5-clip-waveform-final-tests.log`、
`out/r5-clip-waveform-reopen-red.log`、`out/r5-clip-waveform-drag-tests.log`、
`out/r5-clip-waveform-ui-tests.log`、`out/r5-clip-waveform-studio-tests.log`、
`out/r5-clip-waveform-android-build.log`、`out/r5-clip-waveform-android-tests.log`。
已查看截图：`out/windows-release/template-switch-gpu/568547217523800/zh-CN-ink_tide.png`。

Windows Studio/Player Python 部署完成，各含 20 个 DLL。最终交付强制模板回归 4 项
通过，日志 `out/r5-clip-waveform-final-delivery.log`；界面及部署检查 7 项通过，见
`out/r5-clip-waveform-delivered-ui-tests.log`（其中一项为音乐文件准备）。

额外导出检查曾与部署 GPU 冒烟同时运行，首次报 `export UI did not complete`，
见 `out/r5-clip-waveform-export.log`。不能据此宣称首次通过或已定位该次失败根因。
随后独立运行真实 Studio 导出，完成 480 帧 H.264，逐帧时间戳与 16 秒非静音音轨
解码检查通过；日志 `out/r5-clip-waveform-export-recheck.log`，成品
`out/r5-clip-waveform-export-recheck/569365761371400/Exports/音画验收.mp4`。
GPU 界面/导出检查应串行执行，包括从不同命令启动时，避免相互干扰与误读验收结果。

本增量不改变包格式、音频混音或设备时钟，不据此标记全部 R5/R6、视觉品质或长稳验收完成。
