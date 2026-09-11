# Godot Glow 核心验证（2026-09-11）

## 变更

- 新增独立 `texture.glow`，普通 `texture.blur` 不再承担 bloom 语义。
- 按固定 Godot 提交 `cb41ea115914c61a8329087b4cffbad7477b8427` 适配首次 HDR
  高光筛选、逐级降采样、八 tap 上采样和最终加法合成。
- 中间层和输出均为 RGBA16F；加法辉光只写 RGB，保留原图 alpha。

## 验证路径

- `blur` 与 `render_contracts`：验证节点图执行、固定 float16 输出、资源复用释放、非法参数
  和 effect 冲突。日志：`out/godot-glow-contract.log.runs/1789110238537258500.log`。
- `windows_effects_gpu`：通过真实 D3D 渲染目标回读断言中心高光保留、近远辉光连续衰减、
  透明区域 alpha 不受加法辉光污染、画布角落不泄漏。日志：
  `out/godot-glow-gpu.log.runs/1789110423394034900.log`。
- 同次检查纠正了遗留 Python 包装器对旧 blur 宽核足迹的错误假设，使外部截图检查与
  Godot 13-tap 内部回读使用同一标准。
- 完整 Windows delivery 在无操作增量构建中通过 9/9：真实 Studio 模板应用、节点 ID
  重映射、当前输出、保存重开、发布、双语 GPU 路径和大型音乐工程均通过。日志：
  `out/windows-release/studio-delivery-tests.log.runs/1789110950622499800.log`。Studio/Player
  的 `deploy` 各重新同步 20 个 DLL 和全部运行资源。

首次完整回归暴露 `official.glow.default` 额外覆盖 `bloom_floor`，违反“default 必须等于
节点声明默认值”的内容契约。根因是把推荐风格参数放进了默认预设；已将默认预设恢复为空
覆盖，把增强参数仅留在“霓虹远辉”，并通过聚焦内容契约与随后完整 delivery。失败日志：
`out/windows-release/studio-delivery-tests.log.runs/1789110743501561600.log`。

## 尚未关闭的范围

核心 glow 算法和节点可用。现有作品从 blur 拼装迁移到 glow、Godot tone mapping、完整
合成模式和动态作品视觉评审仍属于后续交付，不把本次核心测试当作作品质量验收。
