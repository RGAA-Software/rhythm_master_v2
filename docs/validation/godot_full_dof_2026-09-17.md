# Godot 完整景深验证（2026-09-17）

`texture.dof` 已从全分辨率单 pass Circular gather 升级为 Godot 4.5.1 的完整 bokeh
结构：Circle、Box、Hexagon，Very Low 至 High 四档质量，有符号 R16F CoC 权重，
Box/Hex 两次分离卷积，以及半分辨率最终权重合成。固定来源、修订、哈希和图目标适配
见 `provenance/depth_pipeline.json`。

后端按已见尺寸缓存 7 张私有临时纹理：全分辨率原始权重 R16F、全分辨率
RGBA16F+R16F，以及两组半分辨率 RGBA16F+R16F，总计 `17 × width × height`
bytes。Circle 为 3 pass；Box/Hex 的 Very Low/Low 为 4 pass，Medium/High 为 3 pass。
这些值由渲染统计直接断言，并与图纹理共同接受 256 MiB 纹理预算。640×360 的实际作品因此稳定增加 3,916,800 bytes，测得
点阴影路径 74,301,892 bytes、无阴影路径 23,970,244 bytes。

Windows D3D11 固定探针覆盖：

- Circle Very Low/High、Box Very Low/Medium、Hex Low/High，确认各质量结果不同；
- 焦平面棋盘、离焦对比度、有符号近远景边界和预乘半透明 alpha；
- 64×32 横屏与 32×64 竖屏的半分辨率 Box 和全分辨率 Hex；
- 3/4 pass 与 17 bytes/像素资源预算；
- 全套渲染探针未回归。

日志：`out/dof-full-final-gpu.log.runs/1789577793786587400.log`。

“音律珐琅”改用 Hexagon Medium，并新增静音下持续往返的相机横移；真实发布图在
640×360 连续运行 121 帧，同时保留细金属轨道、远景亮部、旋转环境和点阴影。
日志：`out/dof-hex-dynamic-camera-sonic-enamel.log.runs/1789577322202996500.log`。

Windows SM5 与 Android GLES 3.0 的四个新 shader 均由仓库内已验证 `tools/shaderc.exe`
编译通过。当前阶段没有 Android 设备和可用 NDK，实机读回按执行计划留到最终平台验收，
不能由编译结果替代。
