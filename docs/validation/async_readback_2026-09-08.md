# 有界 GPU 异步读回（2026-09-08）

导出前置模块已接通 RhythmRender 的值类型契约：`SupportsReadback`、
`RequestReadback` 和只移动的 `Readback`。`Poll` 不等待，正常 `EndFrame` 推进完成。
返回自有的左上原点、预乘 RGBA8 图像，可转交编码线程。节点 Viewer 继续使用 GPU 纹理，
没有接入 CPU 读回。

仅支持 RGBA8 渲染目标；浮点目标须先经过显式 SDR 输出 pass。最多三帧同时挂起，
每帧最多 1080p 像素数，staging 计入既有 256 MiB/1024 纹理预算，不额外放宽预算。
当前完成后释放 staging；尚未声称零分配、零拷贝或硬件编码互操作。

复用本项目 `windows_spike/tests/gpu_probe.cpp` 已验证的 bgfx blit/frame/read 顺序，
未复制新第三方代码。native handle/pixel pointer 均在 private renderer adapter 内。
取消只丢弃交付；正在写入的 CPU 缓冲保留到 GPU completion，或保留到 bgfx shutdown
完成后才销毁。设备失效禁止交付旧图像；ticket 保留 backend 生命周期。

Windows `out/readback-final-tests.log`：公共 Null 契约、实际 D3D11 读回、源码边界、
Studio 音乐启动与部署检查共 5 项通过。GPU 测试覆盖同一目标连续三次内容变更的
帧身份、顶部/底部颜色、透明预乘、队列上限、资源计数、move 后旧票据拒绝、
取消前/后提交、设备失效及有挂起读回时销毁并重建设备。

USB `e2b3b128` 的 Adreno 650/GLES 实际支持该能力，同一组像素/队列/取消测试通过，
见 `out/readback-android-tests.log`。这是原生渲染测试，不替代 Android 应用生命周期验收。

Windows Studio/Player 已按 Python 构建规则更新各自同级 `deploy`，各含 20 DLL。
导出任务、Studio 界面、音视频流的离线确定性与文件原子发布仍需接通。
