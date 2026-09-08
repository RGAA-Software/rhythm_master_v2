# R5 场景队列与转场短功能验证

接续 `203a243`，Windows Release / Android arm64 Release，增量构建 20 workers。
沿用项目已验证的 bgfx、FFmpeg 与设备适配，不添加网络、音频设备或第二个播放时钟。

## 证据

| 检查 | 结果与范围 |
| --- | --- |
| Windows 9 项聚焦 CTest | 全部通过：scene_deck、scene_queue、control_player、package_import_contracts、package_loader_contracts、player_contracts、scene_compositor_gpu、scene_queue_ui、windows_player_smoke |
| Windows 真实 UI | 激活队列窗口、内置入队、未就绪 Go 禁用、准备完成后 Go 与 GPU 接管通过 |
| 两端 GPU 溶解像素 | 端点/中点、预乘 RGBA、横竖画布等比适配、单目标复用与释放通过 |
| 两端复杂作品 | Crystal Choir → Luminous Concerto，100 帧、Balanced；峰值纹理 53,890,184 B、28 pass；转场中段/接管后非黑像素与资源释放通过 |
| Android 原生导入契约 | 私有缓存边界、取消中保留加载源、迟到结果丢弃、接管后清理副本、预备不覆盖持久选中包通过 |
| Android 中文入队 | 修复 JNI `jbyte` 迭代器的 UTF-8 JSON 解析；转成标准字节字符串再解析，中文标题在队列正常显示 |
| Android 实际转场 | 内置目录选择晶瓣合唱、就绪、转场 45%、成功接管；保留原有 16 秒音乐，场景局部时间独立显示 |
| Android 配乐接管 | 反向入队光幕协奏，接管后音乐约 1.89 s，场景最近一次状态约 1.73 s；两种 UI 快照刷新频率不同。画面和非零 RMS 正常 |
| Android 暂停/取消 | 暂停中开始转场保持 0%；恢复再暂停后连续检查保持 32%；取消返回原场景 |

纯核心测试另外覆盖暂停、seek/循环 generation 不连续、切换后局部时间、公开宏隔离、
新配乐接管定位、纹理预算不足保留当前场和释放压力后重试。
预算拒绝用 Null 后端资源表做可重复注入；不是手机真实内存耗尽实验。
复杂 GPU 探针使用确定性音频特征；实际设备配乐检查使用 APK 内置音乐及原音频设备时钟。

本地日志：

- `out/r5-scene-delivery-tests.log`
- `out/r5-scene-complex-windows-tests.log`
- `out/r5-scene-complex-android-tests.log`
- `out/r5-scene-deck-android-tests.log`
- `out/r5-scene-android-native-final.log`
- `out/r5-scene-delivery-build.log`
- `out/r5-scene-android-delivery-build.log`

本地实际设备截图：

- `out/r5-scene-phone-ready.png`
- `out/r5-scene-phone-transition.png`
- `out/r5-scene-phone-takeover.png`
- `out/r5-scene-phone-music-handoff.png`
- `out/r5-scene-phone-paused-a.png` / `paused-b.png`
- `out/r5-scene-phone-paused-mid.png` / `paused-mid-b.png`
- `out/r5-scene-phone-cancelled.png`
- `out/r5-scene-phone-final-ui.png` / `out/r5-scene-phone-final-takeover.png`

## 交付与限制

Windows 完整部署：

- `out/windows-release/src/windows_spike/deploy/rhythm_master.exe`
- `out/windows-release/src/windows_player/deploy/rhythm_player.exe`

Python 构建脚本继续自动复制 exe、20 个运行 DLL 和资源到各自 deploy 目录。
Android APK 为 `out/android-arm64-release/apk/rhythm-player-release.apk`，USB `adb install -r`
覆盖安装，未卸载、未清空应用数据。设备 Redmi K40S / Android 14 / Adreno 650。

横竖画布合成经过两端 GPU 像素检查，接管发布方向沿用已有宿主路径；本次复杂作品的
手机实操均为横屏，未把它写成新增横转竖实机验收。
队列不持久化，预备只覆盖 CPU/媒体资源，GPU 首次准备仍可能产生瞬时开销。
本批没有跨作品音频交叉淡化、任意转场 shader、HDR 输出或通用图事件分发。
没有执行最终长稳、热稳定或全作品品质验收，也没有据此承诺帧率或软件能力对等。
