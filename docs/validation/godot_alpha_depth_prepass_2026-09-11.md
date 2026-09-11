# Godot alpha 深度预通道验证（2026-09-11）

## 实现边界

`material.unlit` 与 `material.pbr` 新增显式 `alpha_depth_prepass` 开关，默认关闭。
启用后材质进入透明队列；渲染器先运行只写深度的预通道，以材质颜色 alpha 与基础纹理
alpha 的乘积作为覆盖率，并采用 Godot 场景预通道的 0.99 阈值丢弃透明像素。随后颜色
通道使用相同覆盖率、预乘 source-over 与 less-equal 深度测试。普通不透明材质仍可实例
合批，启用预通道的材质按对象提交，骨架和 morph 使用各自顶点变体。

这项模式适合带接近不透明区域和透明孔洞的网格、树叶或装饰面。半透明覆盖低于阈值时
仍走排序混合，因此它不等同于 OIT，也不保证任意相交半透明三角形的逐像素正确次序。
自定义 surface 程序仍以材质基础纹理 alpha 作为预通道覆盖率，保持 surface RGB 合同。

## 永久检查

- `scene_runtime` 验证图属性进入不可变材质快照，且启用模式的 alpha=1 材质仍进入透明
  排序队列。日志：
  `out/godot-alpha-prepass-runtime.log.runs/1789116955402786500.log`。
- `windows_gpu_execution_probe` 在 D3D11 上绘制蓝色不透明背景、红色 alpha 预通道前景
  和后提交的绿色半透明中层。前景纹理左半 alpha=1、右半 alpha=0；实际回读断言左半
  保持红色且中层不能穿透，右半透明孔洞显示绿色与蓝色的混合。该路径同时验证纹理
  alpha、discard、预通道深度写入、颜色通道 less-equal 和普通透明兼容。
- 聚焦 GPU 日志：
  `out/godot-alpha-prepass-gpu.log.runs/1789116601747590000.log`。
- 场景运行时与 D3D11 截图回归日志：
  `out/godot-alpha-prepass-scene.log.runs/1789116628233114400.log`。
- 完整 Windows Release delivery 通过 9/9，包含真实 Studio 模板应用、节点 ID 重映射、
  保存重开、发布和 GPU 控件路径；Studio 与 Player deploy 均更新可执行程序、20 个 DLL、
  双语资源和内容。日志：
  `out/windows-release/studio-delivery-tests.log.runs/1789116990367414300.log`。

最初的预通道片元着色器只声明实际读取的两个 varying，shaderc 可以编译，但 bgfx 在
D3D11 创建场景程序时因顶点/片元接口不完整而返回无效句柄。永久修复是让预通道声明
完整场景 varying 接口；验证必须运行真实 GPU 程序创建，不能以 shaderc 成功代替。
