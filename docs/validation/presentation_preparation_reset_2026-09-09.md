# 横竖屏变化中断下一场 GPU 准备

发现于 P3.5 Android 实际连续节目单检查。来源不是用户报告，是本次集成检查。

## 实际问题

光幕协奏 → 光雕粒子回响（720×1280）→ 织光机的临时节目单，在竖屏切场、
短暂后台再回前台后，第三项保留为失败，原因 `runtime.preparation_device_changed`。
旧场与节目单未丢失，但下一场不能直接 Go。失败证据：
`out/android-continuous-program/079603691a8f415f844609f80c5d455d/`，
`ui-14.xml`/`failure.png` 包含队列错误，`failure.log` 包含实际生命周期。

## 原因与修复

bgfx 宿主提交发现画布尺寸改变时调用 reset，并递增 presentation_generation。
它发生在 SceneDeck 当帧图执行/预备提交之后。Runtime 的分帧准备正确拒绝跨代次
继续使用部分结果，但 SceneDeck 只处理显式设备释放和目标画质变化，没有在下一帧
先识别该呈现代次变化，因此把可恢复的宿主变化当成了来场内容失败。

SceneDeck 在消费 Go/准备结果前观察 Renderer 代次；变化时释放来场部分资源，
清除其 GPU 就绪状态，保留 CPU 包、队列条目和音频过渡身份并重新分帧准备。
重新准备淡化目标；旧场普通帧仍使用 Runtime 已有的重绘路径，不无条件销毁旧图。
若正在恢复串行硬切的旧图，则重启恢复准备，保留此前已接受的冻结纹理。
真实设备释放仍释放所有 GPU 所有权；内容/预算错误仍保持显式失败和重试。

## 漏测路径与永久回归

之前的 Runtime 设备变化测试只证明“拒绝过期准备”正确，SceneDeck 的显式
ReleaseGraphics 测试只证明“调用方主动释放后能重建”。它们没有覆盖宿主在图提交
之后因实际窗口方向/大小变化重置的时序。

新增 Windows `scene_program_ui`：实际 ImGui 队列 Go，三套内置作品、两个配乐、
横竖横画布；下一场准备已有进度时改变真实宿主窗口大小，还覆盖暂停、seek、
图形释放和恢复，读回各个已接受场景的非均匀实际像素。
`out/p3-presentation-recovery-windows-tests.log` 8 项通过，包含串行 GPU+音频硬切、
队列/恢复/音频核心和部署 smoke；详细输出保留到
`out/p3-presentation-recovery-windows-detail.log`。

Android 自动检查通过真实目录/节目单 UI 建立临时三场列表，逐场检查暂停 Go、
实际消费接管、画布方向和队列剩余数量，竖屏时短暂后台/恢复；不覆盖已保存列表。
工具为 `tools/test-android-program-ui.py --reopen-only --mixed-draft` 和
`tools/test-android-continuous-program.py`。修复 APK 已覆盖安装并完成同一三场实机复验：
`out/android-continuous-program/e1574ccf6f9848f7ab751a4288676263/`，
摘要 `out/p3-mixed-program-android-presentation-fixed.log`。
三次接管身份 1/2/3，剩余行 2/1/0，音频同步 true/false/true；画布横/竖/横，
竖屏后台恢复后第三场成功，不再出现该失败。已查看三张实际输出截图；没有把
独立原生程序或 Windows 成功代替 APK 工作流。APK SHA256：
`0f21c36617b961d06f63616b680110362fb541683ff636b54669ee1fa5bd0569`。

## 验证边界

截图和消费帧不是声学回录；短暂后台不是长稳。工具限定 Redmi K40S 2400×1080、
440dpi 锚点，目录/弹窗控件仍按新鲜 accessibility XML 定位。校准工具时曾遇到
输入法组合输入、底部导航栏偏移和继承量化设置问题，均保留失败输出；一次偏移
误开文件选择器已取消，未导入文件。最终临时条目显式选立即模式；缺节拍网格
拒绝下一小节是正常合同，不计为播放器错误。
