# GammaRay 自有基础库复用计划

> 2026-09-07：有界执行器和延迟线程回收已提取，通过 Windows/Android 测试；其余条目按下文逐项验证。
> 用户明确授权复用 GammaRayPremium 中我们自己编写的代码。

## 1. 审查来源与边界

- 仓库：`D:/GoCloud/GammaRayPremium`。
- 基础库：`src/px_deps/px_common`，不是一个假设存在的 `src/common`。
- 检查时 HEAD：`524d1b5a6f2d46911dd05109256dcf30b32ca6c2`。
- 本次依据工作目录中的 CMake、异步/网络/二维码/日志/时间接口及部分实现，
  并检查测试目录；HEAD 仅是定位信息，不代表已证明工作目录与提交完全一致。
- 原项目保持不变。正式导入时记录实际文件清单、文件哈希、来源提交和本地差异。
- 我们自己的代码按第一方复用处理，不再以“无第三方授权”为理由重复实现。
  内嵌第三方代码仍保留原来的许可与来源；自有仓库不意味着其中所有文件都由我们拥有。

## 2. 实际可复用内容

| 来源 | 决定 | 适配和验证要求 |
| --- | --- | --- |
| `async_runtime.*`、`async_delay.*`、scope drain、callback quiescence | 优先复用异步运行时和可取消生命周期 | 内部依赖 standalone Asio；接口目前暴露 executor/awaitable，隔离在实现侧；关闭期间不得悬空回调 |
| `blocking_executor.*`、`async_mailbox.h` | 复用有界后台任务和可靠消息队列 | 不把普通消息队列当作高频频谱队列；频谱使用 latest-value/有限时间窗覆盖旧帧 |
| `reconnect_backoff.*`、`reconnect_supervisor.*`、connection workflow、WebSocket reconnect adapter | 复用重连、代际校验、取消与退避 | 加入房间 epoch、抖动退避、重连限流；回到前台先恢复快照，不重放积压帧 |
| `http_client.*`、`http_base_op.*` | 审核后适配资源下载/兼容传输 | cpr 不进公共接口；修正默认 TLS 验证策略；限制重定向、响应体和下载并发 |
| `file.*`、`async_file.*`、`path_codec.*`、`zip_util.*` | 按需抽取资源 I/O | UTF-8/`std::filesystem::path`；原子落盘、哈希校验、路径穿越和解压炸弹测试 |
| `qrcode/qr_generator.*` | 复用我们自己的二维码封装 | 从 Windows target 分离；加静区、整数模块缩放、纠错与实际扫码测试 |
| `qrcode/qrcodegen.*` | 保留 MIT 上游实现 | 文件注明 Project Nayuki；保留完整版权/许可，不改称我们自研 |
| `time_util.*` | 借用本机单调时间工具 | 不能把本机 steady-clock epoch 直接与另一台机器相减；另建时钟同步服务 |
| `log.h`、`privacy_log.h` | 参考/适配日志入口 | UTF-8 跨平台路径、节流、脱敏和异步写入；不沿用全局高频 debug flush 策略 |
| `shared_preference.*` | 当前不导入 | 新项目已有配置/SQLite cache 分工，不为它额外引入 LevelDB |
| `px_udp_protocol.h`、现有视频 UDP transport | 借鉴测试/分包边界，不复用业务线协议 | 当前字段围绕视频分片、FEC、codec/显示器；不适合音频特征和 cue 的最新状态语义 |
| `md5.*`、`px_aes.*`、Reed–Solomon | 不作为新会话安全协议 | 不自行组合加密/认证协议；FEC 只有测量证明收益后才引入，MD5 不作可信内容身份 |
| Win32、DXGI、远控、剪贴板、音视频编码、旧服务登录模块 | 不随 common 聚合导入 | 不属于本次集群基础功能；需要时按独立平台边界另审 |

## 3. 已发现的具体工程问题

1. CMake 已分成 `px_common_core/async/file/net/storage/crypto/win`，是很好的起点；
   但总目标 `px_common` 仍聚合全部。新项目不能直接链接该总目标。
2. 配置入口无条件查找 cpr/fmt/spdlog/miniz，组件又通过 PUBLIC 传递依赖。
   需要独立、按需配置的 targets，避免只用时钟或二维码也要求整套网络/存储依赖。
3. 当前 C++23，`async_runtime.h` 要求 Asio >= 1.38.2；必须验证新项目四平台工具链，
   不能仅因 `#ifdef` 存在就声称 Android/iOS/macOS 已支持。
4. `HttpDownloadOptions::verify_ssl` 与 `HttpClient::verify_ssl_` 默认为 false。
   新集成必须 fail closed：正常 CA 校验或显式的二维码公钥绑定验证，
   绝不能以关闭验证解决本地证书问题。
5. 二维码当前被编入 `px_common_win`；封装生成 RGBA 并直接缩放，需补可扫描静区与
   模块边界测试；生成库不包含手机摄像头扫码识别能力。
6. `async_mailbox.h` 已有容量限制、队列统计和关闭语义，但每次 push 构造共享信封；
   高频发送应按组只序列化一次，再用有界槽位发布，实测分配/锁竞争后优化。
7. 本次尚未确认可直接复用的四平台 QUIC/安全实时数据报、跨设备时钟同步或房间协议。
   这些列为新增职责，不冒称已有能力。

## 4. 新仓库集成形态

建议目标（名称在实现前冻结，不导出上游业务类型）：

- `foundation/core`：少量值类型、路径、单调时间、诊断契约。
- `foundation/async`：调度、取消、drain、有界阻塞任务；Asio 作为私有实现。
- `foundation/io`：文件和受限下载，不依赖图、渲染或 UI。
- `network/transport`：项目消息/连接 ID、能力协商，私有 QUIC/WSS 实现。
- `cluster/session`：房间、权限、成员与代际；不管理 socket 内部状态。
- `cluster/time`：时钟映射、漂移、抖动缓冲和同步统计。
- `cluster/distribution`：资源准备、profile 检查和定时切换。
- `cluster/replication`：频谱帧、cue、角色、可选状态快照的复制策略。
- `platform/scan`：手机相机扫码与权限；不和二维码生成混在一起。

不建立一个新的万能 Common/NetworkManager。公开头按职责拆分，第三方头私有，
协议生成文件只进入协议 codec target。运行时仅消费值快照，不消费网络客户端对象。
独立渲染库仍能只用自身目录构建；网络不进入渲染核心依赖。

媒体后端已经确认为 FFmpeg，不迁移 GammaRay 或其他参考中的高级播放器/解码器。
HTTP 资源获取可复用 cpr wrapper，WebSocket 则是 Asio2 adapter，不混淆两者职责。
整个产品的已确认和待定依赖见 [技术栈评估](technology_stack_evaluation.md)。

新维护代码遵守 AGENTS：Google 命名、4 空格、成员声明处初始化、组合设计。
唯一所有权用 `unique_ptr`；异步确实共享的生命周期才用 `shared_ptr/weak_ptr`。
不为已有值/引用强行分配堆对象。外部 C API 指针仅在 RAII adapter 内。
异步结果回到 UI/render 线程消费，不跨线程直接修改图或 GPU 状态。

## 5. 导入顺序与验收

1. 逐文件确认第一方/第三方身份、修改情况、依赖、源许可与目标分发许可。
2. 先提取最小 async/core 和其测试；不用运行 GammaRay 全项目构建或复制整个 deps。
3. 移植 scope shutdown、callback quiescence、mailbox、reconnect、取消测试；
   当前源码有这些测试，但本次没有执行，不代表它们已在新项目通过。
4. 修正 HTTP 安全默认值，添加错误证书、错误 pin、恶意重定向、取消/截断下载测试。
5. 分离 QR 生成，验证短/长邀请、整数缩放、不同 DPI、低亮度与失效邀请。
6. 做 QUIC DATAGRAM 跨平台依赖/构建 spike，再冻结具体传输库；Windows 优先实测，
   移动端真机阶段遵循主路线的 Windows 验收门。
7. 每个模块增量构建与测试、记录来源；不 bulk restyle 上游三方、不改原 GammaRay。

下一步设计见 [集群播放方案](cluster_playback_plan.md)。

## 6. 首次提取证据（2026-09-07）

`foundation_async` 提取 `blocking_executor.*` 和 `async_runtime.cpp` 中的线程回收职责，
来源为实际提取时的提交 `83b59ed96a2483ab9cad370e2925238dc2d52046`，不是前期审查提交。
所选文件工作树干净，逐文件 SHA-256、第一方授权、修改清单见
`provenance/gammaray_common.json`。原 GammaRay 仓库未修改。

执行器使用标准库，未引入 Asio/网络。总计最多 8 个存活/回收中的线程池，每池最多
16 线程、65,536 个等待任务；闭包持有的数据另由调用者预算。取消任务在锁外析构，
工作线程释放最后一个执行器所有者时交给有界回收服务 join，不 detach 或自 join。
工程保存/发布/模板载入和素材导入已接入单工作线程、单等待任务配置，退出时完成保存，
导入通过 stop token 取消并清理。公共接口不导出原仓库类型或外部库类型。

Windows 28 项和 Android USB 原生 20 项测试通过。覆盖饱和拒绝、排空、取消、并发提交、
抛异常任务、重入析构、工作线程内释放、导入失败后恢复和退出资源清理。
Asio、HTTP/TLS、QUIC、二维码扫码和实际房间服务仍按各自验收门推进。
