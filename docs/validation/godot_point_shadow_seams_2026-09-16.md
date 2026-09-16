# Godot 点光阴影跨面滤波验证

日期：2026-09-16。范围是 W1.1 点光 cube 阴影的后续增量：修复六张独立 2D 深度图
在 PCF tap 越过 cube face 时被 clamp 到原面边缘的问题。六面相机、资源合同、预算和
作品选择均保持不变。

## 来源与适配

固定 Godot 4.5.1-stable 提交 `f62fdbde15035c5576dad93e586201f4d41ef0cb` 的 GLES3
cube 路径使用 `samplerCubeShadow`，硬件按三维方向选择相邻 face；点光阴影写入的是径向
线性深度。项目公开合同使用六张 2D 可采样透视深度图，不能直接获得硬件 cube 边界行为。

本次对每个越界 PCF tap 执行以下有界适配：从当前面 UV 和 Godot 固定 face 基向量还原
源面射线；保持接收点在源面前向平面上的深度；按射线绝对主轴选择 `+X/-X/-Y/+Y/+Z/-Z`
相邻面；用该面的 world-to-clip 矩阵重新计算 UV 和比较深度。中心 tap 不变，方向光和
聚光仍走原 2D 路径，点光继续使用项目既有 Nearest/PCF5/PCF13 核。

这消除了 2D clamp 接缝，但不宣称与 Godot 硬件 cube 比较逐像素相同：项目保留透视
深度及显式 5/13 tap，Godot GLES3 cube 路径使用径向深度和硬件 shadow sampler。
修改记录同步在 `provenance/godot_shadows.json`。

## 固定像素验证

Windows D3D11 探针使用六个真实 90 度面矩阵，在全部 12 条 cube edge 和 8 个 corner
放置接收片；中心选择面全遮挡、相邻面保持全亮。代表性的 `+X/+Z` 回读红通道为：

- Nearest：`0`，中心仍选择被遮挡的 `+X` 面；
- PCF5：`17`，部分 tap 已进入全亮 `+Z` 面；
- PCF13：`26`，更大的核获得更多相邻面样本；
- 两面全亮对照：`84`。

同时通过六个 face 独立选择、既有方向/聚光阴影、三档过滤、材质、环境、实例、skin、
morph 和形变检查。全部 edge/corner 的串行日志为
`out/point-shadow-all-seams-diagnostic-2.log.runs/1789571424974501000.log`。

同一 shader 使用已跟踪 `tools/shaderc.exe` 编译 Windows SM5 和 Android GLES 300 的
六种 vertex、四种重复 fragment 与非法表达式拒绝；输出目录为
`out/point-shadow-seam-final-material-profile/9daefab0a3604b57a18b0e0b46a417de/`。两端 artifact
白名单、profile 隔离与畸形绑定拒绝日志为
`out/point-shadow-seam-final-artifact.log.runs/1789570351123054300.log`。

## 作者作品回归

Sonic Enamel 当前编译图再次以点光 PCF13 和关闭阴影各运行 121 帧。最大平均 RGB 差为
`0.308209`；稳定纹理差保持 `50,331,648` 字节，即六个 1024² 颜色/深度对。
点光整帧 host p50/p95 为 `10.0814/14.5801 ms`，无阴影为 `6.1677/10.5060 ms`。
这些是包含图执行、提交和宿主调度的短测，不是隔离 GPU pass 成本。串行日志为
`out/point-shadow-seam-final-work.log.runs/1789570371331578500.log`，机器结果与帧位于
`out/windows-pcf13/shadow-work-gpu/sonic-enamel/`。

## 未覆盖范围

本批建立了全部 cube edge/corner 的永久像素回归和实际作品回归。仍未完成 Android
实机像素、与 Godot 硬件 cube sampler 的固定场景对照、隔离 GPU pass 成本或多个完整
作品周期；W1.1 保持进行中。
