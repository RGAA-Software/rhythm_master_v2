# Godot 双级联方向光阴影验证

日期：2026-09-16。范围是 W1.1 的第三批增量：在旧作品默认单张方向光阴影不变的前提下，
增加可选双级联方向光阴影。这里不实现四级联、级联混合或点光全向阴影。

## 固定来源与适配

继续使用 Godot 4.5.1-stable 提交
`f62fdbde15035c5576dad93e586201f4d41ef0cb`，MIT 许可及通知保持不变。级联相机主要参考
`servers/rendering/renderer_scene_cull.cpp` 的
`RendererSceneCull::_light_instance_setup_directional_shadow`：

- 以接收相机 near、作者上限和归一化 split 计算两个视深区间；
- 为每段生成八个视锥端点，计算中心与包围球，并按阴影图尺寸扩展一像素边界；
- 使用 `radius * 4 / texture_size` 在灯光横向/纵向稳定吸附；
- 接收 shader 按相机 view depth 选择近级或远级。

项目适配没有引入 Godot RID、场景剔除器或全局 atlas。每一级联由 Runtime 的 RAII
`ShadowPass` 持有一对独立的颜色/深度附件；两级使用相同分辨率、PCF 档位和偏移。
`shadow_distance` 作为视锥包围球之外的投影深度余量，`shadow_max_distance` 限制参与
级联的接收相机最远距离。当前没有 Godot 的四级联、split blending、按级联 caster
octree 裁剪或自动 pancake 深度收紧，这是明确保留的兼容缺口。

## 合同与兼容

- `shadow_cascades` 默认 `Single`，所以缺少新属性的旧图仍只分配一对附件、执行一次
  caster pass，并保持原显式 center/extent 投影。
- `Two Cascades` 只接受方向光；聚光仍走原单透视图，点光仍明确拒绝。
- 图预算把双级联计为两次 caster index 工作；1024 档从单图 8 MiB 增加到 16 MiB。
- 第二张深度图必须与第一张不同、分辨率相同，不能与接收深度附件重合；两张图在一次
  资源替换成功后才共同发布，关闭、重置和预览释放遵循原 RAII 路径。
- surface shader artifact 白名单固定新增 sampler 7、第二矩阵和级联设置向量；普通材质、
  skin、morph 与自定义 surface program 仍共用同一场景绑定合同。

## 验证结果

CPU 合同覆盖：旧节点默认单级、非法级联枚举、分割距离、近远视锥尺寸、近级联子网格
相机运动稳定，以及双级联只增加一对附件并在 reset 后完整释放。`scene_graph` 与
`shadow_runtime` 均通过。

Windows D3D11 完整 GPU probe 进一步使用两张内容不同的深度图，确认相机 view depth
在 split 两侧分别选择近级与远级；原 Nearest/PCF5/PCF13、聚光、材质纹理、环境、
实例、skin、morph 和形变检查同时通过。最终串行租约日志为
`out/windows-pcf13/cascade-shadow-final-gpu.log.runs/1789550579231518800.log`。

同一受影响 shader ABI 还使用已跟踪 `tools/shaderc.exe` 重编 Windows SM5 与 Android
GLES 300 的四种 surface 表达式及六种 vertex 组合；重复产物一致、非法表达式拒绝、
artifact 反射白名单和 surface wrapper roundtrip 均通过。Windows 随后实际创建 24 组
scene program，七项 PBR 绑定、实例、48 骨骼 skin 和四目标 morph 像素均通过；串行日志为
`out/windows-pcf13/cascade-material-profile-gpu.log.runs/1789550539689362800.log`。

真实作者路径使用当前 Chromatic Loom 编译图，640×360、静音运行双级联 PCF13 共
121 帧。双级联相对关闭阴影的检查点最大平均 RGB 差为 `0.362297`，相对原单图 PCF13
为 `0.359650`；末帧构图与细线格架保持一致，没有错误投影覆盖。稳定纹理占用从
`18,938,052` 增至 `27,326,660` 字节，差值恰好是一个 1024² 颜色/深度对的 8 MiB。
双级联 host-frame p50/p95 为 `2.1844/3.8835 ms`。同次短测的单图 PCF13 为
`2.4105/4.1366 ms`，次序受提交、显示和调度噪声反转，因此这里只记录可运行短测，
不宣称隔离 GPU 成本或级联更快。作品最终串行检查也记录在
`out/windows-pcf13/cascade-shadow-final-gpu.log.runs/1789550579231518800.log`，机器可读
结果和对照帧位于 `out/windows-pcf13/shadow-work-gpu/chromatic-loom/`。

## 未覆盖范围

本次证明双级联从作者属性、预算、运行时相机、资源生命周期到 D3D11 采样完整贯通，
但尚未证明四级联、分割混合、极远开放场景的 caster 裁剪收益、隔离 GPU pass 成本或
多个完整作品周期。点光全向阴影仍未实现。W1.1 因这些剩余项保持进行中。
