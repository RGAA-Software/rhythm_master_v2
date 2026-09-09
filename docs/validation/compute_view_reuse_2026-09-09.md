# GPU 计算视图复用导致 Android 黑画面

P2 事件像素验收发现，GLES 先执行拖尾/反馈绘图，再复用 pass 编号执行 GPU 粒子
计算与绘制，同帧输出读回全黑；随后直接绘制同一 GPU 点缓冲可见。Windows 同用例
可见，说明不能用 Windows 或句柄有效代替 Android 实际画面。

根因在项目 bgfx 适配器的 `BgfxGpuPoints::Update`：每帧从 0 重新分配视图编号，
计算视图只设置名称/顺序，保留了此前绘图使用该编号的 framebuffer/clear 状态。
现在提交计算前 `bgfx::resetView(view)`，再设置所需的名称和顺序。不修改上游 bgfx。

隔离证据：只 reset 读回视图仍失败（`out/p2-event-gpu-readback-only-android-tests.log`）；
只 reset 计算视图通过（`out/p2-event-gpu-compute-view-android-tests.log`）。诊断阶段
下一帧直接绘制峰值 255、原同帧为 0，见 `out/p2-event-gpu-diagnosis-android-tests.log`。
没有把这个后续绘图当作原帧成功，也没有采用延后一帧读回的规避方案。

永久检查 `event_gpu_tests` 与 Android `android_gpu_contract_tests --events` 共用
`src/runtime/tests/event_gpu_contracts.cpp`，先运行定时 ADSR、隔离的拖尾/反馈 reset，
再执行初始填充为 0 的 GPU 粒子重置。最后两帧峰值分别为 255 和 0，要求旧状态
存在且被事件清除。空渲染器另检查物理重置恢复输入位置并改变点身份代次。

最终 Windows event_gpu 和既有 GPU 综合探针通过：`out/p2-compute-view-windows-tests.log`。
USB Redmi K40S event GPU、既有 --execution-probe 和物理/粒子原生重置通过：
`out/p2-compute-view-android-tests.log`。综合回归含计算/实例、1万至262144粒子、
颜色/深度/材质/阴影/环境/变形/骨骼/morph。长稳没有在此执行。

两个更早的测试夹具错误也保留：读回浮点拖尾需先确定 SDR 输出，首次报
`render.readback_precision`；GPU 默认 initial_fill=1，重置会回到有粒子的初始分布，
不能断言空场。清空用例显式 initial_fill=0 并加大点尺寸/不透明度，未改变产品默认值。
分别见 `out/p2-event-gpu-android-tests.log`、`out/p2-event-gpu-sdr-android-tests.log`。

此前 GPU 粒子检查重点覆盖直接更新后独立绘制及资源/数量，缺少在复杂图后复用
视图编号的同帧 Runtime 输出。以后更改计算/绘制/视图生命周期必须运行此检查，
且不能把 readback 执行成功、纹理有效或绘制调用数当作当前帧正确像素的证据。
