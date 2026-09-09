# 从空画布创作验收

本页记录 P4 退出作品的真实创作路径。截至 2026-09-10，2D 和 3D 示例均已完成
实际 UI 添加节点、参数与配色、音乐绑定、命名连接、视图平移/旋转/缩放、组件封装、
保存重开和发布。两件作品的 Android APK 包内 PCM/GLES 对照通过；应用内操作的
截图审核独立记录，不用底层渲染通过替代应用验收，也不将功能示例计入 P7 品质数量。

## 脉动等高线 / Contour Pulse

配方为 `content/authoring/contour_pulse.json`，已保存的可编辑示例为
`content/templates/contour_pulse`。示例使用现有基础算子，属于功能示例，视觉审核待定，
不计入 50 个基础或 50 个高端品质模板的交付数量。

通过 Studio 界面清空种子输出后逐个创建 12 个节点：时间、两个音频频带及其映射、
噪声、万花筒映射、等高线、背景、合成、二维变换和最终输出。低频改变空间尺度，
高频推动等高线相位，时间继续驱动底图和缓慢转动。配色通过现有颜色选择器输入。
每个命名输出及输入绑定都与保存结果逐条核对，包括非必需的时间和音频输入。

从“播放演示音乐”实际加载原创素材，确认解码后的 RMS 非零，再通过“作品音乐”
绑定文件。保存的工程和发布包均包含这份音乐；没有创建第二条媒体后端。

选择二维变换作者，在最终输出实际拖动约 0.1 画布宽度，再旋转约 15 度并放大约 10%，
通过组件面板封装该变换。
发布结果必须与已保存工程编译出的程序一致；重新打开后必须安装新的当前计划，
不能沿用旧画面作为成功证据。

## 自动入口和证据

3D 配方为 `content/authoring/resonant_armillary.json`，可编辑示例为
`content/templates/resonant_armillary`。共振星仪从 21 个节点构成双金属环、灯光和
正交相机；低频驱动环体旋转，高频与 RMS 驱动发光。通过实际相机枚举选择、颜色
输入和命名绑定完成构图，再选取外层变换，在最终输出中完成三种视图操作。
初版原创音乐响应偏弱，加入 RMS 和有界映射后重新验证；当前仍是功能示例，
不宣称已达高级模板质量。

```powershell
python tools/build-windows.py --target from_empty_studio_tests
python tools/verify_windows.py --match '^from_empty_studio_2d$' --require from_empty_studio_2d --log out/from-empty-studio-tests.log
```

该入口串行执行 Studio 作者操作和发布包的 GPU 音频对比。后者复用现有 FFmpeg
解码/分析/Player 检查，在相同场景时间比较原创音乐、静音、低频、高频四种 PCM 输入。
每次记录配方快照、输入身份、工程修订、发布包、GPU 图像、像素差异和验收状态。
GUI 流程失败时不会继续将预制包作为替代结果。

2026-09-09 的已测记录：

- `out/p4-from-empty-bound-music-tests.log`：绑定音乐的完整 2D 路径通过。
- `out/p4-from-empty-recorded-recipe-tests.log`：不可变配方快照与同一发布包音频检查通过。
- `out/windows-release/from-empty-studio-2d/0180e95f6ed3488a8306471996182cc4/`：
  `recipe.json`、`workflow.log`、`acceptance.json`、工程、发布包和 `music/` GPU 图像。
- `out/p4-from-empty-example-delivery.log`：Studio 与示例交付，20 DLL 自动部署、
  四项强制模板应用回归通过。
- `out/p4-from-empty-final-tests.log`：加强重新打开的新代次检查后，完整流程和交互回归通过。
- `out/p4-contour-promoted-music-tests.log`：内置示例重新编译的四组 PCM 画面与作者发布包
  逐像素一致，避免转为模板时重新构图或丢失绑定。

缩略图通过现有 `tools/render-catalog-thumbnails.py` 从实际 Player 渲染生成；它使用
合成频谱特征，记录在 `thumbnail.json`，不替代上述真实 PCM 检查。

`tools/promote-authoring-work.py` 将通过验收的**实际保存工程**解码为可审查的
`graph.textproto`，保留布局和内容寻址音乐，记录证据与哈希；它不重新构造最终图。
使用 vcpkg 的 protoc，无新依赖，无上游源码修改。示例描述的变化和配方变化均必须
与对应验收快照一致，不能把旧通过记录套到新配方。

此处的音频证据是解码 PCM 与画面响应，不是麦克风/扬声器声学采集、音质评审或长测。
Android 状态继续独立记录，不能用 Windows 通过或兼容平台字段代替手机验收。

2026-09-10 增量证据（以下为包含三种视图操作的新修订）：

- `out/p4-from-empty-view-operations-tests.log`：2D/3D 完整作者 UI 和同一发布包的
  四组 PCM/GPU 检查通过；实际工程分别保存在
  `out/windows-release/from-empty-studio-2d/c3c27479744b45c5b32b6f70eb576a26/` 和
  `out/windows-release/from-empty-studio-3d/ec5d0fee0210462db586d50e1ed67ccf/`。
- `out/p4-both-authoring-windows-delivery.log`：两件内置示例与缩略图交付，四项
  强制模板应用回归通过。
- `out/p4-both-authoring-android-delivery.log` 和
  `out/p4-both-authoring-android-install.log`：宿主内容重建、47 件作品 APK 打包验证、
  `adb install -r` 覆盖安装成功。
- `out/p4-contour-android-music.log` 和 `out/p4-armillary-android-music.log`：从实际
  APK 提取包并核对哈希，手机运行音乐与静音各 960 帧，在 2/6/10/14 秒对照像素。
  两件作品均有音频响应。640×360 内部渲染的音乐帧 p95 分别约 10.16/3.18 ms；
  这是同步离屏检查，不是屏幕帧率、发热或长稳结论。

`tools/test-android-authored-works.py` 核对安装 APK 身份，通过内置目录搜索选择两件
作品，保存播放/暂停/恢复截图并检查节目单未变化。主窗口帧数持续更新导致系统
UI dump 无法等待空闲，因此脚本仅将目录操作判为通过，将画面、标题、16 秒音乐
和传输状态保留为 `presentation_review_pending`，必须另行审核截图。

本次应用证据位于 `out/android-authored-works/586a6e4604c24d158f71a53f992e5518/`。
已逐张查看两件作品的 `*-paused.png` 和 `*-resumed.png`：标题分别正确，显示等高线
和金青双环而非默认渐变；均显示 16.00 秒配乐及非零 RMS。暂停/恢复状态正确，
时间分别由 2.50 前进至 3.36 秒、2.27 前进至 3.28 秒，画面随之改变。
这是人工图像核对，脚本自身的待审状态不改写为自动通过；不据此宣称声学验收完成。
初次启动点击时序、SearchView 控件类型、IME 输入时序和主窗口空闲导出失败，
均保留在 `out/p4-authored-works-android-ui*.log` 及对应独立运行目录中。

## 这条路径发现并修复的问题

- [跨窗口释放鼠标取消输出编辑](validation/cross_editor_mouse_release_2026-09-09.md)。
- [连续新增节点堆叠](validation/node_palette_placement_2026-09-09.md)。

这些缺陷说明预先构造完整图并运行，不能覆盖实际从零创作。
