# trail/feedback 时域修复验证（2026-09-21）

W1.5a：对照 Godot 4.5.1 TAA（`taa_resolve.glsl`，溯源 `provenance/trail_temporal.json`）
修复 `texture.trail` 的回收边界跳变与快速运动覆盖断层，并补齐动态测试。

## 缺陷与根因

1. **停顿重种子跳变（回收边界）**：`TrailPass::Draw` 在帧间隔超过 250 ms 时
   丢弃历史、只绘制当前源——窗口拖拽、着色器编译等任何呈现停顿都会让拖尾
   瞬时消失。反向时间同样重种子。运动时间契约（`playback_clock.h`）保证纪元
   内单调、seek/load 提升 generation 并清除状态，因此停顿期间历史仍是最佳
   可用近似，硬切没有依据。
2. **覆盖边界硬切（断层）**：`texture_trail.sc` 用 `step` 覆盖掩码裁切旋转/
   缩放后的历史采样，拖尾在画面边缘被一刀切断，旋转作品（motion_echo
   -12°/s、dunhuang zoom 0.006）每帧都有可见的边缘切割。

## 修复

- `src/runtime/trail_pass.cpp`：停顿按封顶 250 ms 的衰减步收敛（Godot TAA
  去遮挡"快速但连续收敛"的对应物），不再重种子；反向/重复/暂停时间保持
  像素不变。generation 重置仍清空历史（seek/load 语义不变）。
- `src/rhythm_render/shaders/texture_trail.sc` + `bgfx_texture_programs.cpp`：
  历史覆盖边缘按两个 texel 的 smoothstep 淡出（新 uniform `u_trail_fade`）；
  变换为单位阵（无缩放旋转）时边距为 0，普通拖尾边缘零调光。
- 未采用项（结论已记录在 provenance）：运动矢量重投影、3×3 方差裁剪、
  Catmull-Rom 历史采样——拖尾是刻意残影效果，2D 纹理链无逐像素速度，
  双线性重采样模糊属作品美学，额外 pass 与速度缓冲无作品收益。

## 实际验证路径

- `python tools/verify_windows.py --match "^(trail|windows_effects_gpu)$" ...`
  日志 `out/validation/w15-trail.log`：
  - `trail`（空渲染器契约）：暂停/重复/反向保持像素、停顿后仍单步收敛、
    调整尺寸/旁路/重置/缓存与资源有界。
  - `windows_effects_gpu`（D3D11 实际像素）：
    - 既有 trail-0/1/2：30/60 fps 衰减率一致（128±3）与最终消失（0）；
    - 新增 trail-3/4（经 `TrailPass` 运行时组件 + 8 位回读断言）：2 秒停顿
      结果 =157，与精确 250 ms 稳态参考逐像素一致（±4）；旧实现此处为 0
      （重种子）；
    - 新增 trail-5（6°/帧旋转）：四角对角线淡出 [0, 1, 158, 254]，内部 254
      零调光；旧实现为 [0, 254, 254, 254] 硬切。
- `python tools/review-concept-motion.py --name dunhuang_ribbons`（实际作品
  回归，使用 `texture.trail` half_life 0.13 + zoom 0.006）：静音/音乐双场景
  961 帧循环边界与持续运动检查通过；证据
  `out/concept-motion/79abd4549195417fadea2fd15a720d12/`，联系人表已目检，
  拖尾完整、无黑帧无撕裂。

## 遗漏的测试路径与永久检查

- 此前 trail 只有空渲染器契约与着色器级衰减截图，没有经运行时组件的
  停顿/边界动态像素测试——本次补齐为永久回归（`effects_gpu_tests.cpp`
  场景 3-5 + `test-effects-gpu.py` 断言）。
- 截图捕获在后台缓冲尺寸切换帧不可靠（得到陈旧显存内容）：捕获前必须以
  相同尺寸重复呈现数帧，或以 8 位 inspection 纹理回读断言为准。该模式已
  固化在上述场景与既有 blur/glow 测试中。
