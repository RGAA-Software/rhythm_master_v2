# P7 瓷光钟摆开场响应不足

状态：0.2.0 音乐响应修复已验证，视觉品质与最终应用验收仍单独追踪。原作品0.1.0，源SHA256
`fff0c61478ac3d685205d17cfa101f455f8309ce0c1ab96947e32f4796f66261`，
包SHA256 `ce30662a75694253a267ef730ed563f5938311464d29e9fac56bf873420227c9`。
失败基线APK `3cca4bf3433f3bb16aacf5bbb072f2a33381c8b3ff3971c03be2fb520a0dcb6d`。

119条指令、640×360，真实包内编排音乐／静音各960帧：GLES正常输出，
最大RMS0.151579；第2秒平均RGB差0.132124，低于既有0.15门槛，检查失败。
音乐p50/p95为11.5576/15.0265ms，纹理峰值13,107,396字节；不是性能超预算。
日志`out/p7-calibration-porcelain_pendulum-android.log`与
`out/android-music/7953282010804fb888a3603fd62f18cb/device.log`保留。
原生检查已经写出全部八份PPM，失败后手动拉回到同目录，未重跑选择有利结果。

Windows通用演示音乐第4秒差0.6569以及完整16秒编排导出通过，不能替代
Android包内编排第2秒的实际检查。作品开场Rest快照响应较低，频段值直接
乘宏后只轻微改变少量摆杆／内环像素。修订将加强有界频段映射及开场响应，
不改变测试阈值、素材或检查时间来绕过失败。

人工查看完整音乐静帧及导出1/3/5/7/9/11/13/15秒联系图，还发现陶瓷主体
与地面背景明暗接近，画面显灰。品质审核暂不通过；同时调整光照／背景层次。
证据`out/p7-calibration/porcelain_pendulum/music/resonance_demo.png`、
`out/p7-calibration/porcelain_pendulum/motion.png`；导出原文件在
`out/p7-calibration/porcelain_pendulum/export/654676766046300/Exports/`。

永久工具修正：`test-android-music.py`现在在finally中尝试拉回所有检查点图像，
保留原始失败异常；成功但证据缺失也会报错。此前失败即退出导致图像留在手机，
会妨碍复盘。其他独立作品继续检查，不把本次失败归零成所有渲染能力失效。

## 0.2.0 修订与复核

频段映射改为有界放大，增强 Rest 开场快照，调整陶瓷材质、灯光、取景和
背景层次，增加 FXAA；120 节点、166 条边。保留原音乐、时刻和 0.15 门槛。
源 SHA256 `d191d306771c4b3a6ee13131aabfe22df725e64bb2969c7a5817c060157ac008`，
包 SHA256 `971cbc3fe32d57b169dd62f9cccd46eacf4afd562341a4ceae5281dca54180ed`，
APK SHA256 `fa3a8486a4ec275af58fea6c686f3f409be306c9a84aaa695b0a17316bd2825b`。

- Windows 外部真实 PCM 对照：音乐／静音平均 RGB 差 1.48035，低频／静音
  1.26597，高频／静音 1.25705，低／高频 2.52120。
  日志 `out/p7-porcelain-final-music.log`，图像 `out/p7-porcelain-final/music/`。
- 实际 Studio 包内编排经导出按钮完成 16 秒、480 帧有声 MP4；
  `out/p7-porcelain-final-export.log`，产物
  `out/p7-porcelain-final/export/655630267930400/Exports/音画验收.mp4`。
- Android 当前 APK 包内真实 PCM／静音各 960 帧，640×360，2/6/10/14 秒差值
  为 0.538403 / 1.32492 / 0.802627 / 1.13435，全部通过原门槛。
  音乐 p50/p95 11.9707/15.7453ms，纹理峰值 13,107,396 字节。
  日志 `out/p7-porcelain-final-android-music.log`，原始八张图与设备日志
  `out/android-music/4d4c39b46cf74d2185b7be876b003212/`。
- Balanced 960×540 原生短测，420 帧中记录后 300 帧，同步耗时 p50/p95
  12.4386/15.9282ms，稳定纹理 25,318,596 字节。
  `out/p7-porcelain-final-balanced.log`。这是离屏同步耗时，不是纯 GPU 时间、
  实际应用 FPS、声学或热稳验收。
- Windows 完整 deploy 及强制五项检查通过，包括八模板双语言实际应用路径；
  `out/p7-porcelain-revision-windows-build.log`。Android 已覆盖安装，安装日志
  `out/p7-porcelain-final-install.log`。首次界面测试被来电打断，保留中断记录。
  后续当前 APK 内置目录选择、16 秒时间显示、暂停与恢复已完成，并人工核对
  实际输出为钟摆，恢复后时间及摆杆姿态改变。日志
  `out/p7-calibration-authored-ui.log`，截图与包身份
  `out/android-authored-works/3fc5e38a6e444d67aec8ea201d50948a/`。
  同次墨潮、织光机、共振拱廊的暂停／恢复截图也已核对，各自输出正确，
  保存的节目单不变。这只证明短播放路径，不包含换素材和全部公开控件检查。

人工复核当前静帧与八时刻联系图：主体明暗分离改善，但地面与天空的硬边界
仍影响画面完整性，视觉审核继续 pending。音乐修复通过不能替代参数极端值、
中频、不同响度、换音乐和应用工作流的完整品质审核。原始失败记录保留。
