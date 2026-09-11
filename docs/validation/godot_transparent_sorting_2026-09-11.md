# Godot 透明物体排序第一增量（2026-09-11）

## 问题与实现

场景运行时原先以实例矩阵的原点计算透明深度。同一原点下，只要两个网格的顶点位于不同
局部深度，排序就退化为作者插入顺序，远处物体可能覆盖近处物体。

对照固定 Godot 提交 `cb41ea115914c61a8329087b4cffbad7477b8427` 的 forward mobile
render list，本增量保留不透明物体先提交、透明物体稳定后到前排序，并改用变换后的网格
包围盒中心。透视相机使用中心到相机的距离，正交相机使用视空间深度。上传缓存保存静态
网格中心，不在每帧重新扫描顶点；渲染公共 API 没有暴露 Godot 或 bgfx 类型。

第二增量把 Godot 的两个显式排序控制接入可编辑图合同：PBR/unlit 材质提供整数
`render_priority`（-128..127），低优先级先绘制；`scene.instance` 提供
`sorting_offset`，从相机距离中扣除。两个值随节点、运行快照、保存和发布传递，默认值
均为 0，因此旧作品排序保持兼容。

具体来源、文件哈希和适配范围记录在 `provenance/godot_3d.json` 的
`transparency_sorting_reference`。

## 验证

- `scene_runtime` 构造两个实例原点完全相同、局部网格中心分别位于近处与远处的半透明
  立方体，并反向安排作者顺序；透视和正交相机均断言蓝色远物体先于红色近物体。
  日志：`out/godot-transparent-sort.log.runs/1789114206544786800.log`。
- `windows_scene_gpu` 通过实际 D3D11 渲染同一场景并读取中心像素，得到近红覆盖远蓝的
  预乘 source-over 结果 `(128, 0, 64)`。日志：
  `out/godot-transparent-sort-gpu.log.runs/1789114367664299100.log`。
- 第二增量的 `scene_runtime` 同时验证节点属性进入材质/实例快照、priority 优先于距离、
  sorting offset 改变透明次序；D3D11 分别读回 `(64, 0, 128)`，证明两个显式控制确实
  改变最终像素。聚焦日志：
  `out/godot-transparent-controls.log.runs/1789115153242164300.log`。
- 完整 Windows Release delivery 通过 9/9，Studio 与 Player 的 deploy 均同步最新
  可执行程序、20 个 DLL 和运行资源；测试临时目录随后自动清理。日志：
  `out/windows-release/studio-delivery-tests.log.runs/1789114484917171300.log`。

第二增量首次完整交付时，内容合同发现 `scene.instance` 从无属性节点变为可调节点后缺少
默认预设。其余 8 项均通过；新增 `official.scene_instance.default` 后，聚焦内容合同与
完整 9/9 delivery 均通过。失败日志：
`out/windows-release/studio-delivery-tests.log.runs/1789115284721480300.log`；最终日志：
`out/windows-release/studio-delivery-tests.log.runs/1789115526909697400.log`。两个 deploy
再次同步最新程序、20 个 DLL、双语文本和预设资源。

本增量修复可由物体级排序正确表达的透明场景。相交三角形仍不能靠一个物体中心得到逐像素
正确前后关系；骨架、morph 和顶点变形当前使用静态网格中心。后续继续评估可选 alpha
depth prepass，不把本结果写成通用 OIT 支持。
