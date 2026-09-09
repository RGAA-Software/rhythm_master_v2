# P5 矢量几何实施记录

当前已完成 `geometry2d` 值类型基础、图运行时接入及 Windows/Android 矢量作品
增量交付，详见 [实际创作与交付证据](validation/vector_authoring_2026-09-10.md)。
继续复用已有 `scene::Path`、点桥接和 GLM
Catmull–Rom 重采样；本增量接收已采样的折线，不承诺任意 SVG 或贝塞尔编辑器。

## 来源与候选核对

- vcpkg `clipper2` 2.0.1，来源 `https://github.com/AngusJohnson/Clipper2`，
  tag `Clipper2_2.0.1`，BSL-1.0。使用其布尔轮廓处理、PolyTree 孔洞层级和
  InflatePaths 描边偏移。已检查本地 `clipper.h`、`clipper.engine.h` 和
  `clipper.offset.h`；不复制自写轮廓相交和描边算法。
- vcpkg `earcut-hpp` 2.2.4#1，来源 `https://github.com/mapbox/earcut.hpp`，
  tag `v2.2.4`，ISC。调用 header-only `mapbox::earcut` 完成含孔洞多边形三角化。
  保留 vcpkg 的 `include-cstdint.patch`，没有项目自改上游代码。
- Clipper2 2.0.1 自带三角化头文件标为 BETA，且明确拒绝相交路径。本增量不采用
  该候选三角化接口；先用 Clipper2 规范轮廓，再用 Earcut，不把未测算法直接定案。
- 两者均通过 `C:/source/vcpkg` 安装 Windows/Android triplet，未升级其他包。
  精确安装包来源清单和原许可复制到 `third_party/notices/vector/<triplet>/`。
  Windows Clipper2 是静态库、MD CRT；Android 使用 NDK 29 arm64 库；跨平台
  可用性以接下来的实际模块测试为准，尚不宣称 Apple 验证。
- 首次 MSVC 编译因 Clipper2 的异常路径中 `DoError` 抛出后的截断代码触发 C4702，
  记录 `out/p5-vector-foundation-build.log`。只在未修改的 Clipper 头文件包含范围
  屏蔽该上游警告；项目适配器保持 `/W4 /WX`，未关闭项目警告检查。

## 合同

公开 API 只用已有 `geometry2d::Point` 和项目 Contour/VectorMesh/StrokeStyle。
第三方类型限在实现文件，调用同步完成；Clipper 的树和 Earcut 临时结果不逃出适配器。
坐标采用画布方向（Y 向下），Clipper 精度固定 1e-6 单位，输入坐标有限且绝对值
不超过 10000。填充使用 even-odd 规则，支持孔洞、嵌套岛和自交；未闭合填充拒绝。
空和退化轮廓可输出空网格。

描边支持 miter/bevel/round 连接及 butt/square/round 开放端点；闭合路径采用
Joined，宽度零输出空。宽度范围 0–100、miter limit 1–16、arc tolerance
0.001–1。输入最多 64 轮廓、累计 1024 点；结果最多 16384 顶点、49152 索引。
这些是入口／输出预算，不是 Clipper 内部每次分配的硬上限；病理自交的成本仍需
在模块与运行时验证中测定，后续图集成不得按无限点数调用。

Windows 的 `vector_geometry`、既有 `geometry2d` 与源码边界检查通过，记录
`out/p5-vector-foundation-tests.log`。NDK 构建及 USB Android 同一组几何合同通过，
记录 `out/p5-vector-foundation-android-build.log` 和
`out/p5-vector-foundation-android-tests.log`。覆盖面积、凹形、孔洞/嵌套岛/自交、
端点、闭合 miter、预算及非法输入。模块依赖验证成立，不等于实时渲染或完整作品验收。

## 图与运行时增量

`texture.path_fill` 接收必需的闭合 Path 与可选孔洞 Path；
`texture.path_stroke` 接收 Path 与可选标量线宽。输出是普通 Texture，直接使用
已有节点内预览、最终输出、合成和发布合同。每条路径最多采样 512 点，闭合路径
不重复尾点，开口路径保留两端。投影选择 XY/XZ/YZ，跨度对应画布短边，保持比例，
世界第二投影轴朝上；描边宽度和圆弧误差以目标纹理像素计。

运行时私有 CPU 缓存以源节点身份和版本、投影、目标尺寸及描边参数为键。
单纯配色修改复用三角化结果；数值驱动线宽会更新几何。最多 64 个矢量节点，
累计缓存最多 65536 顶点、196608 索引；删除节点或 Reset 释放缓存。
静态图沿用已有输出复用，未新增每帧纹理上传。每帧动态路径仍在执行线程同步细分，
不宣称任意复杂矢量都达到实时预算。普通纹理抗锯齿可显式接 FXAA，未实现解析覆盖 AA。

`out/p5-vector-runtime-tests.log` 覆盖实际投影面积和位置、孔洞、颜色复用、尺寸和
路径版本失效、标量线宽失效，以及 Null 后端完整图求值和纹理释放；同时通过原有
路径回归与源码边界检查。Null 后端检查不作为真实图像证据。
NDK 编译后的同一 `vector_runtime_tests` 已通过 USB 在 Android 执行，记录
`out/p5-vector-runtime-android-build.log` 和 `out/p5-vector-runtime-android-tests.log`；
这仍是原生运行时合同验证，Android 应用内画面尚待本作品交付。
`out/p5-vector-content-tests.log` 验证新算子的默认预设覆盖。
中英文名称、连接／端点选项、使用说明和失败原因已接入；归入图像生成类别。
算子使用已有 Path/Texture/Scalar 属性格式，运行包按算子名称校验能力，旧 Player
不认识算子时拒绝载入，未因相同属性格式增加无意义的 wire 版本。

`vector_resonance` 已通过实际空白创作、孔洞／描边画面、PCM 驱动、导出和 Android
播放，见上述独立证据。P5 的频谱与纹理采样桥接、资产局部缓存及系统 IME 专项
继续推进，未将这个增量等同于全部 P5 完成。
