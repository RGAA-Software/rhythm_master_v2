# 3D：以 Godot 为主要设计与源码参考

> 2026-09-06：用户确认的参考方向。2026-09-07 已固定 4.5.1-stable 提交并适配
> 投影和球体网格源码；基础数值与 Windows 场景 GPU 验证通过，图和应用接入待完成。
> 具体范围见 [验证记录](validation/scene_foundations_2026-09-07.md)。

## 1. 定位

Godot 是 3D 能力的首要设计与代码参考，不仅观察功能，还要评估可提取的实现。
TouchDesigner/TiXL 继续作为视觉编程、创作流程和实时预览参考；Axmol 保留已有
2D/设计分辨率经验，但不是本次 3D 的主要参考。

不把产品改成 Godot 编辑器，也不默认嵌入整个 Godot 引擎。
继续保留独立 RhythmRender/bgfx、类型化节点图与四平台 Player 的架构边界。
参考引擎的选择不等于承诺完整复制其所有 3D 特性。

## 2. 重点参考范围

| 领域 | 需要研究与采用的原则 | 我们的落点 |
| --- | --- | --- |
| 场景 | 层级变换、可见性、实例与资源分离 | Scene3D 与变换/可见性模块 |
| 相机 | 透视/正交、投影与视口关系 | Camera 数据、竖屏/横屏正确投影 |
| 模型 | glTF/GLB 场景、网格、材质、动画导入 | 离线导入器和平台无关资源包 |
| 材质 | 标准材质参数、纹理语义、PBR/透明/自发光 | Material 图和版本化参数契约 |
| 灯光 | 方向/点/聚光灯、环境光、阴影与质量分级 | 独立 lighting/pass 模块 |
| 动画 | 动画资源、混合、骨架与 morph 的责任分离 | 可由时间/信号驱动的动画运行模块 |
| 效率 | 实例化、裁剪、资源缓存与不同设备档位 | 受能力约束的执行/渲染计划 |

这张表是研究任务，不是已经逐项完成源码审查的结论。正式复用前固定 Godot
稳定版本和具体 commit，再为每个领域记录文件、依赖、许可、可迁移部分和测试。

## 3. 代码复用边界

- 优先抽取可独立解释和测试的算法、资源处理和 shader 思路；可适配的实现允许
  直接复用，不因“不是我们写的”重复造轮子。
- 遇到 Object/Variant/Ref/RID、引擎容器、资源系统或 RenderingServer 等依赖，
  先计算拆分成本；改用项目值类型/智能所有权/稳定句柄，不整片复制依赖图。
- Godot Shader、材质和渲染资源不视为 bgfx 即插即用；显式适配绑定、颜色/坐标、
  shader 产物及 capability profile。不可在业务层绕过 RhythmRender 调用原生 API。
- Render Graph 与场景生命周期由我们自己的契约管理，不同时运行两套 renderer。
- glTF 解析器仍按体积、依赖与正确性评估；参考 Godot 导入语义，不代表必须整体
  搬入其 glTF 模块。标准测试资产用于验证，不把解析库本身等同完整场景支持。
- Godot 内嵌的媒体播放器不纳入迁移：媒体唯一后端仍是 FFmpeg。
- 不因此新增 3D 物理选型；既有 Box2D 只负责 2D，3D 物理另立需求。
- 保留原版权/MIT 许可，第三方内嵌代码另审；改写或翻译代码不抹去来源。
  新维护实现遵循 Google 风格、4 空格、声明初始化和智能所有权规则。

## 4. 实施顺序

1. 固定参考版本，建立场景/相机/材质/模型的源码映射与复用清单。
2. 最小 3D 闭环：glTF 静态模型、层级变换、相机、深度、unlit 材质，
   Scene3D -> Texture -> 2D 合成和节点实时预览。
3. PBR、纹理语义、灯光、阴影、IBL、透明和颜色管理。
4. 实例化/裁剪/性能档位，再扩展骨架、morph 和动画混合。
5. 横屏/竖屏/正方形、多视口、软硬件能力差异与媒体/音频驱动组合验收。

用同一 glTF 和明确相机/环境制作参考图，检查坐标、法线、UV、颜色、阴影和动画。
Godot 可作为对照之一，但不要求不同 renderer 像素完全一致；数值契约和可解释的
视觉差异分别记录。Windows 先验收，再推进 Android Player；macOS/iOS 按
2026-09-07 用户决定留到最后的 Apple 移植阶段。

## 5. 初始上游入口

- [Godot 场景 3D 源码](https://github.com/godotengine/godot/tree/master/scene/3d)
- [Godot glTF 模块](https://github.com/godotengine/godot/tree/master/modules/gltf)
- [Godot 渲染实现](https://github.com/godotengine/godot/tree/master/servers/rendering)
- [Godot 许可与第三方说明](https://docs.godotengine.org/en/stable/about/complying_with_licenses.html)

以上 master 链接仅为发现入口，不能代替正式导入时的固定提交与文件清单。
