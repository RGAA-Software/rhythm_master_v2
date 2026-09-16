# Godot PCF13 阴影质量验证

日期：2026-09-16。范围是 W1.1 的第一批增量：保留 Nearest/PCF5，增加 PCF13，
不在本批实现稳定方向光投影、级联或点光全向阴影。

## 来源与实现

- 上游为 Godot `4.5.1-stable`，提交
  `f62fdbde15035c5576dad93e586201f4d41ef0cb`；文件
  `drivers/gles3/shaders/scene.glsl` 的 SHA-256 为
  `741d48c937836dd443b4526544648194588d0349e88472a0f54c1b140a9cdf31`。
- 保留 `SHADOW_MODE_PCF_13` 的中心、两倍 texel 远十字、单 texel 近十字和四个
  对角线，共 13 个比较；前五个比较全亮或全暗时直接返回。
- 适配只把 Godot shadow sampler 比较改为项目已验证的 D24S8 原始深度显式比较，
  继续使用项目标准的 `[-1,+1]` clip 深度、左上 UV 和公共渲染句柄。
- 图属性、Scene3D、Runtime 和 Render 合同统一为 Nearest/PCF5/PCF13 三档。
  已有数值 `0/1` 语义不变，默认值仍为 `1`（PCF5），新高档为 `2`。
- 完整来源、许可、哈希和适配范围在 `provenance/godot_shadows.json`。

## 验证

- 受影响目标在 `out/core` 和 `out/windows-pcf13` 增量编译；
  `scene_render_contracts`、`scene_graph`、`shadow_runtime` 均为 3/3 通过。
- 同一 `scene_fragment.sc` 使用仓库跟踪的 shaderc 1.19.157，分别编译 Windows
  `s_5_0` 和 Android `300_es` 成功；本项只证明两端 shader 语法/容器生成，
  不把它当作 Android 实机像素证据。
- 完整 shader 工具检查在 Windows/Android 各 12 个组上重复编译 64 个程序产物，
  每组两次字节一致；失败和并发过期编译继续保留先前有效产物。记录位于
  `out/pcf13-shader-verification/verification.json`。
- GPU 检查只通过 `tools/verify_windows.py` 串行执行。D3D11 固定场景使用 256×256
  阴影图和 128×128 接收目标，分别回读三档；红色直射光通道的全图绝对差为：
  Nearest→PCF5 `1920`，PCF5→PCF13 `1792`；中间强度边缘像素依次为
  `0/24/210`，并由永久测试断言单调增加。全遮挡、全照亮、透视聚光、域外、偏移和
  关闭绑定检查继续通过。
- D3D11 完整 probe 日志：
  `out/windows-pcf13/pcf13-gpu-final.log.runs/1789543660795018700.log`。
  CTest 包装日志：
  `out/windows-pcf13/pcf13-gpu-ctest-final.log.runs/1789543735115995000.log`。
- `tools/check-boundaries.py`、locale 对称性、内容质量测试和 `git diff --check`
  在提交前重新执行。

## 构建环境修正与限制

新工作区恢复依赖时发现 Python 可同时继承 `Path` 和 `PATH`，VS 2026
`Enter-VsDevShell` 会拒绝重复键；Windows 构建包装现在按 Windows 大小写不敏感规则
去重。失败配置只留下 `CMakeCache.txt` 时也会强制重新生成 `build.ninja`，并拒绝传播
不存在或 `-NOTFOUND` 的 SDK 缓存值。

聚焦 GPU 构建关闭媒体模块，因为本工作区尚未恢复项目隔离的 LGPL FFmpeg SDK；它不算
Studio delivery。完整 `tools/build-windows.py`、强制 9 项 Studio 作者路径、Android
实机 PCF13、大投影/移动相机/细几何动态评审和短性能数据仍待后续增量，不在本文宣称通过。
