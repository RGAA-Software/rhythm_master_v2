# R3 首批交付：纹理材质与点光/聚光

R3 尚未完成：本增量完成图内四槽材质、切线法线和局部灯光，继续推进阴影、IBL
与 GLB 内嵌纹理导入。R4–R6 仍按主路线实施。长期运行和视觉品质验收分别跟踪。

## 可操作交付

- `material.textures`：基础颜色、RGB +Y 法线、ORM、自发光输入；sRGB 开关、
  法线强度与 UV 缩放/偏移。接图像/视频/生成纹理并沿用图资产发布。
- `scene.point_light` / `scene.spot_light`：能量/位置/聚光半角可连接信号，
  与方向光共用四灯预算，变换/合并/预览/发布参数保留。
- 音律珐琅 / Sonic Enamel：55 指令、73 边，三颗纹理雕塑和四条金属轨道，
  低频控制点光，高频驱动流纹自发光，响度控制呼吸。所有图节点可编辑。
- Windows Studio/Player 的 `deploy` 已更新，各包含 20 DLL 和全部资源；
  Android 增至 35 个内置示例，第一项直接选择新作品，未要求目录选择。

## 验证证据

Windows：`scene_tangents`、`scene_graph`、`scene_runtime`、`material_runtime`、
`texture_lifetimes`、`depth_runtime`、`scene_render_contracts`、`render_contracts`、
`program_contracts` 与 `windows_gpu_execution_probe` 通过。

D3D11 与 USB Adreno 650/GLES 3.1 后端逐像素检查颜色解预乘/转换、UV 象限、
法线方向、镜像非均匀缩放、ORM 金属度、自发光、点光衰减与聚光边界。
批处理额外检查不同贴图必须分开提交、相同贴图继续批量提交。
MikkTSpace 切线在 Windows 和 Android 都实际执行了基础几何、镜像 UV 与异常输入检查。

真实 PCM 同时刻平均 RGB 差异（0–255）：音乐/静音 1.692195，低频/静音 5.727565，
高频/静音 6.813701，低频/高频 5.708719。实图位于
`out/windows-release/sonic-enamel-music-captures`，不是凭节点命名判断音乐驱动。

MP4：120 帧解码、时间戳、重复导出像素一致、真实音轨 MSE 0.0000076297、
静音和取消清理检查通过，日志 `out/sonic-enamel-export.log`。
窗口全依赖部署和 Android `adb install -r` 成功，保留数据。
手机 UI 选择音律珐琅、播放内置 16 秒演示音乐；截图 `out/r3-android-sonic-enamel.png`。
手机实际选中运行包 SHA 与 APK 内置及 Windows 发布包完全一致。

| 产物 | SHA-256 |
| --- | --- |
| Sonic Enamel 运行包（2749 字节） | `f0fba9e755f57534aca6c196823f7cebfd1476a9238646e749fdac496c11b180` |
| Android APK（27,903,160 字节） | `81b6c6566972a4236c5775428c5061d178f89bf92c0a40cfd61923f98038c8e1` |

完整产物清单 `out/r3-textures-delivery.json`。后续构建将替换 deploy/APK，以上仅为本批证据。

## 边界

[材质契约](../material_pipeline.md) 记录 alpha、坐标、预算与生命周期。
目前贴图 alpha 不改变材质覆盖率；没有宣称完整 glTF 材质导入、阴影或 IBL。
35 是可运行示例数，不是基础/高级各 50 的品质目标已达成。
复用固定 vcpkg MikkTSpace（Zlib）、Godot 4.5.1（MIT）和 GLM 色彩公式（MIT）；
版本/文件/改动与许可分别保留在 provenance 和第三方 notices。
