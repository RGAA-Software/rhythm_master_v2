# QUIC 候选实验记录（2026-09-07）

本记录只覆盖独立 `probes/quic` 可执行程序。Studio/Player 尚未链接 QUIC，
不代表加入房间、邀请认证、资源准备、手机扫码或场地容量已经完成。

## 固定输入和平台

- MsQuic v2.6.1：`a01333cf7c2659cce0ff03ef3f21e1ff15bb5b83`，MIT。
- quiche 0.29.3：`55886df3be579579207104c8e645825b6347a209`，BSD-2-Clause；
  Rust 1.95.0、FFI feature、项目留存 Cargo.lock。BoringSSL 来自 boring-sys 4.22.0。
- Windows 10 Pro 19045，MSVC 19.51，Ninja/CMake 3.28；Android NDK 29、arm64、
  编译 API 26，USB 真机 22021211RC / Android API 34。编译 API 26 不等于 API 26 真机验收。
- LAN：PC 有线 `192.168.31.6`，手机 Wi-Fi 初始 `192.168.31.168`、正式 TLS 复测时为 `192.168.31.49`。
  不修改防火墙、根证书库、系统 TLS 设置或手机安装限制。

## 已执行实验

| 候选/路径 | Windows | Android 真机 | 跨设备 LAN |
| --- | --- | --- | --- |
| MsQuic / Schannel | TLS 凭据失败 `SEC_E_ALGORITHM_MISMATCH` | 不适用 | 未执行 |
| MsQuic / 上游 OpenSSL 3.5.8-dev 快照 | TLS 绑定、可靠流、DATAGRAM、关闭重建通过 | 同项通过 | 两端分别作为 server/client，双向通过 |
| quiche / BoringSSL | C API 内存传输握手、流、DATAGRAM、错误 CA 拒绝通过 | 同项通过 | 尚未实现该候选的 socket 驱动测试 |
| MsQuic / 独立 OpenSSL 3.5.8 正式版 | 握手、错误绑定拒绝、流、DATAGRAM、关闭重建通过 | 同项通过 | 两端分别作为 server/client，双向通过 |

Schannel 路径的失败来自当前 Windows 10 TLS 能力；不据此把整个 MsQuic 判断为不可用。
MsQuic OpenSSL 实验启用证书回调，逐字节校验预期 DER；错误绑定在 Connected 前拒绝。
quiche 使用显式测试 CA 和 peer verification，错误 CA 在 established 前拒绝。
没有使用关闭证书验证的回退。测试证书和私钥为公开、短期、可重建的实验夹具，
不能用于产品身份或长期密钥。

quiche 内存传输测试关闭 pacing，仅用于确定性包交换；不视为真实网络性能证据。
DATAGRAM 队列配置为两条，第三条拒绝，消费后验证字节；这不等于项目级发送背压已完成。

正式 TLS 的双向日志为 `out/quic-release-lan.log`，手机证据目录
`/data/local/tmp/rhythm-quic-lan-20260906212648`。两平台分别执行 8 次握手发起后立即取消、
8 次流/DATAGRAM 发送后立即取消，均收到关闭完成并在释放 callback state 前关闭句柄。
允许取消与正常完成竞争，不假定每次取消都先于握手/发送完成。
中途手机 DHCP 地址变化导致反向
连接旧地址并握手超时，证书回调未触发；读取实际 wlan0 地址后恢复双向通过。
脚本现每次读取并校验地址，保留超时/证书/能力状态诊断，不将网络地址变化误判为 TLS 错误。

后续将项目 `SendQueue` 和 `StreamDecoder` 接入同一真实探针，两平台及双向 LAN
再次通过，记录为 `out/quic-buffer-lan.log` 和手机目录
`/data/local/tmp/rhythm-quic-lan-20260906213421`。发送回调只写有限槽位的完成标志，
宿主线程回收 ticket 与不可变载荷，不在回调线程修改宿主队列。
取消实验发现 shutdown notification 早于部分最终 send callback，故验证改在原生
StreamClose/ConnectionClose 停止回调后检查 in-flight 为零；不提前释放借用缓冲。
当前探针收到背压即失败退出；生产适配器的暂停/恢复接收策略仍待实现。

OpenSSL 3.5.8 的未修改上游 Windows 源码在 MSVC 19.51 下有数值转换告警及
tsan_add 宏未选择分支的指针类型告警；后者经源码检查是 sizeof 条件选择两种
Interlocked 宽度时对另一分支的类型检查。项目自有适配代码仍以 /WX 检查。
此项来源限制已记录，不以实验通过宣称上游全部 warning-free。

## 构建和来源记录

`tools/build-quic-probe.py` 使用隔离输出及增量缓存，默认 20 workers。
OpenSSL 原生 Windows nmake 无并行选项，单独记录此上游构建限制。
部署由 `tools/deploy-quic-probe.py` 复制完整依赖闭包、夹具、许可和 SHA256 清单到
实验 EXE 同级 `deploy`，主应用部署内容不因此增加 QUIC。

MsQuic 的本地补丁记录在 `third_party/patches/msquic-probe.patch`：
MSVC SAL 临时变量范围修正、中文 include 前缀的 Ninja 依赖修正、Android API 26
runtime-only 构建排除不使用的测试自签名 helper。没有修改旧项目或 GammaRay 源码。

quiche 的运行依赖按实际 package/target/ffi feature 的 normal tree 记录，
Windows 20 项、Android 17 项；包含 BoringSSL 原始 notice 和 Rust 标准库 attribution。
完整 Cargo.lock、两平台许可 ledger 位于 `third_party/probe_locks`。
`third_party/quic_probe_sources.json` 保留来源、提交、文件哈希与修改情况。

上游 MsQuic 子模块报告 `3.5.8-dev`，因此新增独立 TLS 发布版源码目录，保留快照。
正式版 3.5.8 的提交固定为 `f4dc4d58b48d346a8270183f89acf826d459b0ca`；
它列在 [OpenSSL 官方下载页](https://openssl-library.org/source/) 的 3.5 LTS 发布列表。
`tools/prepare-quic-tls.py` 验证 tag 对应提交；构建配置指纹包含提交和选项，
不同 TLS 版本使用独立输出目录，不复用旧版本库假装完成替换。

## 后续门槛

### 真实 loopback 连接的分级短测

`tools/test-quic-stress.py` 使用同一个进程中的客户端和服务器、一个有限 listener，
并发保持指定连接数后逐连接发送一条可靠消息和一条 DATAGRAM，再同时发起关闭。
第 N+1 个连接在 listener 上限处拒绝。发送回收后 in-flight 为零；项目没有为每个
peer 创建线程，I/O 使用 MsQuic 调度。底层 API 生命周期与单 peer 缓冲适配已拆成
私有 `native_api.h` / `connection.h`，多连接驱动位于 `stress.cpp`，不导出给 runtime。

| 平台 / 并发连接 | 建立全部连接 ms | 总阶段 ms | 进程 CPU 增量 ms | 进程峰值驻留字节 |
| --- | ---: | ---: | ---: | ---: |
| Windows / 10 | 43.478 | 131.848 | 93.750 | 22,347,776 |
| Windows / 100 | 118.917 | 275.042 | 234.375 | 61,476,864 |
| Windows / 1,000 | 490.794 | 1,157.640 | 3,328.120 | 380,604,416 |
| Android / 10 | 61.647 | 89.953 | 113.667 | 11,628,544 |
| Android / 100 | 177.621 | 328.775 | 553.995 | 21,471,232 |

Windows 为 Ryzen 9 5900X、12 核/24 逻辑线程、约 32 GiB RAM、Windows 10 19045。
Android 为 SM8250 / 22021211RC / API 34。计时从连接创建前开始，包含 native close，
但不含之前的注册/TLS 配置装载；CPU 为所有线程合计，可大于墙钟时间。
Windows 驻留指标来自 PeakWorkingSetSize，Android 来自 getrusage ru_maxrss，
统计口径不同，不能据此得出手机比桌面省内存的结论。
原始值和实际 EXE/SO 哈希在 `out/quic/stress-windows.json`、`stress-android.json`，
手机目录 `/data/local/tmp/rhythm-quic-stress-20260906221821`。

这是单次短突发，不是稳定吞吐/网络故障/真实房间协议压测；没有 GPU、音频和包下载
并行负载，也没有连接 1000 台手机。Android 的 100 条同进程 loopback 连接同样不能
解释为手机主控或现场 AP 容量。C3 仍需协议与故障注入，C5 仍需现场多设备长期测量。

正式 TLS 发布版已完成两端编译、双向 LAN 和基本取消竞态验证；接着完成项目级有界发送/接收、取消/退出、错误包、故障注入、
连接迁移和协议模拟 peer 负载。明确 CPU/内存/包体积测量条件之后，才冻结采用范围。
Apple 平台按用户安排最后移植，当前矩阵仍为未测。
补充接收流控证据：Windows 与 Android 各经过 1000 条 4096 字节消息的真实
loopback 传输，50 次暂停/恢复，应用队列峰值 65536 字节，消息顺序和 FIN 正确。
使用原生部分接收计数停止回调，MsQuic 保留未消费后缀；不保存借用指针，也不创建
另一条无界复制队列。暂停时取消可完成两端关闭与发送回收。
`tools/test-quic-lan.py` 已纳入这些检查，双向 LAN 回归通过；本次设备目录
`/data/local/tmp/rhythm-quic-lan-20260906230335`，日志 `out/quic-flow-lan.log`。
这是控制流验证，尚不覆盖资源流并发、网络故障和长时间慢客户端。
目前不宣布通用四平台支持，也不将一台手机互通推算成 10/100/1,000 台现场容量。
