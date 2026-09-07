# 房间身份与密钥存储验证

本模块仍由 `probes/security` 独立构建，未接入 Studio/Player；不能视为房间加入闭环。
使用与 QUIC 正式版候选相同的 OpenSSL 3.5.8，来源、提交和完整文件哈希见
`third_party/quic_probe_sources.json`。没有新增未记录的密码库，也未选定项目出口许可。

`src/security` 的公共契约只含值、span、路径和独占身份对象。OpenSSL、DPAPI、
私钥 PKCS12 和原生句柄均留在内部适配器。身份使用 P-256 / SHA-256 自签名服务端
证书，随机序列号，当前有效期 30 天；轮换必须同步更新邀请指纹，此策略尚待产品集成。
令牌由 RAND_priv_bytes 生成 256 位随机值，以 CRYPTO_memcmp 比较。

客户端校验邀请中的完整证书 SHA-256，同时检查有效期与服务端用途；TLS 握手负责
证明私钥持有。pin 模式不依赖系统 CA/name 信任，也未关闭校验直接接受证书。
正确 pin 仍不能接受过期、尚未生效或仅供客户端认证的证书。

Windows 密钥文件采用当前用户 DPAPI（禁止弹窗、不使用机器共享 scope），外层固定
版本头，输入大小上限 128 KiB + 8 字节。只写密文；空密码 PKCS12 仅存在于私有的
可擦除内存中。Native DPAPI 输出即刻由 RAII 接管，复制失败也会擦除后 LocalFree。
文件写入使用有界随机独占暂存目录、WriteGuard 和 durable/atomic replace；加载失败
不会自动覆盖或重生成身份。调用方须提供已有的私有宿主目录。

已执行：

- Windows：独立身份、正确/错误 pin、DER 大小与尾随数据、随机令牌与常量时间比较接口。
- Windows：DPAPI 保存/恢复、密文随机性、篡改/版本/截断/超限拒绝、写锁冲突、
  原子替换和暂存清理。未执行另一个 Windows 用户账户的解密尝试。
- Windows：保存后重新加载的身份经 MsQuic 完成真实 TLS、可靠 stream、DATAGRAM，
  错误 pin 在 connected 之前拒绝，握手中/发送后取消回收通过。
- USB Android API 34 / arm64：相同密码与证书策略测试、生成身份的真实 QUIC 上述流程。
  Android 是参与者目标；宿主私钥存储明确 unsupported，没有明文降级实现。
- 两端真实加密双向 stream：Join/Welcome、续期、错误/撤销邀请拒绝、重放关闭与
  连接身份解除；详见 `docs/cluster_invitation_protocol.md`。没有 App/相机界面验收。

执行脚本：`python tools/test-security-probes.py --serial e2b3b128`。
结果及二进制哈希写入 `out/security/validation.json`；Windows 完整测试部署在
`out/security/windows/deploy`，每次构建调用 Python 部署并串行发布共享 DLL 与清单。

初版静态基线中身份与 MsQuic 各含同版本 libcrypto；随后独立构建共享 provider，
Windows 与 Android 的八项安全专项均已通过，共享方案下两模块实际链接同一 libcrypto。
原静态缓存保留。构建/测试命令增加 `--shared-tls`，结果在 `validation-shared.json`。
Android ELF NEEDED 与 16 KiB LOAD alignment 已检查，`shared-android-linkage.json` 记录
依赖、尺寸和哈希；这不是在 16 KiB 页设备上的运行证据。

Windows 首次共享启动暴露上游 `/DEPENDENTLOADFLAG:0x800` 仅搜索 System32，导致
虽然 libssl 已部署仍以 0xc0000135 退出。私有 fork 新增显式共享 TLS 选项，改为
0xA00（应用目录 + System32），保留其他 profile 的上游默认值，不改变机器搜索设置。
依据 [MSVC DEPENDENTLOADFLAG](https://learn.microsoft.com/en-us/cpp/build/reference/dependentloadflag)
和 [LoadLibraryEx 搜索标志](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw)。
补丁、来源与修改清单仍记录在 `third_party/quic_probe_sources.json`。

正式接入前仍须完成共享 provider 的 LAN/故障与部署回归、资源权限、连接限额、
撤销/重连/流控和协议故障注入。没有用这一实验冻结四平台依赖；Apple 最后验证。
