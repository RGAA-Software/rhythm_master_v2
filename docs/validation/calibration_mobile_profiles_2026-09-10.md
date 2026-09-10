# 四件校准作品的手机档位短测

USB `e2b3b128`，Redmi K40S／Adreno 650／Android 14。
源与包身份记录在 `out/android-template-measurements/16e6dd71f0cd487bad0daf1642e2abb9/results.json`，
均为本次当前 125／297／211／247 指令版本。

## Balanced 原生离屏

运行前停止本项目 Player，避免它与原生测量竞争 GPU。复用
`measure-android-templates.py --balanced`，每件 420 帧，预热后 300 帧统计。
960×540，合成音频特征驱动，`glFinish` 同步，耗时包括 CPU 图执行和 GLES 完成等待，
不是纯 GPU 时间、真实 PCM 音乐验证或应用显示 FPS。

| 作品 | P50 ms | P95 ms | 稳定纹理字节 |
| --- | ---: | ---: | ---: |
| 墨潮 | 19.4922 | 23.1334 | 16,588,804 |
| 织光机 | 18.0011 | 21.0140 | 31,610,052 |
| 瓷光钟摆 | 13.8986 | 16.0259 | 25,318,596 |
| 共振拱廊 | 26.5415 | 31.6986 | 43,698,244 |

四件均无预算拒绝、无 GLES 错误、统计区间纹理字节不增长。
共振拱廊接近 33.3 ms，不能据此承诺稳定 30 FPS，更不承诺 60 FPS。
日志 `out/p7-calibration-balanced-measurements.log`；原生执行程序 SHA256
`10b258092724f85e207547b7e27c7993e96a0e4cb344ce5069b298f758c71e34`。

## 当前安装应用：Original

实际保存的 `render_quality=0`，即 Original；没有在测试中改写偏好。
虽然本次 UI 日志文件名含 balanced，应用检查不是 Balanced 画质。
`out/p7-calibration-balanced-app-ui.log` 和
`out/android-authored-works/b9d36df8839649a096cf31051fa9fc91/`
保留目录实际选择、暂停／恢复和原始截图。

安装 APK 与本地包摘要相同：
`7dc8da1964b6408e16a323a587faae834f083823c90f6b2e5a571fd7a5f08f80`。
四件暂停／恢复截图均已人工查看，中文与英文标题正确、总长 16 秒，
暂停后恢复时间前进且主体图像变化：墨潮 3.23→4.50，织光机 2.91→3.90，
瓷光钟摆 2.90→3.90，共振拱廊 3.10→4.32 秒。
图像与作品一致，没有默认渐变回退；原保存演出列表字节不变。
脚本原始 `presentation_review_pending` 保留，本段记录人工复核结果。

这是小包作品的短功能证据，发生在随后发现的 16–32 MiB 单首音乐分流修复之前。
不把它当作该修复后的 APK、大音乐包、声学输出、温控长稳或最终设备阶段全面通过。
