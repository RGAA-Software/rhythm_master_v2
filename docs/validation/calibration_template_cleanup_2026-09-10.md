# 高级模板清理后的校准回归

范围：Windows Studio 交付检查与内容质量账本。本次不改变图执行、渲染或音频
运行时。

删除简单模板后，`calibration_controls_gpu` 仍在模板切换循环中对已删除作品后的
无公开控件作品执行滑块校验，报出 `calibration work has no public controls`。
这不是控件或保存功能失效，而是测试夹具没有随目录清理更新。此前完整交付构建
因此为失败，不能视作交付成功。

`template_switch_gpu_tests` 现在仍逐个实际应用全部高级模板，检查 ID 重映射、
当前输出、保存、重开和发布；带 `--controls` 的附加校准路径只选择公开声明控件的
`chromatic_loom`。这使“所有作品可应用”与“鼠标控件编辑”成为各自明确的判据，
不会把无控件作品当成失败。

- `out/calibration-controls-fixed.log`：`calibration_controls_gpu` 通过，5.93 秒；
  实际鼠标拖动两个公开控件的上下限、保存、重开与发布均通过。
- `tools/build-windows.py` 的交付必测集合仍包含该 CTest；后续 no-op delivery build
  也会再次执行它。

同次内容质量索引发现五条评审记录仍指向已删除的 `ink_tide` 与
`porcelain_pendulum`，使 `audit-content-quality.py` 拒绝生成索引。只删除这些没有
对应源文件的过期记录，随后重建并以 `--check` 验证索引。当前清单为 20 个高级
模板、30 个组件、209 条预设记录；质量接受数仍为零，未把功能测试当作视觉或用户
验收。

## 全部高级模板应用回归

扩大切换清单到 20 个保留的高级模板后，旧测试把多个模板放进同一 Studio 进程。
第 17 个作品会因历史实例的异步资源尚未回收而触发 `async.pool_limit`；即使改为
独立工程，`phase_loom` 在节点实时预览需求变化时仍会让预览根不断覆盖待安装的
编译代次。此前只验证部分清单，因此没有发现这个路径。

永久检查改为由 `test-template-switch-gpu.py` 从运行时资源清单读取每个 Advanced
模板，并为每个模板启动独立 Studio 进程。每一项都仍通过模板浏览器搜索和“使用”
按钮应用，检查 ID 重映射、当前最终输出、保存、重开、发布、控件／Cue／节拍网格
引用和运行包指令数。模板应用回归关闭节点实时缩略图，避免预览需求改变不断取消
待安装的最终输出；实时节点预览继续由专用 GPU 回归覆盖。

- `out/template-switch-all-final.log`：英文和中文各 20 个高级模板全部通过，
  共 94.75 秒。
- 单独复现和修复确认：`out/phase-zh-preview-off.log`，中文“相位织光”从默认
  Studio 工程应用、保存和发布通过。
- 此入口继续保留在 `tools/build-windows.py` 每次 Studio delivery 的强制集合中，
  no-op 构建也会执行，不再以部分模板或直接包播放作为成功证据。
