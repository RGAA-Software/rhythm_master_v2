# GPU 粒子柔化与星铸圣坛复核（2026-09-11）

## 用户报告与原因

用户检查高级作品缩略图时指出，粒子与光斑放大后像生硬的大色块，缺少自然的光核和衰减。
问题不属于某一个模板：`gpu_point_fragment.sc` 此前在整个圆形内部使用恒定亮度，只有圆周处
通过 `smoothstep` 截断。所有 `gpu.render` 的点精灵都会继承这个外观。

## 修复

- 公共 GPU 点片段着色器改为高斯亮核与羽化边缘的乘积；输出保持预乘 Alpha，兼容 source-over
  与 additive 合成。
- D3D 像素回归 `SoftParticleFalloff` 创建单个固定粒子，验证中心、中圈、边缘严格递减，防止
  以后又退回为恒亮圆盘。
- `星铸圣坛 / Astral Forge` 使用每秒发射的双层 GPU 粒子，每层先独立模糊合成光晕，再叠加原始
  细粒子。用户复图后发现第二层的频段尺寸倍率和 5 像素模糊仍把星尘扩成光斑，因此改为细尘尺寸、
  有界倍率和 1.5/2.3 像素柔光；三维主体保留柔光，轨道航标缩小为背景节奏标记。

## 实际 Windows 证据

- `out/soft-particles-gpu.log.runs/1789089089087132800.log`：D3D11 GPU 点检查通过；固定
  粒子读回为中心 251、中圈 29、边缘 0，满足连续衰减检查。
- `out/astral-forge-thumbnail.log.runs/1789090826546486000.log`：实际 Windows 渲染包生成细尘
  版本缩略图；原始 TGA 位于 `out/catalog-thumbnails/7910259c82a04dc795d85ce2f5b00d28/astral_forge/`。
- `out/astral-forge-quality-gpu.log.runs/1789090864783395300.log`：当前 285 指令包在真实 PCM、
  静音、低/中/高频与响度夹具下通过；低/静音 2.2852，高/静音 2.9011，低/高 2.4670。
- `out/astral-forge-controls-gpu.log.runs/1789090897755379000.log`：六个公开控件极值均有非零
  图像差，范围 0.5303–29.1384。
- `out/windows-release/studio-delivery-tests.log.runs/1789090639431426500.log`：当前源完成
  Studio/Player deploy，并通过 9/9 交付检查；模板应用检查走的是实际中英文 Studio UI。

这些检查证明当前 Windows 路径的外观梯度、音频输入和控件可达；不把缩略图或像素差当作用户的
最终视觉验收，也不替代连续运动、多周期和 Android/长稳验收。
