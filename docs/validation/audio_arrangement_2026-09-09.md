# R5 多轨媒体检查（2026-09-09）

## 已通过

- Windows 20 worker 增量编译，Studio/Player 自动部署完整资源与 20 个 DLL。
- `audio_arrangement`：源区间、采样对齐、半开区间、四路重叠预算、非法值拒绝。
- `audio_mixer`：实际 FFmpeg PCM、错开/循环/淡入淡出、左右声道、静音、混合后统一限幅、
  精确 seek、文件和不可变字节来源、取消。
- `audio_playback` / `arrangement_host`：设备输出路径、混音特征、暂停定位、队列边界、
  消费时钟、整体循环、结束与重播。
- `music_authoring` / `soundtrack_contracts` / `soundtrack_player`：不同源导入、单曲转换、
  保存/发布/重开、旧单曲兼容、错误 profile 和超预算拒绝、包替换后的源存活。
- `audio_clip_ui`、`soundtrack_ui` 中英文：拖动/吸附/缩放、单次撤销、第五路重叠拒绝、
  删除最后一条不恢复旧单曲、追加/载入/清除与撤销，导出窗口无文件时仍启用编排音频。
  原时间片段和时间线交互回归通过。
- 更新目录测试，使它通过真实资产与 Player 准备路径验证所有 40 个示例。
  原测试仅打包裸图，遇到已有 GLB 示例会报 `package.asset_missing`，已修正测试入口。
- `export_soundtrack` 对比混合 PCM 和规范 FFT；Studio 真实导出按钮生成 16 秒 / 480 帧
  H.264 MP4，音轨解码时长及非零能量通过。导出期间父 UI 220 帧，p50 16.44 ms、
  p95 16.79 ms（本次短检查，不能替代跨设备性能承诺）。
- 固定视频帧后，D3D11 同时刻真实 PCM 图像差异均值（0–255）：演示/静音 0.1988，
  低频/静音 1.0072，高频/静音 1.6496，低频/高频 2.5772。每种场景 GPU 纹理
  38,278,660 字节，短循环内稳定。Windows Player 加载编排包、音频特征、30 GPU 帧通过。
- Android ARM64 原生 `audio_arrangement`、`audio_mixer`、`soundtrack_contracts`、
  `soundtrack_player` 通过。独立命令行设备输出测试未通过；不能拿核心 PCM 测试代替
  Android 应用设备输出。APK 实际应用检查单独记录于下。

## 交付与设备证据

Windows Studio：`out/windows-release/src/windows_spike/deploy/rhythm_master.exe`。
Windows Player：`out/windows-release/src/windows_player/deploy/rhythm_player.exe`。
APK：`out/android-arm64-release/apk/rhythm-player-release.apk`。

已用 `adb install -r` 在 USB Redmi K40S / Android 14 / Adreno 650 覆盖安装，保留数据。
内置目录显示“光幕协奏”，实际选择后有 16 秒音频时长及非零 RMS，GLES 画面更新。
检查中发现循环勾选框未反映包内设置，已修复原生状态同步并再次覆盖安装。
暂停定位在 7.95358 秒保持，双视频交叠画面正确；关闭循环后停在 16.00 秒；
重新播放及恢复循环后回到片段开头并继续产生 RMS（截图 0.146895）。
作品切换也保留暂停意图。独立 CLI 输出环境的问题未据此声称已修复。

本地日志：`out/r5-audio-*.log`；MP4 在 `out/luminous-concerto-export`，
同刻图像在 `out/luminous-concerto-music`，手机截图在 `out/r5-audio-phone-*.png`。
这些短检查不构成长稳、热稳定或最终视觉品质验收。
