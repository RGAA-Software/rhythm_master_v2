# P6.4：附加场景输出与按需调度决定

2026-09-10。决定：保留显式颜色／深度输出；本轮暂不增加通用法线、对象 ID 附件，
不强制 G-buffer。现有几何法线诊断可使用已交付的受限 RGB 材质表达式。
这是有界评估收口，不表示已经实现屏幕空间法线后处理、GPU 拾取或完整延迟渲染。

## 消费者与现有路径

| 需求 | 核对的实现 | 本轮结论 |
| --- | --- | --- |
| 景深、深度观察 | `scene.capture`→`scene.depth`→`texture.dof`／`depth.linearize` | 已有真实消费者，保留类型化深度和准确投影元数据 |
| 颜色与深度同时观察 | `scene.color`／`scene.depth` 共享 Runtime `SceneCapture` | 同一捕获只绘制一次；不因两个消费者复制附件 |
| 几何法线诊断 | 白色 unlit 材质＋`normalize(normal)*0.5+vec3(0.5,0.5,0.5)` | 现有 Surface RGB 表达式可得到诊断颜色；本次两端像素验证通过 |
| 基础对象选择 | `scene3d/picking.cpp` 与 `studio_ui/scene_selection.cpp` | 当前静态几何用有界 CPU 射线，保留作者路径和实例身份；无 GPU 回读 |
| GPU 变形后拾取 | 现有拾取明确拒绝 deformation／skin／morph | 存在真实缺口，不能以静态拾取宣称解决；独立 ID pass 仍需后续专门合同 |
| 法线驱动描边／SSAO | 当前未实现该消费者与数据法线域 | 暂不先增加无人消费的附件，不把诊断 RGB 当通用法线缓冲 |

几何法线诊断不含切线法线贴图的扰动；必须使用白色无光照基底才与编码值一致。
它输出的是现有颜色纹理，经过显示变换后不能当精确数据法线使用，也不支持同时
保留任意材质颜色而免费获得独立法线输出。欲同时得到原画与诊断需显式第二个场景
分支，由现有需求和预算管理，不能称为 MRT 优化。

## 本地来源核对

实际阅读 Godot 4.5.1（`f62fdbde15035c5576dad93e586201f4d41ef0cb`）本地
`out/reference/godot-4.5.1/drivers/gles3/shaders/scene.glsl` 的
`RENDER_MATERIAL` 路径：分别写 albedo、normal、ORM、emission，法线按
`normal*0.5+0.5` 编码。来源与 MIT 许可沿用 `provenance/godot_3d.json`。
这说明不同用途需显式材质输出路径，不代表 Godot 的完整附件／消费者适合整体搬入。
本次复用项目既有 Surface 包装器和 bgfx 适配器，没有复制或修改 Godot 原仓库。

## Windows／Android 实测

扩展 `depth_runtime_tests`，构造 1002 节点、200 个互相独立的场景捕获分支。
Windows 与 USB Android 原生执行相同 Null 后端合同，两端均得到：

- 只请求最终渐变：8196 字节纹理、零网格；未观察场景不分配、不绘制。
- 请求一个分支的颜色与深度：401412 字节纹理、一个网格、一个新增场景 pass。
  最终画布为 64×32，观察分支使用既有 256×144 viewer extent；不是全图 64×32。
- 静态重复求值：零绘制、不增加纹理；两份预览各一次转换，共两个 preview pass。
- 关闭预览释放预览资源，移除观察请求释放捕获的颜色／深度及网格，回到8196字节。
  重新请求可重建，Reset 后资源全部释放。

这些是编译裁剪、所有权和调度合同，不是 1002 节点实际 GPU 场景性能测试。
日志：`out/p6-output-demand-build.log`、`out/p6-output-demand-contracts.log`、
`out/p6-output-demand-android-build.log`、`out/p6-output-demand-android-contracts.log`。

新增 `--normal-view` 真实 GPU 探针复用 Renderer SurfaceProgram，使用同一四边形
与非均匀 X 缩放 1／0.5／2，逐通道比对中心像素与 CPU 逆转置／归一化公式，
允许 2/255 量化差。D3D11 和 Redmi K40S Adreno 650 GLES 均通过；每次一个
场景 pass，64×32 的目标及深度附件共16384字节，作用域结束释放纹理和程序。
这只是受限诊断表达式探针，没有新增产品节点，不冒充 Studio 或 APK UI 验收。

编译产物：`out/p6-normal-view-shaders/ad7d552974e74b52bfe10d41c42ee48a/`，
两目标各六种顶点合同、四份重复编译一致片元以及两份非法表达式拒绝。
日志：`out/p6-normal-view-shaders.log`、`out/p6-normal-view-build.log`、
`out/p6-normal-view-d3d.log`、`out/p6-normal-view-android-build.log`、
`out/p6-normal-view-gles.log`。

## 预算、兼容和重新进入条件

以1080p计算，单个RGBA8额外附件需8,294,400字节；RGBA16F需16,588,800字节。
若同时加入RGBA16F法线及RGBA8身份编码，单场景额外24,883,200字节；
双场切换还需按同时存活场景累加。以上为格式尺寸计算，不是新附件实机测量，
不含深度、驱动、staging和几何成本。没有基于该估算宣称MRT或整数附件跨平台可用。

本轮不新增 SceneImage 字段、节点、包 ABI 或后端依赖，旧图行为保留。
下一次有法线后处理作品或 GPU 拾取明确需求时，先实现一个真实消费者的探针：
确定世界／视图空间、几何／扰动法线、透明覆盖规则、对象身份与作者映射；验证
D3D11／GLES格式、选择性输出与回读延迟，定义代次和过期结果拒绝，再进入正式合同。
任何已采用输出仍须按请求裁剪、复用同一捕获、可预览、受预算约束并在切图时释放。
P6.4 本轮结论不阻塞 P7 内容生产与 P8 产品工作流，未采用能力保持明确未支持。
