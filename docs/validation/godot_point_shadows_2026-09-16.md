# Godot 点光全向阴影验证

日期：2026-09-16。范围是 W1.1 的第四批增量：在方向光单图/双级联和聚光单图合同不变的
前提下，为点光增加 Godot cube 模式的六面全向阴影。没有增加第二个渲染后端或公开
Godot/bgfx 类型。

## 固定来源与适配

继续使用 Godot 4.5.1-stable 提交
`f62fdbde15035c5576dad93e586201f4d41ef0cb`（MIT）。主要参考
`servers/rendering/renderer_scene_cull.cpp` 的 omni cube 分支：六面固定为
`+X, -X, -Y, +Y, +Z, -Z`，up 向量依次为 `-Y, -Y, -Z, +Z, -Y, -Y`，每面使用
90 度、1:1 透视投影，远面为点光 range。来源哈希、符号和修改记录在
`provenance/godot_shadows.json`，原许可在 `third_party/notices/godot/LICENSE.txt`。

项目现有公开纹理合同没有 cube render-target face，因此 Runtime 持有六对独立的有界
2D 颜色/深度附件，复用普通场景深度 pass。接收 shader 按灯光相对向量的绝对主轴选择
同序面。前两面复用已有 sampler 4/7，后四面使用 8–11；材质、环境和 morph 的 0–6
绑定不变。SM5 与 GLES 300 均编译通过。

这是 Godot cube 路径的聚焦适配，不是 dual-paraboloid。与 Godot 原生
`samplerCubeShadow` 相比，当前六张 2D 图的 PCF tap 在所选面内 clamp，不能跨 cube
边缘取样；边缘 seam 视觉评审和必要的跨面滤波是明确保留的兼容缺口。

## 合同、预算与生命周期

- 点光必须发布六个有效、互不重复、同分辨率的深度句柄；普通 `depth_` 和方向光级联
  同时出现会拒绝，点光/聚光/方向光与阴影表示不一致也会拒绝。
- `shadow_near` 必须小于点光 range。点光不接受方向光级联属性。
- 图编译器保留方向光优先、局部光随后排列的身份规则，并按点光六次 caster pass 计算
  索引工作；高几何 Chromatic Loom 切换点光时以 `graph.scene_budget` 拒绝，没有抬高
  既有 300 万索引提交预算。
- 每面颜色/深度合计每像素 8 字节；1024 档六面精确为 48 MiB。六对附件全部分配成功
  后才发布；设置不变时复用，关闭、预览释放和 Runtime reset 会完整释放。
- 旧图没有新字段；方向光与聚光路径的单图默认、双级联选择和 PCF 档位保持不变。

## 验证结果

CPU 合同覆盖六面相机方向/up/near/far、六对资源的精确内存、复用和释放，点光缺面、
重复面、颜色误绑、错误灯类型和级联组合均拒绝。图编译覆盖点光六 pass 和点光级联的
编译期拒绝。`scene_graph`、`scene_render_contracts`、`shadow_runtime` 的串行日志为
`out/point-shadow-final-cpu.log.runs/1789569181436937100.log`；缺少首面但残留其他面的
补充拒绝日志为 `out/point-shadow-final-contract.log.runs/1789569349256339800.log`。

Windows D3D11 完整 GPU probe 每次只在六个深度面中的一个写入遮挡，六个不同世界位置的
接收片分别命中 `+X/-X/-Y/+Y/+Z/-Z` 对应面；其余五个保持照亮。原方向/聚光、
Nearest/PCF5/PCF13、稳定方向光、材质、环境、实例、skin、morph 和形变像素同时通过。
日志为 `out/point-shadow-final-gpu.log.runs/1789569188895755300.log`。

同一 scene/surface ABI 使用已跟踪 `tools/shaderc.exe` 编译 Windows SM5 和 Android
GLES 300 的四种 surface 表达式及六种 vertex 组合；重复产物一致、非法表达式拒绝。
D3D11 随后创建 24 组 scene program 并验证全部 11 个 sampler 绑定，日志为
`out/point-shadow-material-gpu.log.runs/1789567973453036200.log`；artifact 白名单和畸形
绑定拒绝日志为 `out/point-shadow-artifact-final.log.runs/1789568160812226300.log`。

真实作者路径使用当前 Sonic Enamel 编译图中已有的点光（light 1），640×360 静音运行
121 帧。点光 PCF13 相对关闭阴影的检查点最大平均 RGB 差为 `0.308220`。稳定纹理占用
从 `20,053,444` 增至 `70,385,092` 字节，差值 `50,331,648` 字节恰为六个 1024²
颜色/深度对。点光整帧 host p50/p95 为 `9.9600/13.8741 ms`，无阴影为
`5.9836/11.3490 ms`；这是包含图执行、提交和宿主调度的短测，不作为隔离 GPU 成本。
串行作品日志为 `out/point-shadow-final-work.log.runs/1789569197186318300.log`，机器
结果和对照帧位于 `out/windows-pcf13/shadow-work-gpu/sonic-enamel/`。

## 未覆盖范围

本批证明点光阴影从作者图、编译预算、运行时相机、RAII 资源到 D3D11 六方向采样完整
贯通。仍未证明 cube 面跨边缘 PCF、Android 实机像素、透明 caster、多点光同时投影、
隔离 GPU pass 成本或多个完整作品周期；W1.1 因此保持进行中。
