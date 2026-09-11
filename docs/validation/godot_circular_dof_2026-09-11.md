# Godot Circular DOF 验证（2026-09-11）

`texture.dof` 的黄金角采样预算继续沿用现有 1–64 样本合同，近远景权重改为固定 Godot
提交 `cb41ea115914c61a8329087b4cffbad7477b8427` 的 circular bokeh 语义：近景 CoC 为负、
远景 CoC 为正，跨焦平面采样使用 Godot 的有符号遮挡限制。参考文件
`servers/rendering/renderer_rd/shaders/effects/bokeh_dof_raster.glsl` 的 SHA-256 为
`9a333542197d3dafe2f9d8171789128e22f6b15eade71a3098a8c16d7322ba5a`，许可为 MIT。

Windows D3D11 增加真实深度边界：左半平面位于距离 2、右半平面位于距离 8，焦点为 4，
输入分别为红色和蓝色。距离边界及边界两侧四个采样点的 R/B 值固定为
`173/82`、`135/119`、`118/137`、`80/175`（容差 3），同时要求近侧红色、远侧蓝色
在接缝两边保持主导。日志：
`out/godot-circular-dof-asserted.log.runs/1789119385028689800.log`。

本增量只关闭 circular 单 pass 的有符号 CoC 与边界遮挡缺口。Godot Box/Hex 形状、
half-size 质量档、独立权重缓冲与最终 composite 仍未实现，继续保留在画质审计 P1。

完整 Windows delivery 通过 9/9，覆盖实际 Studio 模板应用、节点 ID 重映射、当前输出、
保存重开、发布、双语 GPU 与大型音乐工程；日志：
`out/windows-release/studio-delivery-tests.log.runs/1789119411983893800.log`。Studio 与
Player deploy 均已刷新为最新可执行程序、20 个 DLL 和完整资源。
