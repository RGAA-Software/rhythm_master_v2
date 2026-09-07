# 流光云海 / Aurora clouds

基础模板候选，视觉验收待完成。双层四阶分形噪声缓慢演化，经过颜色调整和柔光
合成形成青绿与紫色云层。没有音频输入要求，也不依赖图片或视频素材。

选择“流光云海”组件即可调整噪声密度、运动速度、主色、对比度和光晕强度。
进入组件内部可修改第二层的速度、密度及颜色。主画布只保留组件与输出两个节点。
工程和发布包使用普通节点格式，未在应用中硬编码此场景。

空间噪声与高斯模糊改编自 TiXL 的 MIT 实现；准确版本、原始文件、改动和许可见
`provenance/tixl_effects.json` 与 `third_party/notices/tixl-effects/`。
当前 Windows 可运行；Android 编译与实机验证状态以验证记录为准。

Basic candidate, pending visual acceptance. Two evolving four-octave noise fields,
color adjustment and soft glow create a cyan/violet cloud composition. Select the
component for scale, motion, palette, contrast and glow controls. No media assets
or audio are required. Enter the component to edit the secondary field.
