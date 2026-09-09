# 表面 Shader 音乐作品集成验收

状态：进行中，尚未交付。目标作品：光谱釉球 / Spectral Glaze。

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

已有模块级证据见 [材质实施记录](../material_shader_evaluation.md)。
已有图像 Shader 源码恢复缺陷及漏测原因见
[源码恢复回归](shader_source_reopen_2026-09-10.md)。
