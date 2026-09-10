# 输入效果组件的低／高频覆盖修订

11 个官方输入效果组件 0.1.1 → 0.2.0，总组件数仍为 29。
复用四件校准作品已经验证的 `audio_band_groups.py` 分组峰值，
保留公开参数和 source 图像输入，通过普通 `audio.band`／`scalar.expression`
节点构成可编辑内部图。依赖与 FFT 分析器保持原有方案。

## 明确复现与修复

公共构建器之前将 `audio_band=12` 和 `48` 称作低／高频。
它们只读取两个孤立频点。用真实 200 Hz 与 8 kHz PCM 验证旧流纹玻璃，
低／静音、高／静音、低／高三个最终图像差值均为 **0**。
失败日志 `out/p7-flow-glass-band-baseline.log`、输入身份／截图／差值
`out/p7-component-bands/baseline-flow/` 保留。

新组件分别聚合 0..23 与 43..62 FFT 带，即约 20–255 Hz 和 1917–16000 Hz。
组件的低／高频分工保留，中频未被悄悄并入其中，也没有创建不可达的中频节点。
`build_peak` 从原分组实现抽取，原三组的节点、边、ID、位置与表达式逐项比较完全相同。
新增单频点全覆盖、两组互斥／中频排除、非法索引在修改图前拒绝的检查，三项通过。
七组默认 PCM 字节与修订前一致，新增 `--low-hz`／`--high-hz` 支持复现离散频点漏测。

## 实测

Windows D3D11，1280×720，同一 0–4 秒历史；四组真实 PCM 为原有音乐、静音、
200 Hz、8 kHz。11 个组件的音乐／静音、低／静音、高／静音、低／高共 44 项
图像对照全部通过原 0.15 门槛，稳定阶段没有纹理增长。
Android GLES 原生运行各组件 640×360 预览包，420 帧中统计后 300 帧，
使用合成音频特征和 `glFinish`。所有输出有效，无预算拒绝、GLES 错误或纹理增长。

| 组件 | 展开指令 | 低／静音差 | 高／静音差 | Android 同步 P95 ms |
| --- | ---: | ---: | ---: | ---: |
| flow_glass | 78 | 28.770 | 13.177 | 8.89354 |
| self_relief | 80 | 18.940 | 11.121 | 5.77672 |
| beat_shutters | 81 | 9.072 | 6.233 | 6.04859 |
| mirrored_duet | 83 | 25.732 | 24.029 | 5.60177 |
| prism_fold | 78 | 29.682 | 29.963 | 5.80974 |
| contour_engraving | 78 | 26.122 | 18.387 | 5.66401 |
| motion_echo | 77 | 19.856 | 24.209 | 6.30766 |
| soft_glow | 76 | 15.907 | 6.963 | 5.63547 |
| polar_vortex | 80 | 41.933 | 39.849 | 5.73484 |
| audio_iris | 81 | 11.160 | 1.238 | 5.60141 |
| luma_windows | 79 | 6.275 | 5.245 | 5.35385 |

日志／目录：

- `out/p7-input-component-bands-all.log`，
  `out/input-component-bands/2309c2f1fe3e4ff5a3f3812bb2f2b209/`：
  每组件源与包摘要、执行程序／PCM 摘要、四张图、四项差值。
  `contact.png` 按表顺序人工查看，各过滤效果保留可辨认的输入图案。
- `out/p7-input-component-browser-all.log`，
  `out/input-component-bands/2c166cb2592740ceb96248e2647179e3/`：
  11 个实际中文组件库选择、GPU 预览、插入、撤销、固定弹窗及关闭释放全部通过。
- `out/p7-input-component-contracts.log`：全 29 个组件的公共参数／预设、
  插入、保存发布和运行合同通过。
- `out/p7-input-components-android-gles.log`，
  `out/android-template-measurements/eb8c12a673554280b4c53d1933b93431/`：
  USB 原生包、源与执行程序身份和完整测量。
- `out/p7-input-component-thumbnails.log`，
  `out/catalog-thumbnails/4b760da81b2e4da2b132e4be1802cf15/`：
  11 张实际渲染缩略图。仅缩略图在上述音乐／原生检查后重新生成；
  图、参数、音乐输入及渲染实现保持已测版本。

永久入口 `tools/test-input-component-bands.py`（通过 `verify_windows.py`）：
默认跑全部输入组件的 PCM 图像检查，`--browser` 跑实际库操作。
每批独立目录，全部组件分别记录结果，失败项不被其他通过项覆盖。
`measure-android-templates.py --kind semantic --name ...` 复用已有原生测量入口。
来源及生成工具摘要记入 `provenance/input_components.json`。

官方组件按 0.2.0 插入；工程内已经保存的实例保留其组件快照，升级已有实例需要
显式编辑或重新插入。Android 为运行包原生验证，不声称新增手机组件编辑器。
同步耗时不等于应用 FPS；本批不覆盖长稳、声学输出、所有输入图像的视觉质量。
审核账本中的品质合格计数不因修复或新增内部节点而增加。

最终 Windows 增量交付：`out/p7-input-components-current-delivery.log`，
9 项强制回归全部通过（75.01 秒），含双语言实际模板切换、公开控件修改交付
与大音乐文件保存发布重开。Studio / Player 的 sibling `deploy` 均同步可执行
程序、20 个 DLL 和当前内容。前一次失败保留于
`out/p7-input-components-final-delivery.log`，定位与永久检查见
[重开就绪回归](soundtrack_reopen_readiness_2026-09-10.md)。
