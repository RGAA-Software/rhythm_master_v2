# R1 GPU 粒子增量，2026-09-08

## 实现与边界

`gpu.particles` 输出独立 GPU 点类型，`gpu.render` 将其绘制成纹理，可继续接映射、合成、滤镜与输出。CPU 物理/点变换端口不能接 GPU 点；没有隐式读回或 CPU 模拟替代。位置/年龄、速度/寿命、RGBA、尺寸/旋转组成固定 64 字节记录。Renderer 公共 API 只暴露设备/槽位/代次句柄，资源由可移动 RAII 值持有；native buffer 与 bgfx 类型保留在适配器内。

每个缓冲最多 262144 点、最多 16 个缓冲、总容量 1048576 点（64 MiB 点记录）。这只计算点缓冲，不含纹理、网格、驱动开销。图编译、包验证和运行时均限制总容量；renderer 再按实际设备资源准入。没有对不支持 compute/实例化的 profile 静默退回 CPU。

发射、环形覆盖、寿命到期和按时间计算的阻力适配本地 TiXL `ParticleSystem.hlsl`；MIT 来源、固定 revision、原文件 hash 与修改清单在 [provenance](../../provenance/tixl_particles.json)。保留原文件及版权许可。项目补充归一化画布坐标、确定种子的整数随机发射、解析无散度流场，以及直接用同一缓冲绘制软粒子。

## 时间与音乐

- 固定 1/60 秒步长，正常 30 fps 帧执行两个更新；每次最多八步。
- 暂停与节点预览不推进 GPU 模拟。预览读取相同状态，不重新发射。
- 后退或一次前跳超过八步时按固定种子重置到起始分布；这不是重建被跳过的模拟历史。
- 显式重置代次释放旧句柄；配置/容量变化重启，动态输入变化保持历史。
- 发射倍率、突发数量、流场强度接受标量输入，可接真实音频频段、曲线或表达式；突发只在低到高时触发一次。
- 不保证不同 GPU 的浮点结果逐像素一致。相同设备固定种子、顺序时间的重复导出单独检查。

## 已通过的短检查

Windows D3D11 与 USB Redmi K40S（Adreno 650，API 34，GLES 3.1）通过：

- 已知颜色发射、非 64 整倍数容量、环形覆盖边界、暂停不变、寿命到期清空、重置颜色/清空。
- 同帧六次计算更新后按像素质心验证运动，检查连续 compute 的可见性。
- 10000、100000、262144 点各一次计算、一次绘制，并读回确认非空；对应记录字节数为 640000、6400000、16777216。
- 运行时固定步长、暂停/恢复、前后跳转、代次释放和节点预览；编译端独立类型与容量拒绝。

这些是功能和资源路径检查，不是持续 FPS、GPU 耗时或热稳定结论。Null 后端只验证生命周期/调用契约，不产生 GPU 模拟证据。

## 创作与交付状态

“频谱星云 / Spectral Nebula”源工程位于 `content/templates/spectral_nebula`：21 个节点、25 条连接、两组共 98304 个 GPU 粒子，六重/九重映射与柔光合成。低频驱动流动，高频驱动发射，响度驱动光芒。全部节点可编辑，视觉品质仍需审核。

实际 Windows 预览、真实 PCM/静音/频段对照、运行包与 MP4 导出均通过。为平衡画质下的可见性提高了细粒子的尺寸与光芒；修改后重新通过模板、音乐与导出检查。四秒同时间 RGB 平均差：演示音乐/静音 24.005625，低频/静音 39.645757，高频/静音 49.087207，低频/高频 49.360000（原阈值 0.15 未变）。

MP4 检查包含 120 帧顺序时间、音画长度、实际解码样本、非静态画面、重复导出一致、静音画面不同以及取消处理。Windows Studio/Player 完整 deploy 已更新，性能面板显示 GPU 粒子容量/字节数和绘制提交数。

Android 原有 GLES 回归与本运行包 60 帧检查通过；34 个内置效果的 APK 已用 `adb install -r` 覆盖安装。UI 选择“频谱星云”，播放内置演示音乐；从应用私有文件校验选中包 hash，未卸载、清数据或让用户选目录。最后调整版本 APK SHA256：`ae88ccf182d04109dc590c4b4964b5033b29b0b352c4b9bd889ae5deccd41a3e`，27771846 字节；运行包 SHA256：`0323b3e260982d4fd5c31362710e195f6c0fdbbe3bc1b7ee6924db98e7f23bdd`。

新 Android 构建默认 GLES 3.1；已有缓存保留，显式 `--gles-version 30` 仍可验证旧档位。Python 打包器按实际缓存生成 APK 的 GLES 要求，3.1 构建不再误报最低 3.0。实际 compute/实例化仍由能力检查决定，并非所有 Android 设备的通用支持声明。

证据输出：`out/gpu-particle-android-android_gpu_contract_tests.log`、`out/gpu-particle-android-gpu_particle_tests.log`、`out/spectral-nebula-android-regression.log`、`out/spectral-nebula-delivery-tests.log`、`out/windows-release/spectral-nebula-music-captures/`、`out/windows-release/spectral-nebula-export/`、`out/gpu-particles-android-playing.png`。第三方边界/公共 API/中英文 key 检查通过。

R1 的本轮功能闭环完成，继续 R2 浮点/颜色/深度；不表示全部 R0–R6、视觉品质或多平台已完成。长稳与热测试统一留至最后。
