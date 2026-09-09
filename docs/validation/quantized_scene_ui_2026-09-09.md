# 量化队列入口回归（2026-09-09）

P1.3 开发中，`windows_player_smoke` 通过，但 `scene_queue_ui` GPU 界面回归崩溃。
这不是用户提交的新故障；是交付前自动检查发现，未作为通过版本交付声明。

变更前按钮调用 `TakeReady()`，空队列返回空值；接入量化请求时改为读取队首稳定 ID，
遗漏操作入口的就绪检查，仅依赖 ImGui `BeginDisabled`。测试第 5 帧主动投递
`ActivateItemByID` 到禁用 Go，下一帧仍可能触发 Button 返回，空队列 `front()` 越界。
诊断日志确认第 6 帧、queued=0 在 Draw 内崩溃；不将其误报为渲染能力或 GPU 驱动问题。

修复：计算 `can_go` 并同时用于按钮禁用状态和业务操作条件；执行前确认当前场可准备、
队列非空且队首已就绪。保持原有“禁用 Go”回归，继续投递延迟激活；不移除失败用例。
检查还要求量化开始时间在目标拍点后的首个正常显示帧，最后确实完成 GPU 场景接管。

失败证据：`out/p1-windows-player-beat-tests.log`、`out/p1-scene-ui-diagnostic.log`，
各次日志及命令由 Windows 串行检查器保留，不以成功覆盖失败证据。
修复后 `scene_queue_ui` 与 `windows_player_smoke` 同轮通过（3.36 秒），日志
`out/p1-windows-player-beat-fixed-tests.log`。Player 已自动部署完整 DLL 与资源。
