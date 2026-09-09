# 矢量共振：从空白创作验收

## 当前范围

新增路径填充与描边节点，复用 Path、Clipper2 与 Earcut；详见
[矢量实施合同](../vector_graphics.md)。本作品用于验证创作链，不计入 P7 已审核
高品质模板数量。两端与导出按下面各自证据验收，不把 Windows 结果自动推广到
其他路径。

Android 首次音乐检查也保留失败：`out/p5-vector-android-music.log`，运行
`out/android-music/b015cbebcc62486990ceecaf9f277b99/`。APK 及包内容身份正确，
独立 `android_gpu_contract_tests` 未随 APK 目标重链接，旧 Registry 报
`package.operator`。已将原生测试目标的增量构建加入 `test-android-music.py`
每次检查入口，并保留构建日志；不通过重复运行旧二进制或忽略未知算子恢复检查。

## 失败保留

首次实际 UI 运行在第 18 个节点失败：recipe 将 `texture.noise` 的
`noise_scale` 写成 `scale`。测试按 Registry 检查属性，明确报
`recipe property missing: scale`，没有静默忽略错误配置。
保留 `out/p5-vector-authoring-tests.log` 和失败运行目录
`out/windows-release/from-empty-studio-vector/38f0d1f17f38427bb37ca7bc109d195a/`。
修正 recipe 后重新运行实际工作流，未跳过断言，也未复用失败产物。

## Windows 实际路径

`out/p5-vector-authoring-property-tests.log`：`from_empty_studio_vector` 通过。
运行目录 `out/windows-release/from-empty-studio-vector/9f3e0bbc2422466897cbd386ce794218/`。
使用真实 Studio 界面删除初始节点，按 recipe 添加 23 个节点、设置参数／颜色、
命名并绑定输入、播放并绑定原创演示音乐，检查当前编译代次；随后直接变换最终
构图、封装组件、保存、发布和重开。保存内容与 recipe 逐项核对。

同一发布包由 FFmpeg 解码真实 PCM，D3D11 在相同时间比较四种音频输入。
平均 RGB 绝对差（0–255）：音乐／静音 4.69472，低频／静音 8.33336，
高频／静音 5.15870，低频／高频 11.69783。完整 23 条指令可达。
人工查看 `music/resonance_demo.png`：青色填充环保留中心孔洞，金色开放圆弧和
粉色波形正确出现，柔光和背景合成正常；不以非黑像素数量代替画面检查。

这是单个时间点的音乐对照与短操作验收，不是声学回录、长稳或竞品画质等价证明。
命令记录与 recipe/hash 位于运行目录，内置源工程由
`tools/promote-authoring-work.py` 核验成功产物后提取，没有直接拼装绕过 UI 的工程。

## 交付与导出

`out/p5-vector-windows-delivery.log` 完成 Studio/Player 及各 20 个 DLL 和资源的
deploy，五项必需回归通过。实际 D3D11 缩略图生成后再次增量部署，
`out/p5-vector-thumbnail-delivery.log` 五项回归也全部通过。
目录缩略图来自 `out/p5-vector-thumbnail.log`，采用规范合成音频特征，不能当作
上述真实 PCM 验证。

`out/p5-vector-export.log` 使用同一运行包和共享 FFmpeg 后端，导出
`out/p5-vector-export/633934592670400/music.mp4`：640×360、30 FPS、120 帧，
重复导出的逐帧结果一致，静音版本不同；音频回读 MSE 7.6297e-06，取消清理通过。

Android 构建记录 `out/p5-vector-android-delivery.log`；当前 APK SHA-256 为
`1fee037dd33d9f12fffdc009fa51a2f0fae7947af2816e84af73664d9883e21f`。
`out/p5-vector-android-install.log` 记录 `adb install -r` 成功，未卸载或清除数据。
当前独立检查程序重新编译后，`out/p5-vector-android-current-checker.log` 通过，
运行目录 `out/android-music/1728fbc3382b48589efa821fbfbfc017/`。
APK 内部包与当前源工程身份已核对；音乐／静音各 960 帧，640×360 GLES 渲染、
320×180 读回，在 2/6/10/14 秒的平均 RGB 差为 1.29590/5.05192/1.29455/5.42001。
音乐最大 RMS 0.236431，峰值纹理 9,100,804 字节；原生同步执行 p50/p95 为
16.0401/17.696 ms，不是屏幕 FPS，也不是 GPU 单独计时。
`out/p5-vector-android-ui.log` 与
`out/android-authored-works/5dcc2f541c5a4d2fa4e509ecae3c9dda/` 记录安装包身份核验、
应用内检索和唯一作品名称选择，以及暂停／恢复。人工查看 paused/resumed 截图，
作品标题正确，孔洞、弧线和波形与 Windows 结构一致，按画布选择横屏；时间从
2.24 秒前进到 3.07 秒、RMS 非零。原有演出列表字节保持不变。
脚本输出保留 presentation review pending，本段是实际截图核对结论，未将自动
截图动作本身当作呈现通过。
