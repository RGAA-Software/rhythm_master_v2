# 图像抗锯齿

`texture.fxaa` 是可编辑、可发布的空间抗锯齿节点，放在 `texture.display`
之后、最终输出之前。Studio 的滤镜分类提供节点、双语帮助和实时预览；
Windows Player、Android Player 和离线导出使用同一图与渲染契约。

## 处理契约

- 输入预乘 RGBA，输出保持原有精度与颜色空间，不自动执行显示转换。
- 九次纹理采样按实际源尺寸定位；方向搜索跨度默认 8 像素，允许 1–16。
- 方向衰减默认 0.125，最小衰减默认 0.0078125；非法属性在编译或渲染边界拒绝。
- 强度默认 1，范围 0–1，也可接标量信号。接线值在运行时限幅；零强度保留原图。
- 同时过滤预乘颜色和透明度；透明覆盖率参与边缘方向，支持透明黑色轮廓。
- 无历史帧和额外播放时钟。沿用普通纹理目标池、静态缓存及尺寸失效规则。

FXAA 减轻空间锯齿，但不能恢复已经丢失的亚像素几何或保证消除动画闪烁。
它不是 MSAA、TAA 或超采样。Float16 输入可用不代表适合在 HDR 显示映射之前处理。

## 复用与依赖

先检查了本地 TiXL 和 Cables 的 FXAA 实现。Cables 操作元数据指向
`mattdesl/glsl-fxaa`；采用该独立源码包 3.0.0，revision
`5028eff0bc801aab51b884b27b47c313defa6a0c`。配置中的 vcpkg 无 FXAA/SMAA port，
现有 bgfx 源码快照也无可直接提取的同类 shader，因此只提取固定 shader 源码，
没有安装 npm 运行时依赖或引入第二个渲染库。

包级 MIT（Matt DesLauriers，2014）和算法文件 BSD-3-Clause
（Armin Ronacher，2011）都适用，完整通知保存在 `third_party/notices/glsl-fxaa`。
`tools/prepare-fxaa-source.py` 校验固定 SHA-512，并只解包六个允许的普通文件。
精确来源、每个文件哈希、修改及受影响的 Studio/Player 产物见
`provenance/fxaa.json`。项目的整体对外许可证没有因此被选定。

适配修改包括 bgfx sampler/uniform、实际源尺寸、可控强度及预乘透明边处理；
不透明 RGB 保留上游方向与候选选择公式。Windows `s_5_0` 和 Android `300_es`
使用同一 shader 源文件。Apple profile 尚未验证。

## 聚焦检查

两端真实 GPU 检查不透明斜边、彩色透明边、黑色透明边、均匀单像素源、
强度零恒等、能量误差、预乘约束与参数拒绝。图层检查覆盖发布往返、
接线强度、静态缓存、横竖尺寸变化和资源归零。
实际作品及设备记录见 [本批验证](validation/fxaa_2026-09-09.md)。
