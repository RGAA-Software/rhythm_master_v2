# P7 瓷光钟摆开场响应不足

状态：已复现，修订中。原作品0.1.0，源SHA256
`fff0c61478ac3d685205d17cfa101f455f8309ce0c1ab96947e32f4796f66261`，
包SHA256 `ce30662a75694253a267ef730ed563f5938311464d29e9fac56bf873420227c9`。
当前APK `3cca4bf3433f3bb16aacf5bbb072f2a33381c8b3ff3971c03be2fb520a0dcb6d`。

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
