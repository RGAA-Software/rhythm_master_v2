# 首批校准作品宽频段修订

范围：墨潮、织光机、共振拱廊 0.1.0 → 0.2.0。
复用 [瓷光钟摆的频段覆盖修复](porcelain_frequency_coverage_2026-09-10.md)，
不更换音乐、视觉构成、演出宏或测试阈值。

## 原始失败

七组真实 PCM／十项实际 D3D11 图像比较，在 700 Hz 中频／静音项失败：

| 作品 | 原指令数 | 原源 SHA256 | 中／静音差值 |
| --- | ---: | --- | ---: |
| 墨潮 | 34 | `941193819ec6e7e859c31d05df153feab55e63860162d5e3494f6b3238211a99` | 0.0167068 |
| 织光机 | 206 | `e64e014fa62510f030954022c1c662bbf64ac98ed1591bc36db6121d97994109` | 0 |
| 共振拱廊 | 156 | `d237c81b06dff5da3331b9854843ef4cf415e3fc9f31892052c4d093f4ee3895` | 0 |

原包摘要、执行程序和 WAV 摘要、七张图及完整差值分别在
`out/p7-ink-quality/music/`、`out/p7-loom-quality/music/`、
`out/p7-arcade-quality/music/`；失败日志对应 `out/p7-{ink,loom,arcade}-quality-pcm.log`。
墨潮底部频谱贡献了很少的像素变化，主体中频仍未被驱动，不能把“非零”当作通过。

## 修订与 Windows 对照

三件作品均将根图三个窄带替换为覆盖全部 63 FFT 带的可编辑分组峰值，
由 `tools/audio_band_groups.py` 构造；无需修改 Runtime 或新增分析依赖。
共振拱廊内嵌 Prism fold 定义保持原样，仍自带两条局部频带，故展开共 65 条音频带。
所有新增源码为本项目图组合，来源分别记入三个 provenance 文件。

| 作品 | 新指令数 | 音乐／静音 | 中／静音 | 弱／强 | 稳定纹理字节 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 墨潮 | 125 | 7.60726 | 28.93971 | 3.03044 | 29,491,204 |
| 织光机 | 297 | 0.99792 | 1.21605 | 0.25526 | 49,350,852 |
| 共振拱廊 | 247 | 6.68069 | 9.75206 | 1.79008 | 77,365,444 |

三件的全部十项都通过原 0.15 门槛，纹理字节与各自原图一致。
日志 `out/p7-{ink_tide,chromatic_loom,resonant_arcade}-wide-quality.log`，
输入身份、图像、全部差值位于 `out/p7-wide/{作品名}/music/`。
这些输入身份是在更新缩略图前记录的包；最终包还须核对作者源码／缩略图身份。
实际音乐静帧已人工查看，纸色墨绿岸线、青金织面和拱廊空间结构保留。
未把图像差值大小当作视觉品质分数，也未由此承诺声音与形变严格线性。

实际 GPU 缩略图已更新：`out/p7-batch-wide-thumbnails.log`，
`out/catalog-thumbnails/17bf42487cc14adb9aa936436d5f731c/`。
尚不计为公开控件／参数极端值／资源替换的完整 P7 品质通过。

## 两端交付

当前 APK SHA256 `7dc8da1964b6408e16a323a587faae834f083823c90f6b2e5a571fd7a5f08f80`，
`out/p7-batch-wide-android-build.log` 完成作者内容重建，
`out/p7-batch-wide-install.log` 记录覆盖安装成功。

| 作品 | 最终源 SHA256 | 最终包 SHA256 |
| --- | --- | --- |
| 墨潮 | `8977673eb614a7db3636565f1e25a618a779e1a2f61c07d7b7379cb6fe860f12` | `b2c8aba387875b6ba6826cd2d68406c3f186ff694c9bdb733a89295cc6ce9c63` |
| 织光机 | `32da24da5ba9013dac94c1b192c17f8d2f3017052bf8fb49399205f730440d38` | `90a013c407d32ff20eb595e1b024234334f9cbbd3ab207080e5d1ecc47155fb2` |
| 共振拱廊 | `2774b5790df40cd0e456e5d690b0caa467fea4bbc81c74aab436c219a51ca465` | `cac7cb35594b562985911c75d481b4b534ab24e09864c82bbf9a32f9c947b8cb` |

Windows 完整 Studio/Player deploy 与五项强制检查全部通过，47.78 秒，
含八模板双语言实际应用路径；`out/p7-batch-wide-windows-delivery.log`。
三件实际 Studio 16 秒、480 帧有声导出均已完成：

- 墨潮 `out/p7-wide/ink_tide/export/659183036350800/Exports/`。
- 织光机 `out/p7-wide/chromatic_loom/export/659212375387800/Exports/`。
- 共振拱廊首次导出文件共享冲突失败，保留原始证据，修复后产物在
  `out/p8-export-metadata/arcade/659630703758800/Exports/`。
  详见 [导出进度共享修复](export_progress_sharing_2026-09-10.md)。

实际输出及各自八时刻联系图 `out/p7-wide/{作品名}/motion.png` 已人工核对。
墨潮有墨量／岸线发展，织光机维持编织主体、光梭与方向变化，拱廊维持纵深、
纹理与亮度变化。两件高级作品的段落形态丰富度仍待视觉审核，不因节点多而通过。

Android 每件真实包内编排／静音各 960 帧，640×360，四时刻均通过原阈值：

| 作品 | 2/6/10/14 秒 RGB 差 | 音乐 p50/p95 ms | 纹理峰值字节 |
| --- | --- | --- | ---: |
| 墨潮 | 3.31012 / 14.9113 / 19.4121 / 10.7638 | 14.2434 / 17.7912 | 7,833,604 |
| 织光机 | 0.76092 / 0.996019 / 1.75523 / 0.963194 | 18.1581 / 22.4042 | 19,398,852 |
| 共振拱廊 | 3.34306 / 8.56836 / 10.9478 / 7.75581 | 19.0272 / 23.7107 | 20,687,044 |

日志 `out/p7-{作品名}-wide-android-music.log`，原生身份与图像分别为
`out/android-music/46ccf48b8a2e45208be030deabc3eecf/`、
`out/android-music/12d8bdad753648bdb2da8df96a61fe0a/`、
`out/android-music/82e7a25973dc487fac144cc1d6443dee/`。
这是原生离屏同步短测，不是应用 FPS、纯 GPU 时间或热稳验收。

当前 APK 内置选择、16 秒时间显示、暂停／恢复截图已逐件人工核对，
标题和输出一致、恢复后时间与画面变化，已保存节目单未变。
日志 `out/p7-batch-wide-authored-ui.log`，目录
`out/android-authored-works/b76cf21cc44c46c8b9fc3fc0defd4938/`。
