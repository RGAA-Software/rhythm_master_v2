# 表面 Shader 音乐作品集成验收

状态：P6.1 受限表面材质功能闭环通过。作品：光谱釉球 / Spectral Glaze。
视觉品质数量、声学验收及长稳仍分别属于 P7／P8／P9。

26 个节点从空白图通过 Studio 节点分类逐个添加，以命名信号连接材质、音乐频段、
几何、灯光和输出。RGB 表达式在面板输入并通过同一个后台编译服务生成 Windows／
Android 产物；低频改变条纹相位，高频改变色带，整体能量控制材质发光。
这是 P6 功能退出示例，尚未通过 P7 的独立视觉品质审核。

验收必须包含：

- 编译成功后实际作者代次变化、保存资产中的源码与输入一致。
- 非法表达式有明确诊断，不提交新资产；修正后产生当前图的输出。
- 撤销／重做、保存重开后源码仍正确，不能以保留旧资产 ID 充当成功。
- 当前发布包的音乐／静音／低频／高频实际 GPU 图像对照。
- Windows 有声导出与完整 deploy；Android 当前 APK 包身份、GLES 音乐对照、
  应用内直接选择与暂停恢复的可见画面。

集成过程首次测试目标缺少 surface_shader 显式依赖，构建失败记录在
`out/p6-surface-author-work-build.log`；补依赖后暴露缺少 assets/store.h 直接包含，
记录在 `out/p6-surface-author-work-dependency-build.log`。这两次是测试构建错误，
不是用户已执行成功的证据。

完整交付检查还在 `out/p6-surface-author-work-includes-build.log` 拦截了新节点缺少
默认预设的问题（`Missing default preset: material.shader`）；补齐恢复默认记录，
不降低全算子覆盖规则。这条记录是节点默认值操作，不计入 P7 视觉独立预设数量。

## 当前实际结果

`out/p6-surface-author-work-default-build.log` 的完整 Studio deploy 和五项强制检查
全部通过。首次完整创作记录为
`out/windows-release/from-empty-studio-surface/b04c59a85cb04d88857cb2692494f8ef/`。
加入失败热更与源码恢复检查后的成功记录为
`out/windows-release/from-empty-studio-surface/15051c326cd44e2992ccd50304a41d33/`，
日志 `out/p6-surface-hot-reload-tests.log`。

真实 Studio 输入 `vec2(uv)` 后产生 D3D 类型诊断，没有变更作者代次；改为合法
染色表达式后产生新代次并安装当前图。撤销／重做／撤销分别核对输入框完整源码，
保存资产解码核对输入表达式，重开项目后再次核对源码。已查看错误和重开截图，
节点预览仍是球体，没有用默认渐变替代。

同一发布包的 Windows D3D 实际 PCM 对照，平均 RGB 差分别为：音乐／静音
3.61747，低频／静音 16.49254，高频／静音 16.20374，低频／高频 2.59713。
已查看音乐和高频图：釉色球体有条纹、高光和金属轨道。此处只确认功能及基本可见
构图，尚未把截图审核当作动态运动品质验收。

首次作品包在 USB Android 原生 GLES 的 240 帧音乐／240 帧静音对照见
`out/p6-surface-first-work-gles.log`：差值 3.71246，640×360，峰值纹理 8,755,204
字节；未使用实际 APK，不能替代后续内置 UI 验收。

旧版 Windows Player 的实际拒绝见 `out/p6-surface-legacy-player-rejection.log`：
Player SHA256 `5c5f291da6d0effba1503ac65bc6d302b9bf1a38bf346a11351d5c9ce389597a`，
工作包 SHA256 `3f0132dcf045be03169d195ac38913028105c99e475fd5e4892555e1a95ef1c3`，
退出码 1，错误 `package.operator`，未进入播放。旧 Player 不认识新节点，需升级。

看图另外发现共享字段 a/b 和 asset 被误译为“底图／叠加”和“模型资源”。
已统一节点端口、属性、命名绑定及帮助的字段语义：Shader 使用 a/b/c/d 与 Shader
资源，普通合成节点保留其既有含义；稳定属性键、控件 ID 和发布格式不变。

修正标签后的最终作者证据为
`out/windows-release/from-empty-studio-surface/6258125523f14d8dba571e90fe3a4833/`，
`out/p6-surface-label-authoring-tests.log` 通过；已查看标签截图。
作品提升为内置 `spectral_glaze` 0.1.0，保留可编辑图、表达式资产、配乐和作者证据。
包 SHA256 仍为上述 `3f0132...1c3`。

`out/p6-surface-work-export.log` 通过 640×360／30 FPS／120 帧的有声 MP4、相同输入
重复像素一致、静音不同、音频回读和取消；音频 MSE 为 7.6297e-06。导出位于
`out/p6-surface-work-export/647224627643300/`，已查看每秒两帧的八张时序图，
表面色带有连续变化。四秒时序不能代替 P7 段落发展和完整音乐品质审核。

`out/p6-surface-current-player-smoke.log` 为新版独立 Windows Player 的实际 30 帧
GPU／包内配乐检查。`out/p6-surface-template-delivery-build.log` 的强制双语言模板
回归现已固定包含 `spectral_glaze`，验证连续切换后 ID 重映射、当前输出、保存与发布。
已查看 `out/windows-release/template-switch-gpu/647830745471200/zh-CN-spectral_glaze.png`：
当前作品标题、波形和釉球最终输出正确，不是之前模板或默认渐变。

APK 首次交付 SHA256 为
`4c4356c5cb6e52f91a7d09fbcc951e924ffbb35c66ec5731cc2a37c9d1ea7a69`，包含 52 件
内置模板；数量仍不等于 P7 品质数量。覆盖安装见 `out/p6-surface-android-install.log`。
`out/p6-surface-android-music.log` 从实际 APK 提取并核对当前包，音乐和静音各
960 帧，2／6／10／14 秒平均 RGB 差为 1.66027／3.81584／1.49132／3.83459，
峰值纹理 8,755,204 字节，640×360。音乐 p50／p95 为 5.3212／9.36245 ms，
仅是同步离屏短检查，不是手机屏幕帧率或热稳定性结论。

Android UI 脚本首次因硬编码作品查询表缺少新作品而失败，见
`out/p6-surface-android-ui.log`；改为从实际 APK 英文标题选择有区分度的查询词，
保留精确中文标题选择检查。`out/p6-surface-android-catalog-ui.log` 随后通过，
证据 `out/android-authored-works/88b3c659121049519679c782d4d964b0/`。
已逐张查看暂停／恢复图：标题、彩色球体和轨道正确，16.00 秒配乐，播放位置由
2.38 前进到 3.63 秒，RMS 非零，节目单未变。脚本的 presentation_review_pending
保留，图像核对是人工执行；没有声学回录结论。

## 最终交付身份

兼容性元数据已注明 Windows 和 Android，最后构建分别为
`out/p6-surface-platform-metadata-windows-build.log`（含五项强制回归）与
`out/p6-surface-platform-metadata-android-build.log`。当前 APK SHA256：
`5e8730d01e1e67d1bc3d51bf871a7114055f31bf32e54ae6b1c0415cba72640a`。
`out/p6-surface-final-apk-identity.json` 核对最新来源摘要及包 SHA256，与上述
960 帧 PCM 检查的运行包字节完全相同；仅兼容性元数据更新，没有修改图与资产。
覆盖安装见 `out/p6-surface-final-android-install.log`，本次安装仍保留用户数据。

最后应用 UI 检查为 `out/p6-surface-final-android-ui.log`，证据
`out/android-authored-works/192d90f69bce4882b6079f02d1f121d1/`。已查看暂停／恢复
截图：安装 APK 哈希匹配、标题和釉球轨道正确、配乐 16 秒，位置 2.32→3.47 秒，
状态由暂停变为播放且 RMS 非零，节目单未变。脚本原始人工待审字段保留。

完整 Windows 目录：`out/windows-release/src/windows_spike/deploy/`（Studio）与
`out/windows-release/src/windows_player/deploy/`（Player），均含所需 20 个 DLL 和
资源。Studio 的 shader_tools 包含表面编译所需 varying／include。新作品保留
功能示例身份；不把闭环测试或 52 件目录记录当成基础／高级各 50 件品质目标完成。

已有模块级证据见 [材质实施记录](../material_shader_evaluation.md)。
已有图像 Shader 源码恢复缺陷及漏测原因见
[源码恢复回归](shader_source_reopen_2026-09-10.md)。
