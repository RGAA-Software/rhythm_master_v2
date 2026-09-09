# P5 纹理到 GPU 点属性采样

## 渲染基础已验证，图集成继续

复用现有 GPU 粒子缓冲、实例绘制、bgfx Shader 和资源合同，未引入第二个渲染器，
未把纹理每帧读回 CPU。原有粒子更新的 TiXL MIT 来源仍见
`provenance/tixl_particles.json`，上游源文件及许可未改。新增采样是在项目已有
`gpu_point_vertex.sc` 和私有后端适配器中的功能扩展，未导入额外上游文件。

`GpuPointSampling` 是项目值合同：颜色纹理句柄、颜色混合量和大小混合量。
在点中心的画布归一化坐标采样 LOD 0，UV 边缘钳制；纹理是既有 premultiplied RGBA，
恢复直通 RGB 后混合颜色，透明度相乘。大小由带透明度的 Rec.709 加权 RGB 亮度
决定，混合量均为 0–1。这是绘制时的属性视图，原始 GPU 点记录保持不变；
不是 GPU 数据回读、任意 compute 代码或 CPU 点／路径转换。

采样的点仍使用同一个有界缓冲，没有复制粒子数组。已有 262144 点／单缓冲、
1048576 总点数预算不变。未采样时跳过 Shader 纹理读取，使用私有 1×1 白色回退
纹理保持绑定完整。所有 native sampler／texture 类型留在 bgfx 适配器，资源 RAII。
跨设备／失效句柄、深度纹理、自采样目标和非法混合量拒绝；被采样纹理加入本帧
读保护，后续直接上传不得改变已提交绘制的输入。

## 验证

- `out/p5-gpu-sampling-render-build.log` 保留首次 Shader 编译失败：GLSL 的单参数
  向量构造在 HLSL 展开后无效。改用 bgfx 的 `vec*_splat`，修复后构建记录
  `out/p5-gpu-sampling-portable-shader-build.log`；未放宽 Shader 错误处理。
- `out/p5-gpu-sampling-contract-tests.log`：Null 资源／属性合同通过，包含失效、
  跨设备、自采样和采样后的上传拒绝。
- `out/p5-gpu-sampling-d3d-tests.log`：实际 D3D11 点渲染检查通过。蓝色映射覆盖
  原红色；透明映射隐藏点；亮度缩小点；撤去映射恢复逐字节相同原图。
  使用实际绘制的上红下蓝 2×2 纹理验证采样方向，不只检查均匀上传纹理。
  既有出生／回绕／暂停／寿命／重置及 10000/100000/262144 点绘制检查同时通过。
- `out/p5-gpu-sampling-android-build.log`、
  `out/p5-gpu-sampling-android-contract-tests.log` 与
  `out/p5-gpu-sampling-gles-tests.log`：NDK 构建的当前程序通过 USB 在 Android
  运行同一组合同及 GLES 像素检查；强制要求设备支持 GPU 点，不将“不支持”当通过。

## 图运行时增量

`gpu.texture_sample` 接收原始 GPU 点、纹理和可选颜色／大小混合量标量，输出
带采样属性的 GPU 点视图。`gpu.render` 和节点预览使用同一视图，原始输出仍可
单独绘制。一个视图支持一个映射；连续采样会明确报错，需要多个映射时先合成
纹理，避免后一节点悄悄忽略前一映射。

采样纹理会固定保留到实际绘制和预览，不能在别名节点之后就被纹理池回收。
同一源的多个视图共用点缓冲；新增合同只包含项目句柄和值，不传播 bgfx 类型。
端口和属性使用既有 GPU 点／纹理／标量 wire 类型，旧 Player 按未知算子拒绝。

`out/p5-gpu-sampling-graph-tests.log` 和 `out/p5-gpu-sampling-runtime-tests.log`
通过图合同、数值驱动视图、单缓冲复用、源纹理保留、节点预览、链式拒绝和 Reset
释放；原有粒子／纹理生命周期和源码边界检查通过。
`out/p5-gpu-sampling-content-tests.log` 验证默认预设覆盖；中英文属性／帮助已接入。
Android 同一原生运行时合同通过，记录
`out/p5-gpu-sampling-runtime-android-build.log` 和
`out/p5-gpu-sampling-runtime-android-tests.log`。

`glyph_current` 实际从空白创作、同包 PCM 输出、MP4 导出、Windows 完整部署和
Android 当前 APK／GLES／应用内选择及暂停恢复已验证，见
[作品交付与人工看图记录](validation/data_bridges_authoring_2026-09-10.md)。
记录保留首次画面过暗的失败及修正，不以模块通过或非黑像素替代呈现验收。
