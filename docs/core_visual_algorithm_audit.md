# 核心画面算法审计

日期：2026-09-17。本文回答“哪些画面问题能在用户截图前被发现”，并规定替换顺序。
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
| P0 | glow/bloom | `texture.glow` 已完成 Godot Dual Filtering；`texture.glow_display` 已联合执行 Add、Screen、Soft Light、Replace、Mix 与显示映射，并以 D3D11 像素锁定 Godot 的前后置顺序 | 继续迁移其余作品并完成大粒子、细亮线、HDR/SDR 与动态输出评审 |
| P0 | tone mapping/HDR | `texture.display` 已提供 Godot Reinhard、Filmic、ACES、AgX，曝光在映射前、sRGB 转换在映射后；中性与彩色 8× HDR 阶梯已通过实际 D3D11 回读，首批四件作品已接入 AgX | 完成 glow→tone-map 作品动态评审，再决定高级作品的默认映射 |
| P0 | 透明 3D | 已按 Godot 语义采用不透明优先、稳定后到前排序、材质 priority、实例 sorting offset，并完成可选 alpha depth prepass；接近不透明的贴图覆盖写深度，透明孔洞保留后层 | 普通混合仍是顺序相关 source-over；复杂 OIT 单独验证后决定，不把 depth prepass 当作通用逐像素透明排序 |
| 已交付 | 景深 | Godot Circle/Box/Hex、四档 quality、半/全分辨率路径、独立 R16F 权重、分离卷积和最终 composite 已接入；旧 samples 文档兼容 | D3D11 已覆盖近景细边界、远景颜色、透明 alpha、横竖画布和动态相机作品；Android 实机复验留在最终平台阶段 |
| P1 | 阴影 | 已采用 Godot Nearest/PCF5/PCF13、稳定方向光投影、可选双级联和 cube 模式点光全向阴影；越界 PCF tap 会切换相邻面，全部 12 条 edge/8 个 corner、六方向、子网格、Porcelain Bloom、Chromatic Loom 和 Sonic Enamel 均有 D3D11 回读；点光六遍深度已有逐 view GPU 计时，仍只有单选择光源 | 继续评审与 Godot 硬件 cube 比较的差异、Android 实机、代表作品和多周期；四级联/分割混合按实际作品缺口再决定，保留 PCF5 和单图作为兼容默认档 |
| 已交付 | 环境预滤波 | Godot 8/16/32/128 分层采样、平方 GGX 分布、`sqrt(roughness)` LOD、反射弯折与 horizon 衰减已接入现有线性图集；恒定环境保持能量 | 固定 HDR 阶梯 D3D11 回读为 192/185/110/22/9，Sonic Enamel 动态相机 121 帧通过；单一 2D 来源缺少 Godot cubemap mip/PDF，Android 实机复验留最终平台阶段 |
| 已交付 | 粒子呈现 | 解析圆点连续柔边之上已接入纹理图集与每粒子形态变化（发射稳定随机选格、采样形状/色调调制解析包络、未接图集逐像素不变）、软深度交界（Godot proximity fade 良定义等价式，场景距离减粒子深度按带宽 smoothstep 衰减）和分层体积感；鎏光流涡细尘层置于旋涡臂后方、火花层置于前方，2×2 图集提供圆点/圆环/交叉条纹/火花四种形态 | bgfx 精灵表与 Godot 粒子材质变化、proximity fade 已聚焦采用并记录 provenance；Windows D3D11 探针数值、音乐/静音/低/高频像素分工与作品切换资源回归通过，Android 实机复验留最终平台阶段 |
| 已交付 | trail/feedback | 停顿按封顶 250 ms 衰减步收敛（不再重种子跳变），反向/暂停保持像素；旋转/缩放历史在画面边缘两 texel 淡出，单位变换零调光；30/60 fps 衰减一致性、停顿连续性和边缘淡出均有 D3D11 回读断言 | 对照 Godot 4.5.1 TAA 去遮挡收敛原则（`provenance/trail_temporal.json`）；运动矢量重投影/方差裁剪/Catmull-Rom 对刻意残影效果无作品收益，结论已记录 |
| 已交付 | 噪声与纹理生成 | `texture.noise` 已接入 domain warp（去相关双采样域位移）、可配置频谱层（octaves 1-8 / roughness 0-1，TiXL Iterations/Gain 语义）和可选带限抗走样（晶格 footprint 越 2 像素的 octave 经 smoothstep 淡出）；默认参数与旧实现逐字节一致 | 对照 Godot FastNoiseLite 与 TiXL 上游（`provenance/noise_spectral.json`）；带限与显式 octaves=3 渲染逐像素一致、量程内零副作用均有 D3D11 回读断言；cellular/worley 与更换基础原语无作品需求，结论已记录 |
| P2 | 抗锯齿 | `glsl-fxaa` 是成熟实现且两端有像素证据，但单一 FXAA 会软化细线，也不解决时域闪烁 | 记录 Godot MSAA/TAA/FSR 路径的能力和成本；Windows 先做细线运动与粒子闪烁对照 |

## 当前验收状态

普通 blur 已完成源码替换和完整交付，本项关闭。永久 GPU 检查读取实际渲染目标并断言：常量图
守恒、透明背景不泄漏隐藏 RGB、半径 1 命中 Godot 核的中心/邻域衰减、半径 8 的 mip
结果在 X/Y 方向近似一致且向外单调衰减、半径 0 保持原图；Studio delivery、实际
模板应用和 deploy 更新均已通过。

普通 blur 的最终交付证据、曾经漏掉截图的原因和永久检查见
[验证记录](validation/godot_blur_and_generated_cleanup_2026-09-11.md)。

Glow 核心节点已通过 D3D11 中心高光、连续远近衰减、边界和透明 alpha 回读；固定来源见
`provenance/godot_glow.json`。`texture.glow_display` 在一次显示执行中提供五种 Godot 合成
模式，Add、Screen、Replace、Mix 在 tone mapping 前执行，Soft Light 在源图与 glow 各自
tone mapping 后执行；五种模式均有固定 D3D11 像素断言。首批四件作品通过真实 Studio
重建、保存发布与音乐运行；其余作品迁移和动态视觉评审尚未完成，因此该 P0 条目保持进行中。
Tone mapping 核心已通过中性与彩色 8x HDR 的四种映射实际 D3D 回读，来源与适配记录见
`provenance/color_pipeline.json`；其余作品迁移仍待完成。透明材质的物体排序、
显式排序控制和 alpha depth prepass 已通过 Null 合同与实际 D3D 像素验证；任意半透明
相交面的逐像素排序仍未完成。
景深、环境预滤波与粒子呈现已按各自退出条件交付（2026-09-17），Android 实机复验统一
留在最终平台阶段；其余条目尚未
因列入本文而视为完成。每项关闭时补充固定
上游 revision、文件哈希、许可、适配差异、GPU 图像与动态作品证据。

阴影质量的第一批升级已完成：固定 Godot 4.5.1 `sample_shadow` 的 PCF13 样本位置和
提前返回，图/运行时/渲染公共合同采用三档枚举，旧 `0/1` 文档继续保持语义，默认仍是
PCF5。实际 D3D11 回读确认 Nearest→PCF5 与 PCF5→PCF13 均改变边缘；方向光现按 Godot
两 texel 中心网格稳定投影，矩阵回归覆盖正负边界、光照深度和不受影响的聚光路径。
两帧 D3D11 回读中，子网格移动输出差异为 0，未吸附对照差异为 7003。真实作品连续
移动短测又覆盖 Porcelain Bloom 当前编译作者图的 121 帧，连续差异没有突发跳变，阴影
对照与纹理稳定性通过；短 host-frame 记录未观察到可分辨的 PCF13 新增成本，但不是隔离
GPU 计时。Chromatic Loom 的 481 帧又确认 PCF5→PCF13 改变细线作品像素、投影范围
9→36 后阴影仍可见，且第 479→480 帧变化低于普通帧中位数。单个 16 秒边界不替代多个
完整作品周期。可选双级联随后按 Godot 视锥包围球与每级吸附贯通，Chromatic Loom
D3D11 对无阴影和单图均产生可测差异，资源精确增加一个 8 MiB 附件对；四级联/分割
混合、Godot 硬件 cube 比较对照和 Android 点光像素仍未完成，因此 P1 阴影条目保持进行中。
