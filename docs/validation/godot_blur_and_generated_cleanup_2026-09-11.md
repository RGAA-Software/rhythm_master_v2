# Godot 普通 Blur 与测试产物清理验证

日期：2026-09-11。用户要求普通 blur 也采用 Godot，并要求删除项目内已经使用完的
测试生成物，不再让它们占用资源或进入 deploy。

## 实现

- `texture.blur` 的 shader 改用 Godot 提交
  `cb41ea115914c61a8329087b4cffbad7477b8427` 中
  `blur_raster.glsl/MODE_GAUSSIAN_BLUR` 的 13-tap 二维核；采样位置、权重和 linear clamp
  语义保持一致。
- 运行时以半径选择最多六级的有界 mip 链，最后一级承接连续半径余量，再线性返回原
  画布尺寸。原 TiXL 横纵分离 Gaussian 和四 tap downsample 不再处于运行路径。
- GPU 测试读取真实 D3D11 渲染目标，检查常量守恒、透明隐藏 RGB 不泄漏、半径 1 的
  13-tap 邻域、半径 8 的 X/Y 径向衰减和半径 0 旁路。
- 模板合同的临时工程改到系统临时目录并由 RAII 删除。Studio 模板 GPU 验收成功时
  删除逐帧证据工程；失败日志仍由 `verify_windows.py` 单独归档。
- `deploy-windows.py` 明确排除并删除 `content/template-switch-tests`。完整 Windows
  delivery 在成功或失败后调用 `tools/cleanup-generated.py --delivery --apply`，清除可重建
  测试工程、截图、导出和 CTest 临时目录，保留增量构建缓存与归档日志。

完整来源、哈希、许可和适配差异见 `provenance/godot_blur.json`。TiXL 文件继续保留其
原始许可与历史来源记录，不再声称是当前 blur 实现。

## 发现并固定的漏测

第一次 GPU 程序打印“captures”并返回成功，但调用方没有先创建截图目录，实际没有
任何图片。这次结果未计为通过。测试现在自行创建输出目录，并以 GPU readback 像素
断言为主要证据，截图仅供查看。

第一次完整 delivery 在中文遍历的 `resonance_live` 默认 plan 等待阶段超时。检查发现
`out/windows-release/content/template-switch-tests` 累积到 5.10 GiB，并被复制到两个
deploy；整个 `out/windows-release` 一度达到 49.08 GiB。清理后单独复跑通过。等待逻辑
从固定 20 帧改为至少 20 帧完成 UI 初始化，随后按实际 plan 最多等待 180 帧。

等待逻辑的第一版错误地在 plan 提前有效时少于 20 帧退出，导致英文 Astral Forge 和
控件用例没有真正点击模板，诊断为 `nodes=0`。该次失败未解释成产品成功；恢复最少
初始化帧后，两个路径分别通过，随后完整 9 项 delivery 通过。

## 证据

- blur Null/资源合同：
  `out/godot-blur-contract.log.runs/1789106377766744600.log`。
- D3D11 blur readback：
  `out/godot-blur-gpu-assertions.log.runs/1789106805452199900.log`。
- 原始中文超时：
  `out/windows-release/studio-delivery-tests.log.runs/1789107787396409400.log`。
- 初始化循环回归：
  `out/windows-release/studio-delivery-tests.log.runs/1789108600409609000.log`。
- 干净目录最终 9/9：
  `out/windows-release/studio-delivery-tests.log.runs/1789108794583003400.log`。
- 自动清理钩子启用后的 no-op delivery 9/9：
  `out/windows-release/studio-delivery-tests.log.runs/1789109105118999700.log`；测试结束后
  自动删除 6 个本轮生成目录（0.10 GiB）。
- `resonance_live` 中文聚焦复跑：
  `out/resonance-live-template-recheck.log.runs/1789108559165530900.log`。
- Astral Forge 和控件聚焦复跑：
  `out/astral-template-recheck.log.runs/1789108745520120100.log`、
  `out/calibration-controls-recheck.log.runs/1789108762438863500.log`。

一次显式清理删除 131 个生成目录，共 41.98 GiB。清理后 Studio deploy 为 1,090 个
文件、243.3 MiB，Player deploy 为 1,079 个文件、232.2 MiB；各自 20 个 DLL，manifest
中 `template-switch-tests` 条目为零。Windows/Android 构建缓存、vcpkg 安装、正式模板、
必要依赖、归档日志和三个未纳入本次提交的 Glazed Celestial Gate 工作文件未删除。
