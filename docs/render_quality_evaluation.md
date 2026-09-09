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

该选项现已完成图合同、生命周期、UI／包及作品验证，详见下节。新增属性使用既有值格式，
旧 Player 通过未知属性校验拒绝，不能默默忽略质量设置。

相交半透明问题仍待独立方案比较；尚未采用 OIT、TAA、MSAA 或 alpha depth prepass。
2× 超采样不会修复排序，不能以本增量宣布全部 P6.3 完成。

## 可选 2× 场景超采样交付

`scene.render` 新增 `scene_antialiasing`：0 关闭，1 为 2× 超采样。默认单 pass
保留，启用后场景 pass 加一次缩小 pass。`SceneColor` 独立拥有颜色输出和高分辨率
附件；切换开关、尺寸、精度或 Reset 释放旧附件，静态图不重复绘制／分配。
原点、相机比例、输出尺寸不变；Float16 保持到缩小后的目标。分帧准备统计两次
实际 pass，超预算不发布部分结果，缩小后可恢复。没有改变显式颜色／深度捕获。

Windows `out/p6-scene-aa-graph-tests.log`、`out/p6-scene-aa-runtime-tests.log`、
`out/p6-scene-aa-staging-tests.log` 通过合法／非法 profile、旧图兼容、缓存、开关
释放、竖屏 Float16、预算拒绝恢复、分帧准备和原有场景／纹理生命周期检查。
Android 原生同组图／场景合同在 `out/p6-scene-aa-android-contract-tests.log`，
新增分帧准备检查在 `out/p6-scene-aa-android-staging-tests.log` 通过。

“共振星仪 / Resonant Armillary”以完整 Studio 从零创作流程启用该选项，
`out/p6-scene-aa-authoring-tests.log` 通过，运行目录
`out/windows-release/from-empty-studio-3d/35b05496d83f4fbf819f161388679efd/`。
真实 PCM/GPU 差：音乐／静音 1.06503、低频／静音 6.66417、高频／静音 8.63421、
低／高频 2.83642。人工看图确认青色／金色圆环、交叠遮挡、镜面高光和构图正常。
源模板从此通过的工程提取，版本升为 0.2.0，仍为功能示例，不新增 P7 品质计数。

Windows Studio／Player 完整 deploy 和五项强制模板检查通过，最终日志
`out/p6-scene-aa-thumbnail-delivery.log`（28.15 秒），目录缩略图由实际渲染更新。
`out/p6-scene-aa-export.log` 通过 640×360／30 FPS／120 帧有声导出、重复解码帧
一致、静音对照及取消清理，音频 MSE 7.6297e-06。

Android APK SHA256
`32d48e95e7dac044163ab3a11f6c706dae949b885a32fdf1ee54c40460d79dc8` 已覆盖安装。
`out/p6-scene-aa-android-music.log` 通过包内真实 PCM 的音乐／静音各 960 帧 GLES，
640×360，峰值纹理 8755204 字节；2/6/10/14 秒平均 RGB 差为
0.438924／1.53549／0.888368／2.4497。
`out/p6-scene-aa-android-ui.log` 的应用内选择和暂停恢复截图在
`out/android-authored-works/fbb155a7d38d491e88265299dbbe8778/`，已人工查看正确作品
标题、两环和高光，时间 2.27→3.39 秒且 RMS 非零；安装 APK 哈希匹配，用户保存
工程字节未改。没有声学回录、长稳或通用相交透明质量合格的声明。

## 证据

- 初始构建／测量：`out/p6-quality-baseline-build.log`、
  `out/p6-quality-baseline-android-build.log`、`out/p6-quality-baseline-d3d.log`、
  `out/p6-quality-baseline-gles.log`。
- 扩展成本测量：`out/p6-quality-budget-build.log`、
  `out/p6-quality-budget-android-build.log`、`out/p6-quality-budget-d3d.log`、
  `out/p6-quality-budget-gles.log`；完整数字和 PPM 在
  `out/p6-quality-budget-windows/`、`out/p6-quality-budget-android/`。
