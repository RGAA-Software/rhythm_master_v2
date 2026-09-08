# R6 素材维护短功能验证

Windows Release / Android arm64 Release，沿用增量缓存和 20 workers。

| 检查 | 结果与范围 |
| --- | --- |
| asset_commands | 主图/未实例化组件/配乐与片段引用、替换去重、媒体族拒绝、删除保护、undo/redo |
| asset_contracts | 缺失/损坏状态、错误原文件不覆盖、取消、正确恢复、完好副本不重写、单工作线程复用与检查预算 |
| persistence_contracts | 严格加载拒绝损坏内容，修复模式保留原工程，修复前保存/发布拒绝，恢复后严格重开 |
| asset_ui_en-US / zh-CN | 实际 ImGui 输入、引用删除保护、全部替换、后台检查和恢复；此层字节夹具不充当图片解码证明 |
| soundtrack_ui_en-US / zh-CN | 不可用素材阻止自动加载与手动加载按钮，解除状态后重新加载；保留原编排操作回归 |
| asset_recovery_gpu | 真实 PNG 与中文路径；从损坏工程进入 Studio，经素材面板修复，重新 GPU 预览、保存、发布和 Player 非黑像素读回 |
| Android 原生共享核心 | asset_commands / asset_contracts 在 USB 设备通过；移动端仍为 Player，不声称新增 Android 素材编辑器 |

本地日志：`out/r6-asset-android-core-tests.log`、`out/r6-assets-delivery-tests.log`。
最终 Windows 聚焦 CTest 含音频夹具共 9 项全部通过；两个 deploy 均同步完整 DLL 与资源。
Android APK 已 `adb install -r` 覆盖安装，保留选中的光幕协奏并正常播放，
截图 `out/r6-assets-phone.png` 显示非零 RMS 和运行时间；本次没有卸载或清理数据。
GPU 测试保留独立工程、运行包及截图于 `out/windows-release/asset-recovery-gpu/`。

素材警告根据当前工程记录持续显示，编译成功不会掩盖仍未修复的闲置素材。
检查结果与原文件恢复均由已有单工作线程发布值结果；跨工程切换取消并丢弃迟到结果。
修复不是撤销命令，图引用替换是撤销命令；旧内容副本保留供历史恢复。

本批不将内容品质、R6 整体或长稳验收标记为完成。后续继续组件浏览、帮助、
预览调度与 40 语义组件、120 独立视觉预设、50 基础 + 50 高级作品生产。
