# P5 数据域转换实施

## 已实现增量与边界

`point.spectrum` 将共享音频输入的 63 个规范对数频段转换为有序点。
复用项目现有 `SpectrumBands` 和 `DrawSpectrum` 插值实现，提取为共享
`SpectrumSample`；没有新增 FFT、音频时钟或竞争解码路径。既有
`path.from_points`、GLM Catmull–Rom 重采样、点预览及矢量／管线节点继续复用。

采样点数 3–512，左右／单声道可选，首尾频段 0–62；反向范围表示反向取样。
按频段索引线性插值，不在此节点重新做 FFT。增益可由标量驱动，范围 0–100；
非有限驱动值使用零。幅度截到 0–1，映射位移和两端配色。点 ID 按采样顺序固定，
音频改变时发布新的不可变快照；点数改变后更新身份代次。无有效音频时保留线性／
环形基线，不输出无效坐标。保持与既有点域一致的坐标：X/Y 分别以画布宽高归一化。
因此宽屏直接绘制的环形为椭圆，转世界路径后使用明确的世界跨度和等比例投影；
需要匹配画布形状时使用现有点变换配置，不能暗中改变各数据域的单位。

完整频谱值参与运行时脏检查；相同输入复用点和路径结果。频谱节点向下传播动态
标志，普通矢量中间纹理可在最后消费者之后复用，最终输出和显式预览保持保留。
累计点预算仍由 `ValidatePointBudget` 控制，未新增无限数组类型或 GPU 原生句柄。

中英文目录／帮助和默认预设已接入，归入音频分类。Windows 模块和 Android 原生
合同通过，实际 Studio／PCM/GPU 作品也已通过。纹理采样到点属性仍待实现，不能
把本增量视为全部 P5.4 完成。

## 证据

- `out/p5-spectrum-graph-tests.log`：图合同通过。
- `out/p5-spectrum-runtime-tests.log`：原有频谱、矢量运行时、纹理生命周期和源码
  边界通过；新测试首次错误要求最终输出别名可以回收，断言失败。最终输出必须
  保留，修正夹具为带后续消费者的中间矢量纹理，没有放宽资源规则。
- `out/p5-spectrum-intermediate-target-tests.log`：修正后的夹具因测试音频缺少有效
  采样率／代次而被 `runtime.external_inputs` 拒绝；补齐真实输入合同后重测。
- `out/p5-spectrum-valid-audio-tests.log`：频段端点／插值／左右声道、有限增益、
  静音基线、不可变点、完整点→路径→填充、缓存失效和中间纹理复用通过。
- `out/p5-spectrum-content-tests.log`：新增算子默认预设覆盖通过。
- `out/p5-spectrum-android-build.log` 与 `out/p5-spectrum-android-tests.log`：NDK
  当前测试程序通过 USB 实机执行同一组合同。Null 后端不作为实际像素证据。

## 真实 Studio 创作

`out/p5-spectrum-authoring-tests.log` 的 `from_empty_studio_spectrum` 通过。
运行 `out/windows-release/from-empty-studio-spectrum/2010e3e9cffc4db98b9dce2372def787/`
完成 24 节点逐个添加、属性输入、命名／绑定、字体与许可导入、演示音乐绑定、
视图变换、组件封装及保存／发布／重开，当前编译代次与保存 recipe 均核对。
同一运行包真实 PCM/GPU 对照：音乐／静音平均 RGB 差 2.38057，低频／静音
3.05005，高频／静音 0.44463，低频／高频 3.49469。24 条指令全部可达。
人工查看音乐图：带孔轮廓和辉光正确，完整“频域花冠 / SPECTRAL COROLLA”标题
可读。源工程由通过验收的产物提取，仍为功能示例，不计入 P7 高品质审核数量。

这个增量已通过 Windows Studio 和 Android 原生合同。Android 应用内选择、同包
GLES 音乐检查、MP4 导出和目录缩略图将在纹理采样增量一同交付时完成，不提前
报告已覆盖安装本作品。
