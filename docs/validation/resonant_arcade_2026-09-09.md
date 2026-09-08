# 共振拱廊功能交付

第一方高级作品候选：146 个主图节点、209 条主图连线，嵌入 11 节点的
官方“棱镜折叠”组件，发布后为 156 条可达指令，画布 1280×720。
七组青金拱门、十二块纹理屏幕、地面导光线和远端光环形成纵深建筑构图。
共享路径管线、立方体、PBR、三盏灯、环境光照、景深与图像处理沿用既有算子。
组件定义及内部布局随工程保存，可进入组件编辑，不需要外部组件安装。

低频驱动拱高与终点环，中频改变纹理，高频增强发光；响应、节奏、曝光三个
公开宏、三套快照与四段 Cue 可编辑。复用光幕协奏的两份原创 FLAC 和
16 秒四片段编排，资产完整性由公共作者工具检查。
来源记录为 `provenance/resonant_arcade.json`。

首轮实画检查后降低屏幕纹理密度，并加轻微高斯滤波。
这是局部画面柔化，不是 MSAA、FXAA 或时域抗锯齿；远处细线的抗锯齿仍有提升空间。
屏幕为发光纹理实体，地面为 PBR 高光，不宣称真实玻璃折射或屏幕空间反射。

## 验证结果

- Windows 最终版本真实解码音乐/静音、80 Hz/静音、3500 Hz/静音、两音调间
  同一 4 秒场景时间的平均 RGB 差分别为 4.4842、11.8645、5.5715、8.9545。
  156 条指令全部可达；稳定纹理 84,738,244 B。
- 实际 Studio 导出按钮生成 16 秒、480 帧 H.264 与非静音配乐，逐帧时间戳正确，
  导出期间编辑器主图保持不变。输出为
  `out/r6-arcade-export/557844735400300/Exports/音画验收.mp4`。
  已查看最终音乐画面、静音对照和四个时段的导出画面。
- Android Redmi K40S / Adreno 650 的 GLES 非黑帧、两次设备重建检查通过；
  最终工程 60 帧峰值 26 pass、最后 22 pass，无预算回退。
  Balanced 960×540 短测 420 帧取后 300 帧，同步 p50 26.831 ms、p95 29.9755 ms，
  稳定纹理 47,845,444 B。这不是帧率或热稳定保证。
- APK 使用 `adb install -r` 覆盖安装。从应用内搜索 `Resonant Arcade` 得到
  1/43 项，选择“共振拱廊”后为横屏，实际播放 16 秒配乐、非零 RMS 和正确画面。
  演出控件窗口实际显示 Procession 段及 1.100、1.000、−0.200 三个自动宏值。
- 原有五个输入组件通过公共定义作者函数重新生成后，工程与预设字节不变。
  `template_contracts`、`semantic_catalog`、`content_contracts` 三项通过；
  Windows Studio/Player 经 Python 完整部署，缩略图来自最终运行包真实 GPU 渲染。

日志：`out/r6-arcade-music-final.log`、`out/r6-arcade-export.log`、
`out/r6-arcade-android-gpu.log`、`out/r6-arcade-android-balanced.log`、
`out/r6-arcade-contracts.log`、`out/r6-arcade-deploy.log`、`out/r6-arcade-apk.log`。
画面和操作证据：`out/r6-arcade-motion.png`、`out/r6-arcade-phone-playing.png`、
`out/r6-arcade-search.xml`、`out/r6-arcade-controls.xml`。

完整工程示例共 43 个，作者标签为 5 基础、14 高级、24 功能示例。
语义组件仍为 22 项、预设记录仍为 176 条。
`visual-review-pending` 保持；功能检查与作者候选数量不替代用户品质验收。
