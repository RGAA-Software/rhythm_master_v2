# P6.3 透明与抗锯齿分项验证

2026-09-10，基于 P5 交付 `91fda5c`。本页区分实际测量、采用决定和仍未实现的范围。

## 复用与基线

检查本地 Godot 4.5.1 `scene/resources/material.cpp` 的透明、alpha depth prepass、
alpha scissor/hash 选择，以及本项目 `scene_pass.cpp`／`bgfx_scene.cpp`：当前是
不透明先写深度、半透明按物体原点排序，再采用预乘 alpha 混合。Godot 的材质模式
不能直接等同于本项目已实现逐像素相交排序。此次测量使用第一方几何、既有 bgfx
颜色／深度绘制、双线性采样和已有 FXAA；没有引入新的第三方实现或依赖。

`MeasureQualityBaseline` 为同一份 D3D11／GLES 测量代码。两张红／蓝半透明平面
左右深度次序相反但原点相同，交换提交次序后两边都整体变色，证明物体级排序不能
正确处理它们的相交。两端结果均为约 `(R64,B128)` 与 `(R128,B64)`；正确逐像素
次序应随交点左右改变。本测试不把测量完成标成透明质量通过。

0.45 输出像素宽的线移动一个像素，采八个相位：原始和 FXAA 在四个相位完全消失。
2 倍宽高渲染并双线性缩小后八个相位都可见。这是覆盖率采样改善，不能证明所有
运动无闪烁、理想面积积分或任意几何已获得足够抗锯齿。

| 后端 | 原始线红通道能量范围 | FXAA 范围 | 2× 超采样范围 |
| --- | --- | --- | --- |
| Windows D3D11 | 0–13260 | 0–13356 | 6478–6478 |
| Android GLES／Adreno 650 | 0–13260 | 0–13358 | 6528–6528 |

## 1080p 短时成本探针

八个覆盖画面的半透明面，1920×1080 输出，分别 1×、2× 渲染并缩小；每档 12 帧，
每帧完成实际回读并检查非黑输出。不是编辑器帧率、纯 GPU 计时或长稳测试。
两档都保留一个输出 resolve 纹理，数字不能直接当作当前单 pass `scene.render` 成本。

| 后端 | 1× CPU＋GPU＋回读总耗时 | 2× 总耗时 | 1× 保留纹理字节 | 2× 保留纹理字节 |
| --- | --- | --- | --- | --- |
| Windows | 45.496 ms | 65.8583 ms | 24883200 | 74649600 |
| Android | 312.577 ms | 710.138 ms | 24883200 | 74649600 |

两端均通过现有 256 MiB 预算准入，不能据此承诺任意复杂图或 Android 1080p 2×
达到 60 FPS。额外场景纹理和深度为 4 倍像素数，还增加一次缩小 pass。

## 决定与实施边界

**限制采用可选 2× 场景超采样**：先用于输出颜色纹理的 `scene.render`，默认关闭；
相机比例和输出尺寸不变，源场景以 2 倍宽高绘制，双线性缩小。精度继承原配置，
资源计入既有全局纹理／pass 预算，超预算明确失败，不偷偷切回低质量。
`scene.capture` 的显式颜色／深度配对、节点小预览以及已有作品默认效果保持既有
语义；没有定义深度 resolve 之前不把高分辨率深度伪装成同尺寸深度输出。

该选项尚待图合同、生命周期、UI／包及作品验证后交付。新增属性使用既有值格式，
旧 Player 通过未知属性校验拒绝，不能默默忽略质量设置。

相交半透明问题仍待独立方案比较；尚未采用 OIT、TAA、MSAA 或 alpha depth prepass。
2× 超采样不会修复排序，不能以本增量宣布全部 P6.3 完成。

## 证据

- 初始构建／测量：`out/p6-quality-baseline-build.log`、
  `out/p6-quality-baseline-android-build.log`、`out/p6-quality-baseline-d3d.log`、
  `out/p6-quality-baseline-gles.log`。
- 扩展成本测量：`out/p6-quality-budget-build.log`、
  `out/p6-quality-budget-android-build.log`、`out/p6-quality-budget-d3d.log`、
  `out/p6-quality-budget-gles.log`；完整数字和 PPM 在
  `out/p6-quality-budget-windows/`、`out/p6-quality-budget-android/`。
