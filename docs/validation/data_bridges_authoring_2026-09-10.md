# 频谱与纹理点属性桥接：实际作品验收

## 范围

对应 P5 的频谱→点→路径，以及纹理→GPU 点属性两个最小转换。
合同和模块证据分别见 [数据域桥接](../data_domain_bridges.md) 与
[GPU 纹理采样](../gpu_texture_sampling.md)。不包含 GPU→CPU 读回或任意 compute。
两个作品仍为功能示例，不计入 P7 已审核高品质数量。

## 频域花冠

实际 Studio 空图添加 24 个节点、字体与许可、属性／命名绑定、音乐、视图操作、
组件封装和保存发布重开通过。运行
`out/windows-release/from-empty-studio-spectrum/2010e3e9cffc4db98b9dce2372def787/`，
记录 `out/p5-spectrum-authoring-tests.log`。
真实 PCM/GPU 平均 RGB 差：音乐／静音 2.38057，低频／静音 3.05005，
高频／静音 0.44463，低频／高频 3.49469。人工查看花冠孔洞、频谱轮廓及
“频域花冠 / SPECTRAL COROLLA”完整可读标题。

## 字潮：保留过暗结果，不以非黑像素代替验收

初次 `out/p5-gpu-sampling-authoring-tests.log` 的 UI、保存发布和 PCM 对照通过，
但人工查看运行 `out/windows-release/from-empty-studio-gpu-sampling/2f04e7f9c36f447ba65e0f8a57ee92eb/`
的音乐图，“声浪”粒子字过暗。该画面不作为合格呈现证据。点源默认透明度 0.2，
叠加较小点尺寸和渲染透明度后亮度不足。仅修正作品配置：点源透明度设为 1、尺寸
由 0.0035 增至 0.0055、驱动透明度下限由 0.45 增至 0.65，保留点数和全部节点。

重新运行完整实际创作，`out/p5-gpu-sampling-legibility-tests.log` 通过，最终运行
`out/windows-release/from-empty-studio-gpu-sampling/d34597e614764408b9c20e2597d4119d/`。
23 个节点全部可达，65536 点共用一个缓冲，原始点作背景、采样视图形成中文粒子字。
同包真实 PCM/GPU 差值：音乐／静音 5.82822，低频／静音 5.81555，
高频／静音 5.64314，低频／高频 8.47162。
人工查看 `music/resonance_demo.png`：青粉粒子“声浪”清晰可读，英文字标、背景
流动点和柔光均正常。最终内置工程从该成功且已看图的产物提取，没有覆盖失败记录。

## 两端交付

Windows Studio/Player 完整 deploy 已更新，最终五项模板／编辑器／内容／双语言
实际模板切换检查全部通过（28.11 秒），记录
`out/p5-bridges-thumbnail-delivery.log`。两件目录缩略图均来自实际渲染。
`out/p5-spectral-corolla-export.log`、`out/p5-glyph-current-export.log` 均通过
640×360、30 FPS、120 帧 MP4 导出：重复解码帧一致、音乐与静音输出不同、
音轨 MSE 7.6297e-06、取消清理通过。不是仅检查文件存在。

Android 当前 APK SHA256 为
`901e2d035078c09cf842ae65837dc5851cb83c01637f5238ea74b34d5203cee6`，
已 `adb install -r` 覆盖安装，安装后提取的 APK 哈希一致。
`out/p5-spectral-corolla-android-music.log` 和
`out/p5-glyph-current-android-music.log` 分别通过当前包内真实 PCM 的原生 GLES
音乐／静音各 960 帧、640×360 渲染；2/6/10/14 秒 RGB 平均差如下：

| 作品 | 2 秒 | 6 秒 | 10 秒 | 14 秒 | 峰值纹理字节 |
| --- | --- | --- | --- | --- | --- |
| 频域花冠 | 1.49086 | 2.18725 | 1.48701 | 2.23579 | 8306180 |
| 字潮 | 4.34714 | 4.47892 | 4.24595 | 4.45094 | 13041156 |

应用内 UI 证据在
`out/android-authored-works/7ec3fdb8729e4751b6fbd509745e973d/`，
脚本日志 `out/p5-bridges-android-ui.log`。逐件搜索并选中内置作品，人工检查
暂停／恢复截图：花冠孔洞、轮廓、双语标题正常；“声浪”粒子字和英文标识清晰，
背景粒子可见。花冠时间 1.04→2.13 秒，字潮 1.41→2.50 秒，均恢复播放且 RMS
非零；用户保存工程字节未变化。脚本保留 `presentation_review_pending`，本段
记录其后的人工截图核对。没有声学回录或长稳证据，不计入 P7 品质数量。
