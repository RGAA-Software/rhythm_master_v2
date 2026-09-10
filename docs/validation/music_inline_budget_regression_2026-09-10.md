# 16–32 MiB 音乐发布回归

## 触发、根因与漏测

补四件校准作品的配乐替换检查后，复核原有大音乐／时间段 Studio 工作流，
24,576,044 字节、128 秒 WAV 已能绑定和保存，但反复发布报告
`embedded media byte budget`，两个实际 Studio 检查均失败。
原始日志 `out/p7-replacement-large-regressions.log` 保留。

`81cd880` 为完整 CJK 字体将普通素材总预算从 8 MiB 扩到 32 MiB。
`RequiresStreamedAudio` 仅用这个总预算决定单曲是否走流式路径，导致
16–32 MiB 音乐进入 v1 内存资产，而 FFmpeg `LocalInput` 的嵌入字节向量
仍有独立的 16 MiB 上限。导入的文件范围探测成功，发布时转入内存探测失败。
小音乐和字体各自通过，不代表它们共享的预算分类逻辑兼容。

## 修复与复用

新增包合同 `kMaximumEmbeddedMusicAssetBytes=16 MiB`，单曲超过这个上限，
或单曲加普通资产超过 32 MiB 时，选择已经验证的 `music-performance-v2`。
沿用 miniz 的 STORE 文件范围、`FileBytes` RAII 租约、同一 FFmpeg 32 KiB
自定义 I/O。没有扩大内存解码上限、普通素材总预算或单曲 256 MiB 上限，
没有增加后端或改变小包作品内容。

纯合同增加恰好 16 MiB、16 MiB+1、32 MiB 的分流检查。
Studio／Player 检查按单曲嵌入上限核对 profile。
每次 Windows Studio 交付加入 `soundtrack_contracts` 和
`large_soundtrack_studio_gpu`，连同已有六项及大音乐夹具共九项执行；no-op 也执行。

## 实测

- Windows `out/p8-music-inline-budget-regressions.log`：音乐合同、小 FLAC、
  大 WAV 的完整 PCM 对照、替换后旧文件生命周期、实际大音乐 Studio 和
  时间段工作流通过。两项 Studio 分别 7.57／7.63 秒。
- 同批归档检查曾因测试修订错误失败：依据历史文档误以为夹具为 20 MiB，
  实际当前源码生成 40 MiB。我撤回了错误的普通预算断言修改，恢复原有
  40 MiB 未绑定资产拒绝检查；生产预算没有放宽。
  `out/p8-music-archive-current-fixture.log` 原归档测试通过，6.28 秒。
- Windows 最终 `out/p8-music-budget-final-delivery.log`：九项交付检查全部
  通过，74.74 秒，两个 deploy 各同步 20 DLL 和完整资源。
- USB Android Release 原生复用同一合同／归档／Player 测试：
  `out/p8-music-budget-android-core.log` 四项通过。Player 对大 WAV 解码至 EOF，
  与原文件每个 PCM block 对照，并覆盖发布、重新安装包及旧源保留。
  Session 图输出在这项测试中使用 Null，不冒充 GLES／APK 音画证明。

Android 永久入口 `tools/test-android-music-packages.py`：独立运行目录、每次
push 后 chmod 700，保留输入和执行程序 SHA256、完整失败日志与状态。
本次目录 `out/android-music-packages/a4be788990224b0e9811e53aaa33141a/`。
大 WAV 摘要 `9288a92853dcf159b3ce6d698b55b2ce0087e1e3e671b8546adaf587a1f26418`，
小 FLAC 26,303 字节；这不是长稳、声学录音或所有多轨大素材的验收。

编排音频仍遵循其单独的内嵌资源合同；本修复明确针对单首音乐的发布分流。
历史已经产生的、不符合嵌入解码上限的 v1 包需从工程重新发布为 v2。

## Android 交付

`out/p8-music-budget-android-apk-build.log` 完成 Android 增量编译和发布前必需的
主机构建内容校验，随后 `out/p8-music-budget-install.log` 中 `install -r` 成功。
APK／已安装 APK SHA256 均为
`e918426cba6f52aaa58d79a0871765dbf15ec01beaf91f7f13f5cd0e1b36bdbb`。
构建日志末尾的 `APK unchanged` 来自同一构建的第二次幂等打包调用，
首次 POST_BUILD 已生成新包，不能据末尾一行误判无需覆盖安装。

更新后实际 UI 选择瓷光钟摆、暂停／恢复通过，标题、16 秒时长和主体画面
人工复核正确，时间 2.82→4.05 秒。证据
`out/p8-music-budget-apk-ui.log`、
`out/android-authored-works/d677cb926b434bd09fed2c4d25dc159b/`。
安装前后演出列表字节相同，摘要及保留检查记录在
`out/p8-music-budget-apk-identity.json`。本次没有卸载或清数据；大音乐的证据仍为
上面的原生完整解码检查，不将内置小包 UI 冒充大包导入 UI。
