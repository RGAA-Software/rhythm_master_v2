# P5 矢量几何实施记录

当前是 `geometry2d` 中的独立值类型填充／描边基础验证，尚未连接图节点和实际
GPU 输出，不表示矢量创作已交付。继续复用已有 `scene::Path`、点桥接和 GLM
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

下一步接入路径到纹理的显式投影、viewer、缓存和实际从零作品。尚未计入图算子。
