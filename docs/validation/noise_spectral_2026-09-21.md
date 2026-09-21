# 噪声频谱/domain warp/带限验证（2026-09-21）

W1.5b：对照 Godot 4.5.1 FastNoiseLite 与 TiXL 上游 PerlinNoise2d（溯源
`provenance/noise_spectral.json`）扩展 `texture.noise`：domain warp、
可配置频谱层（octaves/roughness）与可选带限抗走样，默认设置位精确不变。

## 审计缺陷

1. **放大暴露网格**：固定 4 octave 平铺 Perlin 在放大（scale 小、目标大）
   时晶格对齐肉眼可辨。
2. **频带单一**：octave 数与增益写死，作品无法调节细节层次。
3. **无抗走样**：scale 大（晶格小于 2 像素）时高 octave 走样为闪烁颗粒。

## 修复

- `renderer.h`/`registry.cpp`/`texture_ops.cpp`/`resource_table.cpp`：
  `texture.noise` 新增 `octaves`(1-8, 默认 4)、`roughness`(0-1, 默认 0.5)、
  `warp`(0-4, 默认 0)、`noise_filter`(0-1, 默认 0) 属性，含边界校验与
  中英文标签。
- `texture_noise.sc` + `bgfx_texture_programs.cpp`（新 uniform
  `u_noise_spectral`，`u_noise_domain.w` 携带基频每像素晶格数）：
  - domain warp：两个去相关单 octave 晶格采样位移基础域（Godot
    FastNoiseLite 域扭曲组合的对应物），warp=0 时完全跳过；
  - 频谱层：octave 循环上限 8、按 `octaves` 截断、增益按 `roughness`
    衰减（TiXL Iterations/Gain 语义）；
  - 带限：`noise_filter=1` 时晶格 footprint 越过 2 像素的 octave 经
    smoothstep 淡出（权重同时作用于累加与归一化），默认关闭。
- 未采用项（结论已记录在 provenance）：更换基础原语为 FastNoiseLite
  Perlin、cellular/worley 类型、ping-pong 增益——既有平铺晶格被已发布
  作品内容锁定，位精确默认输出是硬约束。

## 实际验证路径

- 位精确兼容（git stash 对照）：旧着色器基线捕获与新增功能后默认参数
  捕获逐字节一致（noise-0..4 全部相同），证明默认路径零回归。
- `python tools/verify_windows.py --match "^(trail|texture_ops|windows_effects_gpu)$" ...`
  日志 `out/validation/w15-noise.log`：
  - `texture_ops`（空渲染器契约）：新属性默认值与图属性提取断言通过；
  - `windows_effects_gpu`（D3D11 实际像素，场景 5-11）：
    - 场景 5（octaves=1）：二阶曲率能量显著低于默认（11286 vs 33792，
      约 1/3），确认少 octave 更平滑；
    - 场景 6（octaves=8, roughness=0.8）：曲率显著高于默认（>2×），
      确认频谱扩展更粗糙；
    - 场景 7（warp=2）：与默认逐像素平均绝对差 >8 且保持全量程
      （max-min ≥ 50），确认晶格可见位移且未塌缩；
    - 场景 8/9/11（scale=8 带限）：第四 octave 达每像素一晶格走样，
      滤波后输出与显式 octaves=3 渲染**逐像素完全一致**，且与未滤波
      输出不同、曲率严格更低；
    - 场景 10（默认 scale=4 开滤波）：与场景 0 逐像素一致，确认带限
      在量程内零副作用。

## 遗漏的测试路径与永久检查

- 此前噪声只有范围/周期截图断言，无频谱与走样的像素级回归——本次补齐
  为永久回归（`effects_gpu_tests.cpp` 场景 5-11 + `test-effects-gpu.py`
  曲率/逐像素断言）。
- 一阶差分能量对"少 octave 更平滑"不敏感（低频大梯度主导），二阶曲率
  能量才是高频内容的可靠度量；场景 5 初判失败后已固化该口径。
- 带限有效性不以能量阈值间接推断，改为与显式 octaves=3 渲染逐像素
  对比——直接证明"恰好移除走样 octave"。
