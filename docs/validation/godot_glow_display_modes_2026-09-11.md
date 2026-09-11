# Godot Glow/Display 联合合成验证（2026-09-11）

新增 `texture.glow_display`，把 Godot dual-filter glow 与最终显示映射放进一个明确的颜色
阶段合同。节点提供 Add、Screen、Soft Light、Replace、Mix 五种模式，以及曝光、Reinhard、
Filmic、ACES、AgX、参考白点和 AgX 对比度。旧 `texture.glow` 与 `texture.display` 保持兼容。

实现固定参考 Godot 提交 `cb41ea115914c61a8329087b4cffbad7477b8427` 的
`servers/rendering/renderer_rd/shaders/effects/tonemap.glsl`，文件 SHA-256 为
`266774b9f98a798f2f7f4c8388598f322723c1cfc2813975ed8a3153c6fd68a8`。Add、Screen、
Replace、Mix 在 tone mapping 前合成；Soft Light 将源图和 glow 分别 tone mapping 后再合成。
共享 `godot_tonemap.sh` 保证独立 display 与联合节点不会复制出两套曲线。

Windows D3D11 实际 GPU 回读使用线性 0.25 源图、线性 0.5 glow、0.5 glow 强度和 Reinhard
到 sRGB 显示。五种模式按 Godot 枚举顺序得到灰阶值 156、156、137、124、143，三通道
误差不超过 2，alpha 保持 255；这些数值已成为永久断言，可发现 Soft Light 被错误前置、
Mix 权重错误或显示转换次序变化。带固定断言的直接运行日志：
`out/godot-glow-display-asserted.log.runs/1789118704441532600.log`。

渲染合同、runtime pass 和图执行聚焦测试通过；日志：
`out/godot-glow-display-focus.log.runs/1789118504372115200.log`。完整 Windows delivery
通过 9/9，覆盖实际 Studio 模板应用、节点 ID 重映射、当前输出、保存重开、发布、双语
GPU 与大型音乐工程；日志：
`out/windows-release/studio-delivery-tests.log.runs/1789118786360311200.log`。Studio 与
Player deploy 均由构建脚本刷新为最新可执行程序、20 个 DLL 和全部资源。动态视觉评审
和其余作品迁移仍属于 glow P0 的后续工作，不能由本次像素合同替代。
