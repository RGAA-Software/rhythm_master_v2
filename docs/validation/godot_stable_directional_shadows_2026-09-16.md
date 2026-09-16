# Godot 稳定方向光投影验证

日期：2026-09-16。范围是 W1.1 的第二批增量：实现方向光正交投影的 texel
稳定化，并增加两件真实作者图的移动相机、细几何和大投影短验证；不在本批实现级联、
点光全向阴影或完整动态作品验收。

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

## 真实作品移动短验证

永久 `shadow_work_gpu` 回归使用提交中的 Porcelain Bloom 作者图，经当前 `graph.proto`
编译后的 `graph.pb` SHA-256 为
`2d2da3dc82967ce08741ee8ee5b4318519b2b127e65af084c3893a7c20696d98`。实际路径是：

1. 从 `out/windows-pcf13/content/templates/porcelain_bloom` 读取当前编译图；
2. 用正式 Registry 编译 ExecutionPlan，加载 GLB 和 shader 等非音频资产；
3. 将唯一 `scene.shadow` 设为 PCF13，同时生成只关闭阴影的对照计划；
4. 在 Windows D3D11 Runtime 以静音运行 121 帧、640×360、30 fps 作品时间并逐帧回读。

缺少隔离 LGPL FFmpeg SDK 时，带 soundtrack 的 `.rhythmpack` 发布会明确报
`audio.decoder_unavailable`；因此本检查没有把临时媒体绕行写成 Player/Studio delivery，
也没有把合成音频冒充真实 PCM。视觉图、非音频资产、图编译和 Runtime/Renderer 均走
产品路径。

静音四秒内相机移动 `3.27719` 世界单位；连续帧全图平均 RGB 差异的
p50/p95/max 为 `4.41914/5.95388/6.09989`，永久断言拒绝超过
`max(10, 1.5 × p95)` 的突发变化。PCF13 与关闭阴影的末帧平均 RGB 差为 `0.46252`，
实际检查的陶瓷壳片内阴影可辨，永久门槛为 `0.25`。纹理占用在稳定段保持
`63,111,876` 字节。末帧保存在 `out/windows-pcf13/shadow-work-gpu/pcf13-final.ppm`
和 `no-shadow-final.ppm`。

短性能段各运行 30 帧预热和 120 帧测量：PCF13 host frame p50/p95 为
`16.6673/17.8048 ms`，关闭阴影为 `16.6442/17.4330 ms`。两组都受窗口显示节拍和
同步影响，只说明本次短测没有观察到可分辨的新增 host-frame 成本，不是隔离 GPU 计时或
稳定 60 fps 声明。机器可读结果位于 `out/windows-pcf13/shadow-work-gpu/results.json`，

## 细几何、大投影和 16 秒边界

同一个永久回归还运行当前 Chromatic Loom 作者图；编译 `graph.pb` SHA-256 为
`1b7d9c4675666d64b85aadaae96d5d3e6b12a994eaeeb25a251e90443a998c80`。它的经纬管线、
金属扣和移动光梭提供细几何输入。检查同时生成作者范围 9 的 PCF5、PCF13、关闭阴影，
以及保持 1024 阴影图而把方向光范围扩大到 36 的 PCF13 压力计划。

静音运行 481 帧、16 秒：

- 作者范围 PCF13 与关闭阴影的检查点最大平均 RGB 差为 `0.514314`；
- PCF5→PCF13 的实际作品最大平均 RGB 差为 `0.0308724`，不是只改变枚举；
- 四倍范围 PCF13 与关闭阴影的差为 `0.591304`，扩大投影后阴影仍进入最终画面；
- 连续帧差异 p50/p95/max 为 `0.385420/1.13072/1.34151`；第 479→480 帧为
  `0.186729`，没有在 16 秒边界出现回跳；
- 稳定纹理占用为 `18,938,052` 字节。

作者范围 PCF13 host frame p50/p95 为 `2.1145/3.6565 ms`，PCF5 为
`1.9770/3.8636 ms`，四倍范围 PCF13 为 `1.8299/3.7527 ms`，关闭阴影为
`1.1698/2.3094 ms`。中位数表明本场景阴影 pass 有可测成本，而 PCF13 相对 PCF5 的
中位差约 `0.1375 ms`；p95 次序反转，不能据此宣称隔离 GPU 滤波成本。机器可读结果和
四张对照末帧位于 `out/windows-pcf13/shadow-work-gpu/chromatic-loom/`。最终 CTest
包装日志为
`out/windows-pcf13/shadow-work-gpu-ctest.log.runs/1789547691792030700.log`。

## 未覆盖范围

这里证明运行时矩阵稳定性、两帧子网格移动像素稳定性、Porcelain Bloom 四秒移动相机，
以及 Chromatic Loom 细几何、四倍投影范围和一个 16 秒边界；它仍不等同完整动态作品
视觉验收。更多代表作品、连续多个完整作品周期和隔离 GPU 成本仍待后续。本批尚未实现
的双级联方向光已在后续
[验证记录](godot_directional_cascades_2026-09-16.md)交付；点光全向阴影仍未实现。
