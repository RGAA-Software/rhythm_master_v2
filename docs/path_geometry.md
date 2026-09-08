# R4 路径与管线几何

本批交付有序 Path 类型、螺旋、点到路径、Catmull–Rom 重采样、管线网格，
以及路径/几何节点预览。它是 R4 的首个增量；变形、用户 shader 和模型动画另行推进。

## 创作契约

| 节点 | 输入与输出 | 主要控制 |
| --- | --- | --- |
| `path.helix` | 可选标量 → Path | 半径、高度、圈数、相位可连音乐/时间；采样数与闭合属性 |
| `path.from_points` | CPU Points → Path | 按输入数组顺序均匀抽取，画布归一化坐标转换为世界 XY，跨度可调 |
| `path.resample` | Path → Path | GLM Catmull–Rom 插值；保留开放端点或周期闭合；均匀参数采样，非等弧长 |
| `geometry.tube` | Path、可选管径标量 → Geometry | 圆截面半径与边数，开放路径有平面端盖；可接已有材质/实例/场景节点 |

Path 是不可变有序世界坐标值，不引用编辑器、GLM 或 GPU 对象。闭合路径不重复末端点。
点桥接只读取位置，不暗中把无序粒子排序，也不传递颜色/尺寸；需要逐点网格复制时继续用
`scene.point_instances`。相邻重合点去重；点数不足时输出空路径，空管线正常显示为空。
其他无效坐标、退化切线或过量输入明确报错，不制造 NaN 网格。

每条路径最多 1024 点；编译后的路径快照累计最多 65536 点。管线截面 3–32 边，
沿用 25 万唯一顶点、75 万索引和 300 万场景绘制索引预算。闭合管线同时复制纵向与
径向 UV 接缝，修正绕闭合环路的截面旋转余量。开放端盖使用独立平面法线；法线贴图
继续走 MikkTSpace。现阶段路径与管线在 CPU 生成，输入版本不变时复用快照和上传；
它不是 GPU 几何生成或任意大规模曲线求解器。

路径缩略图将包围盒居中归一化，画细管线；几何预览保留实际网格。预览按既有 15 Hz、
最多 8 项按需捕获，缓存 CPU 预览场景；隐藏后释放资源。发布包按注册的算子名校验，
旧 Player 遇到新增算子明确拒绝，原有作品无需迁移。

## 复用与验证

直接调用已有 vcpkg GLM 的 `catmullRom`、`rotation`、`orientedAngle`。
管线截面沿路径挤出与网格连接参考 TiXL `ExtrudeCurves.hlsl`，保留固定原文件和 MIT
通知；新增封口、旋转最小化截面、接缝与预算适配。具体文件/修订/修改见
[出处记录](../provenance/path_geometry.json)，没有新增待定依赖。

Windows 和 USB Android 的 `scene_path_tests`、`path_runtime_tests` 已通过：
开放端点、闭合接缝、面朝向、端盖、切线、边界、音乐/时间更新、不可变缓存、
反复更新和预览隐藏后的资源回收。现有场景、材质、环境光运行时测试在 Windows 通过。

可编辑作品“极光织带 / Aurora Braid”由 76 个节点和 96 条边组成：三股频段驱动管线、
外侧细轨、五重悬浮环、金属材质与环境反射，再接景深、浮点柔光和 SDR 显示转换。
作品已通过真实解码 PCM、静音、低频和高频的同时间 GPU 对照；0–255 RGB 平均差异
分别为音乐/静音 2.0754、低频/静音 2.6178、高频/静音 6.3945、低频/高频 7.4107。
MP4 120 帧重复渲染、PTS/音频和取消检查通过。缩略图由真实渲染生成，非设计示意图。

Windows Studio/Player 已按 Python 流程更新 deploy，均含 20 个 DLL 与资源。
USB Redmi K40S / Adreno 650 / GLES 3.1 已 `adb install -r` 覆盖安装，并在内置效果
界面播放；截屏显示 4.42/16 秒与 RMS 0.0607089，三股管线/光环/后处理可见。
设备选中包与 Windows 包 SHA256 一致：
`e9fa5c72cbdfe4aaf3488a831fddece57c3875adcbffcf20e241212a023b03e4`。
本批 APK SHA256：`8c7baea32bed62aa0c80103ce4312ca8fa11cbab3358e11aa3226d04e1355c65`。

证据保存在 `out/r4-path-work-tests.log`（其中音乐测试的旧场景限定已修正，最终结果见
`out/r4-path-music-tests.log`）、`out/r4-path-runtime-android-tests.log`、
`out/r4-path-android-picker.png`、`out/r4-path-deploy-final.log` 与
`out/r4-path-apk-build.log`。功能检查不代替用户视觉品质与最后的长稳验收。
