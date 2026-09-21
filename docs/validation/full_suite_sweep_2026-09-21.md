# 全量套件清扫与三处遗留修复（2026-09-21）

W1.5c 新增测试触发全量套件（297 项）运行，暴露并修复三类问题。交付门禁
（build-windows.py 的 9 项）均不覆盖以下测试，故存活至今。

## 1. 阴影探针对齐敏感性（详见 shadow_alignment_hardening_2026-09-21.md）

主机环境漂移使阴影边缘亚纹素位移；方向光 PCF 分级的绝对中间值区间与
点阴影接缝的固定采样像素随之失效。已改为相对亮/暗电平区间与 ±3 像素
过渡带窗口扫描。

## 2. `source_boundaries`：DoF 适配器未登记

`bgfx_depth_of_field.cpp/h`（647c75d，09-17）是 bgfx 私有适配器但未加入
`tools/check-boundaries.py` 的 `RENDER_ADAPTERS`，其 bgfx/bx/imgui 示例
着色器 include 被判越界。已登记，与其他 bgfx_* 适配器同类。

## 3. 音乐 GPU 测试预期过期

- `sonic_enamel_music_gpu`：647c75d 给模板加 DoF 节点（可达指令 64），
  但 CMake 的 `--expected-nodes` 停留在 63。已更新为 64。
- `aurora_braid/torque_garden/spectral_foundry_music_gpu` 的
  `music.texture_growth`：非运动场景 124 帧中 seconds 在帧 120 封顶，
  尾部 121-123 帧 elapsed=0。W1.5a 的 trail 修复（停顿保持历史、不再
  分配新输出纹理）使这三帧纹理字节合法下降，与"帧 30 后字节恒定"的
  泄漏检查冲突。泄漏检查现只覆盖时间推进帧（帧 ≤120 / 运动 ≤960）。

## 遗漏的测试路径与永久检查

- 全量套件与交付门禁的差异是本次三类遗漏的共同通道：09-17 的 DoF 提交
  只跑交付门禁。今后涉及渲染器/内容结构的提交，至少运行一次
  `verify_windows.py --match "."` 全量套件（本次证据：
  `out/validation/w15-full-suite.log`、两轮重跑 `w15-recheck*.log`、
  音乐复验 `w15-music-recheck.log`）。
- 音乐测试的纹理字节泄漏不变量以"时间推进帧"为适用域；冻结时间尾部
  帧的合法分配差异不再误报。
