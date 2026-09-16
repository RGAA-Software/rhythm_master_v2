# Godot 稳定方向光投影验证

日期：2026-09-16。范围是 W1.1 的第二批增量：只实现方向光正交投影的 texel
稳定化，不在本批实现级联、点光全向阴影或完整动态作品验收。

## 固定来源与适配

- 上游为 Godot `4.5.1-stable`，提交
  `f62fdbde15035c5576dad93e586201f4d41ef0cb`，MIT；许可保留在
  `third_party/notices/godot/LICENSE.txt`。
- 方向光稳定边界来自 `servers/rendering/renderer_scene_cull.cpp` 2301-2307，SHA-256
  为 `30b9fc975a861f10f4c3425d02c2513ce82937c2d6917a26e4e82454080b0493`。
  舍入语义来自 `core/math/math_funcs.cpp` 的 `Math::snapped`，SHA-256 为
  `4317feb7b84885dd8554001e6d5e5cf7247eeb7e2ca430cbea4ff7cf30418454`。
- Godot 以 `radius * 4 / texture_size` 吸附正交边界。项目相机合同直接保存完整高度，
  因而将中心的光空间 X/Y 吸附到 `2 * extent / resolution`，即两个阴影 texel；不复制
  Godot 图集、级联、剔除树或引擎类型。
- 只移动方向光相机的 eye/target 横向与纵向分量；沿光照深度保持连续。聚光灯仍由真实
  位置、方向和光锥建立透视相机，不经过该吸附。

完整来源和修改记录见 `provenance/godot_shadows.json`。

## 自动验证

`shadow_runtime` 用 256 阴影图、10 世界单位高度验证：

- 中心移动到网格的正向 `0.49` 倍时 eye/target 与原点完全相同；
- 正向 `0.51` 倍和负向 `-0.51` 倍分别前进/后退一个完整网格；
- 沿光照深度的非整数运动不被量化；
- 聚光灯的非整数世界位置保持原值。

受影响目标以 20 worker 增量构建并通过 `shadow_runtime`。边界、内容和 JSON 静态检查
也在本增量完成后执行。

Windows 专用永久 GPU 回归再让同一斜向遮挡体的方向光投影中心移动网格的
`(0.49, 0.31)` 倍，并用 PCF13 回读两张 256×256 D3D11 画面：稳定 Runtime 相机的
前后输出逐字节一致，红通道累计差异为 `0`；只在对照分支恢复旧式未吸附相机后，
累计差异为 `7003`。完整 GPU probe 通过 `tools/verify_windows.py` 的跨进程租约执行，
日志为
`out/windows-pcf13/stable-shadow-gpu.log.runs/1789545029305544800.log`；注册后的 CTest
再次通过，包装日志为
`out/windows-pcf13/stable-shadow-gpu-ctest.log.runs/1789545114872961600.log`。

## 未覆盖范围

这里证明运行时矩阵稳定性，并以两帧实际 D3D11 像素证明子网格移动不再产生旧式抖动；
它仍不等同完整动态作品视觉验收。大投影、真实作品移动相机、细几何的连续多帧回读和
短性能记录仍是 W1.1 后续退出条件；级联和点光全向阴影也仍未实现。
