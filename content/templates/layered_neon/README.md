# 分层霓虹 · 音频轨道 / Layered neon audio orbit

基础模板候选，视觉验收待完成。桌面 1280×720；Android 尚待验证。

主画布保留七个节点。先选中“霓虹核心”调整青/粉配色、运动速度、环线内径比
和频谱增益；选中“分层辉光”调整近光、远光半径及光晕强度；“星尘背景”控制密度。
组件均可进入内部图编辑，模板没有应用内专用场景代码。

在音频面板启用系统声音驱动后，真实频谱形成放射细线，RMS 轻微推动环线缩放。
关闭输入或静音时，频谱消失，环线和轨道继续缓慢运动，星尘继续漂移。
预览工具使用明确标注的合成音频特征，并不证明实时采集或歌曲解码已经验收。

这是辉光效果：对形状与频谱的透明纹理做近/远两层模糊，再加法合成。
它不是 HDR 高亮提取型 bloom；当前 RGBA8 高亮会限幅。模糊半径以短边 720 像素
为参考，预览和其他尺寸按比例缩放。降采样上限为六级，参数变化可能重建中间纹理。

Glow kernel: focused adaptation of TiXL's Gaussian and downsample shaders, MIT.
Exact imported originals, source revision and modifications are recorded in
`provenance/tixl_effects.json`; application bundles include the original license.
The template graph and its embedded components are first-party authored content.

Basic candidate, pending visual acceptance. Select Neon core for palette, motion
and spectrum controls; Layered glow for radius/intensity; Stardust backdrop for
emission density. Enable system audio in Studio to drive the actual spectrum.
Silence retains slow orbit motion and dust, without inventing audio activity.
