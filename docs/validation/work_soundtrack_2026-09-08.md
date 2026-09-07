# 作品音乐保存、发布和跨平台播放（2026-09-08）

作品现在可以绑定一首音乐及其音量、循环设置。音乐复制到内容寻址资产库，随工程保存，
随运行包发布；Windows Player 和 Android Player 的应用入口自动选择包内音乐。
Studio 与 Android 原生的真实音乐/GPU 验证通过，APK 应用验收仍单独待办。

## 用户流程

1. 在“音频输入”播放音乐并设置音量/循环。
2. 在“作品音乐”点击“保存当前音乐及设置”。后台复制、哈希和 FFmpeg 验证完成后，
   绑定进入正常撤销历史。期间变更工程、音乐选择或设置会拒绝过时结果。
3. 工具栏“保存”保留工程，“发布运行包”将音乐和画面交付为一个 `.rhythmpack`。
4. 重新打开工程会恢复作品音乐；“播放作品音乐”可从临时试听切回。
   “移除作品音乐”解除绑定，撤销可恢复，源文件不会被修改。
5. Player 打开带音乐的运行包会自动加载同一内容及设置。Windows 的显式启动参数
   `--audio` 可覆盖启动包的音乐；后续切换作品以新作品的音乐为准。

当前规格是一首从作品零时刻开始的音乐；重复播放沿用已有尾部排空后重启的语义，
不承诺无缝循环。音乐和其他包内资产共用既有 **8 MiB** 限额，归档总限额仍是 16 MiB。
较长音乐可使用 MP3/AAC；超限不自动增加预算或丢弃素材。
大媒体流式容器、多轨编排、裁剪/偏移、混音和区间预滚仍需继续实现。
离线导出的时长/音乐输入仍遵循其独立设置，本次没有把播放器循环变成导出循环。

## 格式与所有权

- `media::Soundtrack` 仅包含资产 ID、显示标题、增益和循环。路径、设备和 FFmpeg 类型
  不进入工程/运行包公共契约。工程 manifest 在有绑定时使用版本 2；无绑定仍写版本 1。
- 带音乐的运行包使用 `music-performance-v1`，保留 program ABI 2 和既有资源限额。
  旧包继续读取；把音乐字段塞进旧规格或缺少必要音乐字段会拒绝，避免静默丢失。
  暂停的集群调度规格明确拒绝新音乐规格，没有增加通信功能或协议。
- `MusicAuthoring` 在一个有界 worker 上复用 AssetStore 和 prepared-assets 的音频验证。
  完成结果是值快照，UI 检查版本和当前输入后应用。取消在结果应用前仍有效。
  失败留下的内容寻址 blob 不进入新工程记录；历史修订/撤销引用的 blob 不被主动删除。
- 同一 FFmpeg `AudioDecoder` 现在支持已有 LocalInput 的不可变内存源。
  `PreparedPackage` 在后台验证音频首块并发布共享压缩字节；Session 和解码 worker
  共享其生命周期。没有整首 PCM 常驻、临时解压文件或第二个播放器后端。
- 压缩音乐仍受 8 MiB 资产上限约束。解析包和构建共享字节期间存在有界副本，
  不声称零拷贝或流式大文件；读取内存源的底层通用接口上限仍为 16 MiB。
- FilePlayback 的来源代次确认由 worker 在释放旧解码源后发布，与 UI 的 Stop/Load
  意图代次分开。Android 从本地文件切到包内音乐或清空作品时，缓存文件只在确认后回收。

复用本项目已验证的 LocalInput、AudioDecoder、FilePlayback、AssetStore、图/归档和
prepared-assets 实现；没有导入新第三方源码、修改旧仓库或重新构建 FFmpeg。
仍采用记录在媒体专项中的 vcpkg LGPL 库及原有分发材料。

## 验证记录

`out/soundtrack-integration-build.log` 完成 Windows Release 全量目标的增量构建，
没有新增项目编译警告。`out/soundtrack-integration-tests.log` 运行 94 项非通信回归：
93 项通过，源码扫描将测试断言里的英文词组误识别为分配表达式。改写该断言文字后，
`out/soundtrack-final-tests.log` 的源码边界、音频 fixture 和音乐 Player 均通过。
未恢复已暂停的通信测试。新版 Studio/Player 均由 Python 自动部署及解析 20 个 DLL。

最终界面复查截图为
`out/windows-release/soundtrack-studio/468917862880800/soundtrack-studio.tga`；
同时修正了重新打开作品后音乐面板仍显示旧“未绑定”提示的问题。

- `out/embedded-audio-tests.log`、`out/embedded-audio-android-tests.log`：文件和内存
  解码逐样本相同，精确 seek、取消后保留旧会话、共享源生命周期通过。
- `out/soundtrack-host-tests.log`：实际 Windows 音频设备的包内 FLAC 播放、特征、
  暂停/跳转/循环/停止、错误恢复和源释放；Android 音乐宿主适配器的文件→包内音乐
  替换、前后台/焦点意图和缓存清理在 Windows 宿主测试中通过。
- `out/soundtrack-contracts-tests.log`：工程事务、撤销/重做、音乐元数据往返、旧包兼容、
  篡改/错误字段/缺失引用拒绝通过。
- `out/soundtrack-player-tests.log`、`out/soundtrack-android-tests.log`：保存→发布→Player
  的音乐逐样本一致，坏音乐包保留当前作品，共享源能跨作品切换完成读取。
- `out/soundtrack-ui-tests.log`：真实 ImGui 中英文按钮操作、设置更新、清除/撤销、
  保存/发布与过时结果拒绝通过。
- `out/soundtrack-studio-tests.log`：实际 Studio/D3D11 打开 Resonance Live，按钮完成
  绑定→保存→发布→清除→重新打开，音频驱动恢复，根图保持 9 节点 / 197 可达指令。
  真实发布包为 `out/windows-release/soundtrack-studio/468537579291400/Published/音乐作品.rhythmpack`，
  1,785,589 字节。它直接用于下面的 Android 像素验收。测试增益为零以静默运行，
  音乐源 PCM 和特征并未置零。
- `out/soundtrack-player-gpu.log`：部署 Windows Player 直接加载包内 FLAC，30 个 GPU
  帧和非零音乐特征通过。修正了旧 smoke 对末帧绘制次数的假设：静态纹理复用合法，
  应检查运行期间实际产生过图画面。

### USB Android / Adreno 650

`out/soundtrack-gles-tests.log` 使用上述 Studio 按钮发布的包，在 USB 手机 GLES 上
分别运行 240 帧真实解码 PCM和 240 帧静音，统一 60 Hz 采样时刻，输出 640×360。
音乐/静音分别重新建立会话；图和时间相同，输入音乐不同。

| 场景 | p50 | p95 | 稳定纹理字节 | 最大 RMS |
| --- | ---: | ---: | ---: | ---: |
| 包内真实音乐 | 17.366 ms | 19.2538 ms | 26,742,788 | 0.22358 |
| 静音 | 17.1366 ms | 18.8633 ms | 26,742,788 | 0 |

320×180 读回图像的平均 RGB 绝对差为 9.93483（0–255），音乐画面平均亮度 17.71。
原图在 `out/android-soundtrack-music.ppm` 和 `out/android-soundtrack-silence.ppm`。
设备目录 `/data/local/tmp/rhythm-music-gles-9e0d210e1bd144fba715bd091a4ee9fc`。
CPU 分析与 GPU 完成同步计入计时；不代表屏幕刷新、应用音频延迟或热稳定性。
该结果尚未达到稳定 60 fps。APK 安装仍受设备 USB 安装限制，应用音频、生命周期、
中断与长时间验收不能由这些原生探针替代。
