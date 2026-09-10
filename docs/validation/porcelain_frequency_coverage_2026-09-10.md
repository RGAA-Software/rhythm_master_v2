# 瓷光钟摆：中频响应空缺与取景修订

状态：0.3.0 频段／取景修复完成 Windows 和 Android 短交付验证；不计为 P7 完整品质验收。

## 发现与根因

P7 新增真实 PCM 七组对照：原演示音乐、静音、80 Hz 低频、3500 Hz 高频，
以及 700 Hz 中频、原演示音乐 0.25 倍和 1.25 倍音量。原四份 WAV 字节完全
不变；强音量峰值 32332/32768，没有削波。七组均经现有 FFmpeg 解码和分析器，
在相同场景第 4 秒截取实际 D3D11 输出，十项比较沿用 0.15 RGB 差门槛。

120 节点的取景修订在中频／静音一项得到精确 0，其余九项通过。
失败日志 `out/p7-porcelain-quality-pcm.log`，七张 TGA/PNG 与全部差值
`out/p7-porcelain-quality/music/`。失败不会在生成图像后被吞掉。

作者原图只读 FFT 频带 12、28、48，却把三个窄带命名为低／中／高频。
分析器使用 20 Hz–16 kHz 的 63 个等比频带；700 Hz 不落在所选窄带中。
这是作品频段覆盖缺口，不能通过换测试频率或降低门槛解决，也不是媒体解码失败。
先前只有低／高频测试，包内编排混有多个频率，因此都未暴露该问题。

## 修订

`audio_band_groups.py` 复用现有 `audio.band` 和三输入 `scalar.expression`
组合完整、互不遗漏的 0–23、24–42、43–62 三组峰值，约对应 20–255、
255–1917、1917–16000 Hz。没有新增 DSP、依赖或 Runtime 操作码。
组内峰值保留窄谐波，已有有界响应宏控制放大；内部节点可编辑。
`test-audio-band-groups.py` 逐一激励全部 63 个输入，验证每个只到达对应组，
静音三组为零；实际包编译验证表达式合同。作品变为 211 节点、257 条边。

构造时曾误用四输入表达式，现有合同拒绝 `d`（`expression.name`）；已改为
三输入归约，保留 `out/p7-porcelain-broad-band-build.log` 的失败及
`out/p7-porcelain-broad-band-contract-build.log` 的成功，未扩展表达式接口。

Windows 七组对照通过：音乐／静音 1.41463、低／静音 0.93880、
高／静音 0.76331、低／高 1.69083、中／静音 0.69253、中／低 1.60775、
中／高 1.45430、弱／静音 1.34783、强／静音 1.41456、弱／强 0.50314。
纹理稳定 43,059,396 字节，与 120 节点版本一致。
日志 `out/p7-porcelain-broad-band-quality.log`，图像与十项差值
`out/p7-porcelain-broad-band/music/`。这证明所选输入的响应与区分，
不证明任意音乐听感、全频连续扫描或声音大小与画面变化严格单调。

## 取景

相机提高到 y=6.1，以同一目标看向阴影舞台，让地面远端和背景的硬边退出
画面。实际静帧已人工确认。一次放大地面到 200 超出现有属性上限而被拒绝，
保留 `out/p7-porcelain-framing-edge-content-build.log`；修订保留尺度 100，
通过相机解决，不提高图属性限制。此前视点 y=5.5 的右上角仍有残余硬边，
对应 `out/p7-porcelain-framing/music/`；最终取景验证在
`out/p7-porcelain-framing-final/music/`。

## 最终交付

源 SHA256 `6ce6205001ea20cad1536449897d57d8cdd42c5c0c1a9d468c9758dd2324bc9f`，
包 SHA256 `a9bb122853041d6291d2bbf4a479c8eff73a4b4fb5b088ba0e861546ba26c146`，
APK SHA256 `e64c6d109db62680bfd7db91be4c15d86297d07beb4130475f888ed1f940b6df`。

- Windows 两个完整 deploy 已更新；五项强制验收均通过，含双语言八模板实际应用。
  日志 `out/p7-porcelain-broad-band-delivery.log`，总检查 42.81 秒。
- Studio 导出按钮完成 16 秒、480 帧包内编排有声 MP4：
  `out/p7-porcelain-broad-band/export/658526037767400/Exports/音画验收.mp4`，
  日志 `out/p7-porcelain-broad-band-export.log`。人工核对八时刻联系图
  `out/p7-porcelain-broad-band/motion.png`，硬边不再出现，摆杆、内环形态随段落发展。
- Android 已 `install -r`；当前 APK 包内真实 PCM／静音各 960 帧，640×360，
  四时刻差值 0.697865 / 1.47441 / 1.24521 / 1.31855，原门槛全部通过。
  音乐 p50/p95 13.6665/17.2066ms，纹理峰值 13,107,396 字节。
  日志 `out/p7-porcelain-broad-band-android-music.log`，身份、图像与原生日志
  `out/android-music/88e09ddde2b6489bb23e85e1c858b5ae/`。这是短离屏同步测量。
- 当前 APK 内置选择、暂停／恢复、16 秒时间显示人工截图核对通过，输出为
  最新取景；恢复后时间与摆杆形态改变，已保存节目单未变。
  `out/p7-porcelain-broad-band-ui.log`，截图
  `out/android-authored-works/ecba234e3d3f4125a17b0f64a665efab/`。

公开控件极端值和换资源的完整 P7 工作流仍待审。Balanced 960×540 的旧数据
属于 120 节点 0.2.0，不能沿用为本次 211 节点的测量。

## 永久验证

`create-music-fixture.py --tones --quality` 与 `test-music-gpu.py --quality`
保留原四输入默认检查，同时提供中频／响度扩展；先保存全部差值再报错。
后续运行还在 `input-identity.json` 记录包、执行程序和每个 WAV 的 SHA256。
原有模板不能因这一个作品修复而自动通过；逐批使用相同扩展检查并审核
参数极端值、公开控件、换素材和两端修改交付路径。

后续同门槛扩展检查发现墨潮中／静音仅 0.0167068，织光机为 0：
`out/p7-ink-quality-pcm.log`、`out/p7-loom-quality-pcm.log`，各自目录保留
`input-identity.json`、七张图与全部十项差值。这两件仍待修订，不因旧四输入
检查或本次瓷光钟摆通过而算作全面音乐审核通过。
