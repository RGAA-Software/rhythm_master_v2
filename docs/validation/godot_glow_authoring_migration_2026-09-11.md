# Godot Glow 作品迁移验证（2026-09-11）

`Vector Resonance`、`Spectral Corolla`、`Glyph Current` 和 `Phase Plumes` 已把承担 bloom
语义的 `texture.blur` 替换为独立 `texture.glow`，并在最终输出前增加 AgX
`texture.display`。普通 blur 继续只表示空间模糊。

## 用户路径与永久检查

- 四个作品均从空工程通过 Studio UI 逐节点创建、设置属性与命名连接，然后操作输出画布、
  保存、重开、发布，并以 silence、low、high 和完整音乐输入运行发布包。
- 新增编辑域回归：`affine → glow → display → output` 中，保持画布坐标的后处理不会阻断
  上游 affine 的输出画布拖动。blur、glow、FXAA、linearize、display、color adjust 和
  contours 现在共享这条坐标保持规则。
- 音乐 GPU 验收把 `texture.glow` 纳入高级图像处理计数，并在失败前打印可达指令数、预期
  节点数、音频频段和主要渲染能力计数，避免只有笼统错误码。

聚焦画布路由测试通过：
`out/canvas-postprocess-route.log.runs/1789112814951658800.log`。
前三个作品的完整 Studio 路径通过：
`out/godot-glow-authoring-migration-fixed.log.runs/1789112859808852100.log`。
`Vector Resonance` 修正验收器后的独立完整路径通过：
`out/godot-glow-vector-final.log.runs/1789113265437068600.log`。

## 本次捕获的回归

首次运行时，四个作品在末端增加 display 后，输出画布检查把该节点视为不支持的路径，
没有创建鼠标捕获层；测试点击落入后方节点画布，选择变成 display，导致 affine 位移没有
保存。根因是坐标保持后处理列表没有随新节点扩展。失败日志：
`out/godot-glow-authoring-migration.log.runs/1789112437625705600.log`。

修复路由后，`Vector Resonance` 的 24 个节点全部可达，但音乐验收器仍只把旧 Gaussian
blur 计入图像处理能力，因此错误报告 `music.full_graph_not_reachable`。诊断确认实际为
24/24 可达、2 个音频频段；更新能力计数后四种音频输入均通过。失败日志：
`out/godot-glow-authoring-migration-fixed.log.runs/1789112859808852100.log`，修复后的独立
音乐日志：`out/vector-music-glow-fixed.log.runs/1789113243951735200.log`。

最终完整 Windows Release delivery 通过 9/9，覆盖真实模板应用、节点 ID 重映射、当前
输出、保存重开、发布、双语 GPU 路径和大型音乐工程。日志：
`out/windows-release/studio-delivery-tests.log.runs/1789113428801094600.log`。Studio 与
Player 的 `deploy` 均已同步最新可执行程序、20 个 DLL 和完整运行资源；测试产生的临时
目录在验收后由交付脚本清理。

这些检查证明作品可由真实 Studio 流程重建并运行新的 glow/display 图。动态审美、不同
音乐材料下的辉光强度和合成层次仍需移动画面评审，不能由结构及固定 GPU 断言替代。
