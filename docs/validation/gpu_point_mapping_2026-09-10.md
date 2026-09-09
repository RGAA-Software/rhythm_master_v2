# P6.2 GPU 点属性映射交付记录

状态：P6.2 受限 GPU 点映射已完成 Windows／Android 功能交付。
P7 品质数量不因本记录增加；通用 compute／任意缓冲布局未采用。

## 实现与来源

本地 TiXL TransformPoints 的独立输入／输出、复制后修改与 64 线程保护，
按 MIT 通知适配，原仓库只读；详见 `provenance/tixl_point_attributes.json` 和
[设备实验及合同](../gpu_attribute_evaluation.md)。复用现有 bgfx compute 和
64 字节点布局，没有新外部依赖、原生缓冲公开接口或 CPU 同步读回。
`gpu.map` 提供中心缩放／旋转、平移、点尺寸、颜色乘数、透明度；各输出独立，
两级串接和原始分支可并存。既有纹理采样放在映射之后，错误顺序明确拒绝。

## 永久检查

- `gpu_point_contracts`：Null 句柄／代次、跨设备、初始化、别名、容量、有限参数、
  pass 预算失败与设备失效。两端 `--gpu-points`：正式 Renderer 两级像素对照、
  参数变化、输入不变、释放输入后输出仍有效，以及原有粒子行为。
- `gpu_particle_graph`：GPU／CPU 端口边界、音乐端口、多级累计内存和非法采样顺序。
- `gpu_particles`：Null 暂停缓存、参数变动复用、预览只绘制、容量重建、分步准备、
  取消已分配映射、无需求分支不分配；原有 `gpu_sampling` 通过。
- Studio 交付强制五项检查全部通过：`out/p6-gpu-map-studio-build.log`。
  这轮目录尚不包含后续新增的相位羽流，不能代替其模板应用验证。
- 新增 `from_empty_studio_gpu_mapping` 保留实际空图编辑、颜色／端口／音乐连接、
  直接拖动画面、封装组件、保存／发布／重新打开、节点检查和真实 PCM 像素对照。

## 相位羽流 / Phase Plumes

27 节点，65536 源点和三个独立映射缓冲，总请求点内存 16 MiB。
低频控制空间缩放和流场，高频控制尺寸；外环使用完整频谱。
作者源 `content/authoring/phase_plumes.json`，音乐为现有原创合成素材。

首轮实际 UI 与四种 PCM 检查通过，但人工检查发现正常音乐过暗、扩散出画面；
不以像素差通过当作视觉合格。保留 `out/p6-phase-plumes-authoring.log` 及运行
`out/windows-release/from-empty-studio-gpu-map/9b7144c8f68441878c43d3f0fbcb922b/`。
修订降低流场和寿命，提高基础点尺寸与透明度，重新走相同 UI／音乐流程。

第二轮 `out/p6-phase-plumes-visible-authoring.log`，运行
`out/windows-release/from-empty-studio-gpu-map/3bfa4325fb3848bb913479dc3234b1a9/`。
人工已查看正常音乐与高频图：三组粒子卷曲流清晰可见，高频增强尺寸和明度；
音乐／静音平均 RGB 差 17.65515，低频／静音 23.16650，高频／静音 17.25184，
低频／高频 32.06099。该数字证明输入响应，不是音质、帧率或内容品质评分。
接下来追加重开后映射节点检查截图，最终使用该次完整输出作交付身份。

## 已保留的失败

首轮 Renderer 测试代码的统计字段名及预算头遗漏导致编译失败；已修正重建。
Runtime 新测试错误地把两个节点预览预期为两个 pass；实际每个预览含点绘制和
适配拷贝两个 draw，修正为四 pass／四 draw 并通过。详见设备实验记录。
以上均未通过删除断言或以保留旧图替代新图来处理。

## 65536 点版本的 Windows 编辑作品身份（后续已调低容量）

`from_empty_studio_gpu_mapping` 和 PCM fixture 通过，49.39 秒，日志
`out/p6-phase-plumes-inspection-authoring.log`，运行
`out/windows-release/from-empty-studio-gpu-map/47977bb42ab7424bbaaa33c1c3abf0ba/`。
人工查看 inspected-9／10：一级和二级节点均有独立可见内预览，重开后绑定标记、
尺寸倍数、旋转、平移和颜色乘数正确，中文标签完整。已从该保存工程提取
`content/templates/phase_plumes`，作为 example／visual-review-pending 候选。

最终 recipe SHA256 `621f24b7199e075cb80d3348ae3333bc8d79f8db03c17cf3ba6f31b4b905e6e9`。
发布包 SHA256 `fc4d7e26bb0fcbd15af59bcb26bdccbbea911f422e75705b70bdf4c113e23753`。
音乐 SHA256 `f0185471aedfd7561f148cdeb087cc417b5519ed7edd82f0d6c34933c5fd5cec`。

`out/p6-phase-plumes-export.log` 通过 640×360／30 FPS／120 帧 MP4：重复解码帧
一致、音乐／静音输出不同、音频 MSE 7.6297e-06，取消路径清理。
运行 `out/p6-phase-plumes-export/650832290780400/`；人工查看 motion-sequence.png
的八帧序列，粒子由小簇伸展成卷曲流，颜色分支可辨。这不是长稳或全曲品质审核。

## Android 成本反馈与 32768 点修订

65536 点版已覆盖安装并通过 APK 内包／PCM／GLES 对照。
APK `4d1f53f718b5e61ea2f57f6cf144719fc06d5979ae7f2d58f63fa856c6dade0d`，
`out/p6-phase-plumes-android-music.log`，运行
`out/android-music/0c87b6abeae947638143f6051ce13bc1/`。
音乐／静音各 960 帧，640×360；音乐 p50/p95 25.0261/30.2935 ms，
峰值纹理 7372804 B（不含另计的点缓冲）；四时刻 RGB 差
19.9956／16.9803／8.92267／15.1609。功能通过，但成本余量偏小。

将示例源容量减半为 32768，总四缓冲 8 MiB，保留所有分支、音乐连接和其他参数。
每秒生成 10000、寿命 3 秒，稳定段约 30000 个活跃点；此调整主要削减未活跃记录的
映射和顶点处理，不能预先断言耗时减半。Renderer 的验证上限不变。

重新实际 UI 编辑及 PCM 通过：`out/p6-phase-plumes-budget-authoring.log`，运行
`out/windows-release/from-empty-studio-gpu-map/783a2febdb5c43f49fb71950624337e3/`。
人工查看正常音乐图，三组流形保留；音乐／静音差 17.62747、低／静音 23.14040、
高／静音 17.25273、低／高 32.04828。此前 65536 点结果保留作成本对照。

## 最终 32768 点交付与限制

- 源身份 `f6e4c3ac154b814bac90963647615fbf5ed2b3016b62d8c57e14fa762fd99a88`。
- recipe `8f6d52f3b9e8645bc9e0191a744a11355e85cc421a79781be27df1bc9b00df9a`。
- 包 `f2f4762d7c1878f931e046331648f6ceed4181be1ddab701955436b58f4e0681`。
- APK `5fed1789961526974f78e4c9bf653460e9f14e9feb6ee4e7d5f593e2e1a1c510`。

Windows 最终构建 `out/p6-phase-plumes-budget-windows-build.log`，五项交付回归
35.48 秒全部通过，序列包含新作品的中英文列表选择／ID 重映射／当前输出／保存／
重开／发布。此前 65536 点应用截图也已人工查看，不把它当成最终包身份。
最终独立 Player 30 帧原生加载在 `out/p6-phase-plumes-player-smoke.log` 通过。
Studio 和 Player 各自 deploy 均包含 exe、20 DLL 和资源。

最终缩略图由实际 D3D11 Player 生成，`out/p6-phase-plumes-budget-thumbnail.log`；
其输入是合成标准特征，不冒称真实 PCM。最终 120 帧有声 MP4 的重复／静音／取消
检查在 `out/p6-phase-plumes-budget-export.log` 通过，音频 MSE 7.6297e-06。

最终 Android 构建 `out/p6-phase-plumes-budget-android-build.log`，包含 53 个内置
候选，覆盖安装 `out/p6-phase-plumes-budget-install.log` 成功，未卸载或清空数据。
`out/p6-phase-plumes-budget-android-music.log`，运行
`out/android-music/0257910ceaa141e0811d41945a2f4419/`：APK、宿主包和源身份一致，
音乐／静音各 960 帧 GLES；音乐 p50/p95 18.1874/22.7391 ms，静音
15.3974/18.1262 ms。峰值纹理 7372804 B，点缓冲另计 8 MiB。
四时刻 RGB 差 12.1108／16.9474／8.91381／15.1139。
比 65536 点版降低约 25% 的音乐 p95 提交成本，但不是纯 GPU 计时或 60 FPS 保证。

实际安装 App 的内置选择／暂停／继续通过 `out/p6-phase-plumes-android-ui.log`，
运行 `out/android-authored-works/c776f5dc34a3478e92d1cc96eefdae5e/`。
人工查看 paused／resumed PNG：标题相位羽流、三色卷曲点流、16 秒、暂停 2.384 秒
和继续后的播放状态正确，RMS 非零；安装 APK 哈希一致、保存节目保持不变。
自动记录保留 presentation_review_pending，此处记录人工功能检查，不冒称声学录音
或内容品质终审。未进行长稳／温升测试；继续 P6.3、P6.4 和 P7–P9。
