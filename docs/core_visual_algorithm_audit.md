# 核心画面算法审计

日期：2026-09-11。本文回答“哪些画面问题能在用户截图前被发现”，并规定替换顺序。
功能测试、非零像素差和单张缩略图不能作为画质通过依据。核心算法首先与固定 Godot
源码比较；TiXL 用于节点语义、参数和作品结构。其他成熟实现只有在记录 Godot 的具体
兼容缺口后采用。

## 审计方法

每项效果同时检查源码来源和完整度、极端输入的数值行为、Windows D3D11 动态输出。
最小输入集包括中心脉冲、大尺寸柔边图元、细高亮线、透明彩色边缘、HDR 阶梯、深度
前后遮挡和持续运动。检查静止帧、连续帧、画布边缘、回收边界与不同分辨率。输出必须
保留原始运行日志和图像；测试声明产生截图时，截图不存在即失败。

## 当前结论和实施优先级

| 优先级 | 能力 | 当前实现与已知风险 | Godot 基线与行动 |
| --- | --- | --- | --- |
| P0 | 普通 blur | 原 TiXL 五采样横纵核与四 tap 降采样已产生方向性重影风险；2026-09-11 已从运行路径移除 | 固定 `blur_raster.glsl` 的 13-tap 二维 Gaussian 和 mip 链；完成 D3D11 脉冲、透明、径向衰减断言后交付 |
| P0 | glow/bloom | `texture.glow` 已完成 Godot Dual Filtering 的 HDR 筛选、1–6 级降采样/上采样和 RGBA16F 加法合成；首批四件作品已从 blur 拼装迁移 | 继续迁移其余作品并完成大粒子、细亮线、HDR/SDR 与动态输出评审；以 glow/display 联合执行合同补齐 Godot 合成模式 |
| P0 | tone mapping/HDR | `texture.display` 已提供 Godot Reinhard、Filmic、ACES、AgX，曝光在映射前、sRGB 转换在映射后；首批四件作品已接入 AgX | 完成彩色 HDR 阶梯和 glow→tone-map 作品动态评审，再决定高级作品的默认映射 |
| P0 | 透明 3D | 已按 Godot 语义采用不透明优先、透明物体稳定后到前排序，并以变换后的网格包围盒中心替代错误的实例原点；相交透明仍只有顺序相关 source-over | 对照 Godot 的材质 render priority、sorting offset 和 alpha depth prepass 逐项扩展；复杂 OIT 单独验证后决定 |
| P1 | 景深 | TiXL golden-angle gather 已可运行，但没有完整近/远 CoC 分离、遮挡权重和背景泄漏控制 | 对照 Godot `bokeh_dof` 的 shape/quality、近远场和合成；用前景细线、远景高光、运动相机验证 |
| P1 | 阴影 | 已采用 Godot PCF5 核，但只有单张阴影图、单选择光源和开关式低档过滤；大投影和运动时可能锯齿/闪烁 | 扩展 Godot filter quality、方向光级联/稳定投影和点光语义；保留当前 PCF5 作为低档 |
| P1 | 环境预滤波 | 当前 GGX/Hammersley 预滤来自 TiXL，图集和样本预算受限；粗糙材质可能出现噪声或层级跳变 | 对照 Godot reflection/environment filter 的分布、LOD 与能量守恒；固定输入环境做粗糙度阶梯比较 |
| P1 | 粒子呈现 | 解析圆点的连续柔边已验证，仍缺纹理图集、材质变化、软深度交界和体积感；大量同形粒子容易显得单薄 | 参考 Godot 粒子 quad/material、soft particle 和发光材质路径；保留 GPU simulation 与项目节点契约 |
| P1 | trail/feedback | 单历史纹理缩放旋转并线性混合；快速运动、长尾和循环边界可能出现重影断层 | 对照 Godot motion/temporal 处理及成熟反馈实现，增加速度/衰减一致性和回收边界动态测试 |
| P2 | 噪声与纹理生成 | TiXL 固定四 octave Perlin 可重复且范围有限，放大时可能暴露网格和频带单一 | 审查 Godot FastNoiseLite/纹理噪声；增加 domain warp、不同频谱层和抗走样后再决定迁移范围 |
| P2 | 抗锯齿 | `glsl-fxaa` 是成熟实现且两端有像素证据，但单一 FXAA 会软化细线，也不解决时域闪烁 | 记录 Godot MSAA/TAA/FSR 路径的能力和成本；Windows 先做细线运动与粒子闪烁对照 |

## 当前验收状态

普通 blur 已完成源码替换和完整交付，本项关闭。永久 GPU 检查读取实际渲染目标并断言：常量图
守恒、透明背景不泄漏隐藏 RGB、半径 1 命中 Godot 核的中心/邻域衰减、半径 8 的 mip
结果在 X/Y 方向近似一致且向外单调衰减、半径 0 保持原图；Studio delivery、实际
模板应用和 deploy 更新均已通过。

普通 blur 的最终交付证据、曾经漏掉截图的原因和永久检查见
[验证记录](validation/godot_blur_and_generated_cleanup_2026-09-11.md)。

Glow 核心节点已通过 D3D11 中心高光、连续远近衰减、边界和透明 alpha 回读；固定来源见
`provenance/godot_glow.json`。核心 glow→AgX 顺序已通过组合回读，首批四件作品通过真实
Studio 重建、保存发布与音乐运行；其余作品迁移和动态视觉评审尚未完成，因此该 P0 条目
保持进行中。Godot 的 Soft Light 明确在 tone mapping 后执行，不能在独立 glow shader
里伪装补齐；五种模式需由 glow/display 联合执行合同保证正确颜色阶段。
Tone mapping 核心已通过 8x HDR 的四种映射实际 D3D 回读，来源与适配记录见
`provenance/color_pipeline.json`；彩色阶梯和其余作品迁移仍待完成。透明排序的第一步
已通过 Null 合同和实际 D3D 像素验证，但相交几何、材质优先级和 depth prepass 尚未完成。
景深等条目尚未
因列入本文而视为完成。每项关闭时补充固定
上游 revision、文件哈希、许可、适配差异、GPU 图像与动态作品证据。
