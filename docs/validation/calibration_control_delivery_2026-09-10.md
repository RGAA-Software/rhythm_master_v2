# 校准作品公开控件与修改交付

范围：墨潮、织光机、共振拱廊 0.2.0，瓷光钟摆 0.3.0。
本批没有修改作品、运行核心或 Android APK。源／包身份沿用
[宽频段批次](calibration_band_coverage_2026-09-10.md)和
[瓷光钟摆频段覆盖](porcelain_frequency_coverage_2026-09-10.md)。

## 真实 Studio 编辑交付

复用 `template_switch_gpu_tests` 的目录选择、实际应用和 ID 重映射流程，
通过 `--controls` 进入四件作品。`control_delivery_checks` 在当前 ImGui
滑块实际布局上发送鼠标按下、拖动、释放，逐个检查三项控件的上下限，共 24 次。
每次要求编辑后的 requested generation 大于编辑前、当前安装代次有效、
无预算降级，并核对保存的默认值、实际点击重开后的新代次及随后发布的默认值。
原来只验证未修改模板的路径继续保留。

- CTest：`calibration_controls_gpu`，首次通过 26.50 秒。
- 日志：`out/p7-controls-ui-timeline.log`。
- 操作轨迹／24 张极端值截图：
  `out/windows-release/calibration-controls-gpu/661262861993700/`。
- 工程／运行包保留在同一证据目录；操作日志列出重映射后的控件 ID 和期望值。
- 已加入 `tools/build-windows.py` 的每次 Studio 交付必测集合，含 no-op 构建。

首次新测试错误地隐藏时间线，却仍要求时间线的配乐波形缓存就绪，
因此在模板应用阶段失败，尚未执行滑块检查。
失败日志 `out/p7-controls-ui-first.log` 和目录 `661212645809600` 保留。
恢复原测试的时间线入口后通过；没有降低现有模板应用判据。

## 控件确实改变最终图像

复用现有 FFmpeg 解码／分析、Player Session 和 D3D11 实际截图检查，新增
`music_gpu_tests --control-extremes` 及 Python 包装参数。
每个控件分别使用最小／最大值，其余控件固定为作者默认值；同一份真实
`resonance_demo.wav`、同一 0–4 秒历史、1280×720，比较第 4 秒完整最终图像。
这样不把其他 Cue 或时间差当作控件响应。原四组 PCM／四项对照也继续执行。

| 作品 | 音乐响应上下限差 | 速度上下限差 | 纹理／曝光上下限差 | 稳定纹理字节 |
| --- | ---: | ---: | ---: | ---: |
| 墨潮 | 15.61600 | 50.35502 | 4.14356 | 29,491,204 |
| 织光机 | 2.34539 | 15.24618 | 22.91994 | 49,350,852 |
| 瓷光钟摆 | 0.94220 | 1.72907 | 59.75380 | 43,059,396 |
| 共振拱廊 | 11.39453 | 11.60703 | 19.76846 | 77,365,444 |

差值为每个 RGB 字节的平均绝对差，全部超过原有 0.15 门槛。
四次检查全部通过，单场景 124 帧中稳定阶段未出现纹理字节增长。
每次保留 10 张实际截图、七项差值、控件 ID／范围及输入包、执行程序、PCM SHA256：
`out/p7-control-extremes/{ink_tide,chromatic_loom,porcelain_pendulum,resonant_arcade}/`。
日志分别为 `out/p7-{ink,loom,porcelain,arcade}-controls-image.log`。
四份 `controls-contact.png` 已人工查看：极端值下构图仍可辨认，音乐形变、
速度对应的运动状态和墨潮纹理／三维作品曝光均有可见差异。
这证明公开控件有效，不代替最终视觉品质审核。

## 换配乐、保存、清除、重开、发布

复用 `soundtrack_studio_gpu_tests` 的现有文件加载适配器和实际“绑定配乐”按钮，
将每件作品原来的两素材编排改为独立的 16 秒 `quiet.wav`。
新素材 SHA256：`3b088f7ab642a7ca99dd34a8ab3d27811142a41b50b26654c445fcca586e348b`。
四件均通过保存、新音乐身份发布、实际清除、重开恢复非零音频特征、
波形缓存和当前有效图检查；完整指令数分别为 125／297／211／247。

原测试仅以“保存后存在 soundtrack”判断绑定成功；对已内置音乐的模板，
这可能误认旧配乐为新绑定成功。本批改为独立内容哈希核对，保存和发布均要求
目标素材身份且不再保留旧片段编排。文件加载移到首次工程配乐同步之后，
避免初始模板同步覆盖测试指定的新音乐。没有把一次同参数重跑作为修复证据。

- 日志：`out/p7-ink-replace-music.log`、
  `out/p7-{chromatic_loom,porcelain_pendulum,resonant_arcade}-replace-music.log`。
- 独立工程、发布包、输入身份副本、Studio 截图：各作品上述目录下的 `replacement/`。
- 永久入口：四个 `calibration_*_replacement_gpu` CTest，
  自动依赖 `calibration_music_fixture` 生成同内容音乐夹具。

## 边界

滑块检查保存的是作者默认值。实时覆盖不等于持久化的演出状态；重开后 Cue
重新参与求值，不能承诺重开画面与拖动瞬间逐像素一致。图像因果对照使用当前
源运行包的固定输入，真实 Studio 检查则独立覆盖修改／重映射／交付路径。
换配乐检查明确替换整个编排，不声称逐个片段换源或所有图片／模型／Shader
资产编辑均已验收。新音乐通过现有公开加载适配器进入，未自动操作系统文件选择器。
没有新增手机验证、声学录音或长稳测试；最终视觉、设备档位和 P7 数量仍未完成。

## 集成结果

`out/p7-controls-final-delivery.log`：20 worker 增量交付，Studio／Player 各复制
20 个 DLL 和完整资源；六项强制检查全部通过，63.37 秒。其中新滑块检查
21.31 秒，两个语言的原八模板应用序列继续通过。
`out/p7-replacement-regressions.log`：四件配乐替换及旧 soundtrack／arrangement／
harmonic_city 路径，含两项音乐夹具共九项通过，49.92 秒。
审核账本追加历史保留的 functional 通过记录；四件当前为 functional／music
通过、visual／device 待审，品质合格计数仍为零，不增加内容数量。
