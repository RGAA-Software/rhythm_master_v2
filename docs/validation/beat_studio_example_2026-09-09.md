# Studio 节拍画面与音乐示例验收

P1.5 使用原有可编辑“光幕协奏”，新增 120 BPM、4/4、0 秒原点元数据；工程 schema 6、
运行包 ABI 4。素材仍是原有四项哈希资产，未重新编码或新增依赖。
[演出操作](../luminous_concerto_performance.md)记录快照、Cue、取消、保存和发布边界。

`beat_studio_gpu` 运行真实 Windows Studio、ImGui 按钮、D3D11：从无网格工程开始，
启用网格、选择下一拍、暂停后召回，读取最终输出区域的 17×17 像素。暂停期间以及
0.490 秒保持红色，0.500 秒变成蓝色，当前安装计划代次不变且无预算降级；保存和
发布保留网格，现场覆盖不改默认值。重新打开必须得到新的有效计划，画面恢复红色。
截图、像素区域元数据、工作流代次日志保存在
`out/windows-release/beat-studio-gpu/77ded6ff79544b35bc0075491e012c4b`。
日志 `out/p1-studio-beat-reopen-gpu-tests.log`，通过（7.60 秒）。

四项交付强制检查仍由 Python 构建入口调用；中英文实际模板应用检查新增
Luminous Concerto，并核对应用后重映射、当前 85 节点、两项片段波形缓存、保存与
发布的网格。四项通过（18.25 秒），日志 `out/p1-beat-example-delivery.log`。
Windows Studio/Player 完整 deploy 已更新，各含 20 个 DLL 及资源。

Android 新 APK 构建和覆盖安装成功；从内置目录选择竖屏 Sculpture particle echo
得到 1080×2400，演出控件可打开；再选择新版 Luminous Concerto 得到 2400×1080，
控件直接读取工程的 120 BPM、4/4 网格，默认执行时机仍为立即。
证据 `out/p1-orientation-{portrait.png,controls.xml}`、
`out/p1-example-{landscape,current}.png`。这不是修改设备系统方向强行旋转。
APK 构建日志 `out/p1-beat-example-android-build.log`；外部 Perl 的系统 locale 回退
提示不是本次 C++/Java 编译警告，新增项目代码无编译警告。

新版示例的手机量化回归通过，证据
`out/android-beat-ui/0b07b5700b6549db958b472649d66d8c`，包含模式保留、重复请求、
暂停、取消、恢复及实际宏值变化；日志 `out/p1-example-android-ui.log`。
长稳与热测试仍在 P9。
