# Godot 点光阴影 GPU 成本验证

日期：2026-09-17。范围是 W1.1 点光 cube 阴影的短时隔离 GPU 计时。目标不是建立跨机器
性能门槛，而是确认六面阴影的实际 GPU 工作量、区分深度 pass 与宿主调度，并为后续
质量/成本选择留下可复现基线。

## 计时边界

bgfx 后端现在为场景深度、场景颜色、纹理 pass、呈现和回读设置稳定 view 名称；已有
GPU 粒子 view 继续使用各自名称。名称只存在于窄后端诊断层，没有进入渲染公共 API。
这也避免每帧复用 view ID 时，普通纹理 pass 继承上一帧 `Scene depth` 名称。

`shadow_work_gpu` 仅在点光和关闭阴影两组测量期间启用 bgfx profiler。每组运行 150 帧，
前 30 帧预热，随后记录 120 帧全局 GPU 时间及所有 `Scene depth` view 时间。两组使用
同一 Sonic Enamel 编译图、资产、640×360 输出和静音输入；唯一差异是 `scene.shadow`
是否启用。测试永久要求开启阴影时比关闭时恰好多 6 个深度 view。

## 结果

本机 D3D11 结果：

- 开启点光阴影：7 个场景深度 view，深度合计 p50/p95 为 `3.14015/7.26730 ms`；
- 关闭阴影：1 个作品自身深度 view，p50/p95 为 `0.632581/3.18990 ms`；
- 六个点光阴影 view 的两组分布差为 `2.50757/4.07740 ms`；
- 整帧 GPU p50/p95 从 `7.12022/10.7050 ms` 增至 `9.97671/14.0262 ms`；
- 整帧 host p50/p95 从 `6.0166/10.9835 ms` 增至 `10.1606/14.3810 ms`；
- 阴影像素差仍为 `0.308209`，纹理增量仍为 `50,331,648` 字节，等于 48 MiB。

这里的 GPU 深度成本是两个独立 120 帧分布的 p50/p95 差，不是逐帧配对差。它包含六次
caster 深度渲染，不包含接收 shader 的跨面 PCF 成本；整帧 GPU 差包含后者及其他调度
影响。单机短测不能替代 Android、其他 GPU、热状态或最终长稳验收。

作品串行日志为
`out/point-shadow-isolated-final.log.runs/1789574820986421200.log`，机器结果在
`out/windows-pcf13/shadow-work-gpu/sonic-enamel/results.json`。随后完整 D3D11 探针通过
全部既有像素回归，日志为
`out/point-shadow-isolated-full-gpu.log.runs/1789574892530265400.log`。
