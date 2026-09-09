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

下一步接入显式纹理采样节点、点输出视图、资源保留和实际作品；上述后端验证不等同于
Studio 创作、Android 应用或运行包交付已完成。
