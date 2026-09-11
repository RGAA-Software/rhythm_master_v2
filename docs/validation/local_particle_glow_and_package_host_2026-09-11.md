# 统一 GPU 粒子光晕与发布宿主同步（2026-09-11）

## 用户报告、根因和缺失路径

用户在 `星铸圣坛 / Astral Forge` 中看到蓝色粒子似乎被重复拖影，并指出把粒子单纯缩小不能解决问题：大粒子也必须保持自然、连续的柔光边缘。

根因有两层：

- 作品把多个青色轨道航标作为独立 `scene.instance` 对象绘制，又对整个三维雕塑纹理做 `texture.blur`。这使多个真实对象被模糊到一起，看起来像同一粒子的蓝色重影。
- 通用 GPU 点精灵只有尺寸参数。此前若想扩大光晕，只能在精灵外增加纹理模糊，导致不同尺寸和不同模板出现不一致的光斑、拖影与重影。

初次修复后，Studio 交付的模板应用检查通过，但目录缩略图和音乐质量脚本读取当前包时报告 `package.invalid_program`。包本身有效；`windows_effects_gpu_tests.exe` 和 `music_gpu_tests.exe` 是接口更新前留下的可执行程序，未被 Studio delivery 的目标集重建。这是验证宿主版本漂移，不是包或图的错误。

## 统一修复

- `GpuPointStyle` 增加有界的 `glow_radius_`，公开节点属性为 `gpu.render.point_glow_radius`，范围 1–4。
- 顶点着色器只扩大同一颗点精灵的覆盖范围；片段着色器在这个范围内计算高斯亮核、局部光晕和柔边截断。核心尺寸保持稳定，光晕随半径平滑扩展，不再使用整屏或粒子纹理模糊。
- `星铸圣坛` 删除 14 个航标实例和三维场景的 `texture.blur`，保留持续旋转的金/青轨环、相机运动与柱体。两层粒子以每秒速率生成，采用可见的中景青色尘光和前景金色火花（点尺寸 0.0038 / 0.0100，局部光晕 1.55 / 2.20）。
- `tools/build-windows.py` 的每次 Studio delivery 现在同时构建 `windows_effects_gpu_tests` 与 `music_gpu_tests`，使目录缩略图和解码 PCM 验收使用与发布器相同的运行时包读取器。

## 当前 Windows 证据

- `out/windows-release/studio-delivery-tests.log.runs/1789096070674667400.log`：Studio 实际模板套用、节点 ID 重映射、保存/重开和发布路径 9/9 通过；中英文 GPU 模板切换均通过。
- `out/astral-forge-local-glow-thumbnail.log.runs/1789096070674667400.log`：当前 D3D11 目录缩略图包可加载并成功渲染；输出在 `out/catalog-thumbnails/bf8d586bbe9048e5ade5015f8408b8e2/astral_forge/`。评审图显示前景金色粒子连续柔和，蓝色成片重影已消失。
- `out/astral-forge-quality-gpu.log.runs/1789096184417227900.log`：193 个可达指令、63 个频段、两层 GPU 粒子。低/静音 2.2860，高/静音 3.6503，中/静音 2.6728，均超过最小差异阈值。
- `out/astral-forge-controls-gpu.log.runs/1789096211376126500.log`：六个公开控制项的最小/最大图像差为 1.8518–28.0612，全部非零并超过阈值。

以后任何更改 GPU 点样式、节点描述或内容打包器的提交都必须：重建这两个独立 GPU 宿主、从当前运行时包实际渲染缩略图、再运行解码 PCM 质量检查。静态图、旧测试可执行程序或只验证包发布器都不能替代这些路径。
