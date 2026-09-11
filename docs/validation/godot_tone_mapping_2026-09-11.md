# Godot Tone Mapping 验证（2026-09-11）

`texture.display` 从单一 TiXL Reinhard 扩展为固定 Godot 提交
`cb41ea115914c61a8329087b4cffbad7477b8427` 的 Reinhard、Filmic、ACES 和 AgX/AllenWP。
节点公开参考白点与 AgX 对比度；曝光先作用于线性 HDR，tone mapping 后才转换到 sRGB。

Windows D3D11 GPU probe 把 8x HDR 值依次通过四种映射并读取最终 RGBA8，验证各曲线的
固定输出、灰阶中性、彩色通道次序和 alpha 保留；同次运行还验证 glow→AgX 串联后零
覆盖区域的发光 RGB 不会消失。最终日志：
`out/godot-tone-glow-gpu.log.runs/1789111950543156500.log`。

首次 AgX 回读得到洋红偏色。根因是直接照搬 GLSL 矩阵常量后仍使用列向量乘法，而 bgfx
跨编译到 D3D 的该矩阵声明要求行向量形式。永久测试新增灰阶三通道相等断言；改为
`mul(color, matrix)` 后得到中性输出并通过。此前两次失败日志保留在同一日志目录。

组合验证首次把 glow 直接送入 display 时，中心亮度断言仍要求 tone mapping 前的 255，
与 AgX 的预期压缩相冲突。检查截图确认中心、近辉和远辉保持严格衰减后，将永久断言改为
检查相对能量、透明 alpha 和无边界泄漏，不再把压缩后的合法亮度误判为算法失败。

本次证明核心曲线和后端矩阵语义正确。彩色 HDR 阶梯、glow 后接 display 的完整作品和
现有高级作品映射选择仍需视觉评审，不能由灰阶 probe 代替。

完整 Windows delivery 通过 9/9，覆盖实际 Studio 模板应用、ID 重映射、当前输出、保存
重开、发布、双语 GPU 和大型音乐工程；随后清理生成测试目录。日志：
`out/windows-release/studio-delivery-tests.log.runs/1789112082898221800.log`。Studio 与
Player 的 `deploy` 已同步最新可执行程序、20 个 DLL 和全部运行资源。
