# 候选传输服务验证（2026-09-07）

`src/cluster_transport` 将已验证的 MsQuic / OpenSSL 适配整理为项目服务，
目前仅由 `probes/security` 构建，未链接 Studio / Player，也未冻结四平台依赖。
版本、许可和私有 fork 修改沿用 `quic_candidates_2026-09-07.md` 的来源清单。

公共 API 只暴露本地连接 ID、项目 endpoint、证书指纹、有界不可变消息和事件。
网络回调不调用 UI、房间权限、图或渲染代码；所有公开操作在创建线程串行执行。
注册表与回调上下文共享连接生命周期，停止先关闭监听器，再关闭连接并等待回调退出。
发送完成或取消回调退出之前，消息和原生缓冲描述符均由适配层持有。

控制采用客户端发起的一个持久双向流，资源采用主控发起的一个持久单向流；
客户端资源上传和额外流被拒绝。两种流使用已有四字节大端长度帧。
消息上限分别是控制 4 KiB、资源 64 KiB、实时数据报 1 KiB；数据报还受协商上限限制。
应用发送预算包含未完成原生发送：控制 64 条/64 KiB、资源 8 条/256 KiB，
实时数据只保留最新待发值和一个在途值。接收队列同样限额，消费不足时通过原生
部分接收流控保留后缀，应用不额外复制无限积压。原生默认流窗口设为 256 KiB，
连接窗口 1 MiB；这些窗口不等于进程总内存预算。

每次 Poll 最多发布 128 个事件/1 MiB 消息，轮换连接起点并限制单连接突发量。
本地连接 ID 在服务生命周期内严格增长，不与认证成员 ID 混用。
建立 TLS 仅表示 Host 证书验证成功，参与者还必须通过房间认证，才可进入场景服务。

## 实测

- Windows 与 USB Android API 34 / arm64-v8a 均通过公开接口契约测试：正确/错误 pin、
  三通道内容、方向和长度限制、握手中取消、发送中停止、重连、旧 handle 拒绝、
  幂等停止、客户端/服务端连接数上限、暂停接收后恢复及顺序、错误线程调用拒绝。
- Windows 测试从部署目录启动，PATH 仅含 Windows/System32，验证完整 DLL 闭包。
- 实际 LAN 双向通过：Windows Host → Android participant，以及 Android server →
  Windows client 的反向适配验证。每方向持续发送 16 × 64 KiB 资源，同时交换可靠
  控制与实时数据报，校验每个资源字节/顺序，最终确认并断开。
- Android 反向服务端只用于验证适配能力，不承诺手机 Host 产品功能。

命令：

```text
python tools/build-security-probes.py windows --shared-tls
python tools/build-security-probes.py android --shared-tls
python tools/test-security-probes.py --serial e2b3b128 --shared-tls
python tools/test-transport-lan.py --serial e2b3b128 --pc-address 192.168.31.6
```

Windows 构建须在项目 MSVC 环境执行；工具默认增量编译、20 workers，并自动调用
Python 部署。记录为 `out/security/validation-shared.json` 与
`out/security/transport-lan.json`，包含可执行文件/动态库 SHA-256 和新建的设备目录。
目前 9 个安全/传输专项测试组与应用主构建的 37 / 29 个测试组分开计数。

未完成：连接迁移/重绑定、故障注入与长期负载、真实多手机容量、房间服务/资源准备/
定时播放集成、Android APK 权限和生命周期。此次 LAN 检查不替代这些验收。
