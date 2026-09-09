# P6.1 用户材质 Shader 验证

2026-09-10。本页记录候选验证，不把编译成功当作材质编辑器已经交付。

## 已有能力和候选边界

图像表达式可以通过 `material.textures` 驱动 PBR 贴图，但它不具有世界位置、
表面法线或直接材质片元上下文。现有图像容器固定四边形 varying、一个采样器和
两个参数 uniform，不能直接用于场景顶点程序。保留其严格校验，不放宽成任意 Shader。

首个实验仅让用户表达式返回 RGB tint，在采样基础颜色之后、PBR 光照之前相乘；
输入为 UV、世界位置、几何法线、时间和四个标量。颜色限制 0–1，NaN 归零。
实验不改变 alpha、深度写入、法线、粗糙度、自发光、阴影或 IBL 的既有语义。
这不是完整材质语言：片元修改 alpha 而沿用整物体深度策略会产生另一类错误，
必须等待透明合同；修改顶点还会涉及阴影、剔除和包围盒一致性。

## 复用与维护成本

使用现有第一方场景包装器及已适配的 Godot BRDF／灯光／阴影／环境实现，来源
与通知继续由对应 `provenance/` 文件管理。没有复制新上游文件，也没有引入新库。
本地 Godot `scene/resources/material.cpp` 可供材质模式参考，但当前保留快照不含
完整 ShaderLanguage 编译器；不能声称已审计或复用其完整语言实现。
类型检查直接使用已验证的 shaderc 1.19.157；vcpkg 工具版本不兼容的既有决定
见 [构建记录](host_shader_tool_build.md)，此次不升级共享依赖。

场景有普通／实例／骨架／骨架实例／morph／morph 实例六个顶点变体。未来若采用，
每个用户材质必须正确管理对应程序组，实例合批键还要包含材质程序和参数。
容器需要独立 profile、场景 varying 签名和精确绑定规则；旧 Player 必须拒绝新能力。
资产准备、异步编译、失败保留旧版本和编辑历史可复用原有流程，不能绕过发布校验。

## 编译探针

`python tools/probe-material-profile.py` 在唯一 `out/material-profile/<run>/` 下生成
试验源码和产物，不修改生产 Shader，不接收任意用户输入，不作为发布编译器。
对每个后端编译六个实际场景顶点和三个 RGB 表达式，重复表达式产物必须字节一致；
核对 varying 签名及现有 PBR 绑定仍然存在，额外绑定仅允许两个表面参数 uniform。
编译器必须拒绝返回 vec2 的类型错误及未定义函数，保存原始诊断。

`out/p6-material-profile-compile.log` 通过；运行目录
`out/material-profile/6581bc4934bc42b2b403ddc97b0bbb9f/`。
Windows SM5 和 Android GLES 300 各通过六个顶点签名、三个重复片元和两个负例。
场景片元输入签名为 601528614，与图像 profile 的签名不同。
`results.json` 保留源码／编译器／各产物哈希、实际 uniform 类型和寄存器位置。
该记录只证明主机编译和容器头兼容，不证明手机驱动链接、像素、热更或旧包拒绝。

## 原生第一步

共享 `material_profile_gpu.cpp` 是测试专用原生适配器，所有 bgfx 资源由既有
`GpuHandle` 管理，公开测试入口仅有路径和固定容量回读 span。宿主在设备销毁
之后才释放回读内存，包含异常路径；没有在生产 Renderer 增加任意 Shader 后门。
Windows D3D11 和 USB Adreno 650 的 GLES 均创建 18 个场景程序组合，普通场景
顶点＋参数表面片元的实际中心像素从 `(255,0,0,255)` 变为 `(0,255,0,255)`。
其余顶点组合在本步只创建程序，没有声称各自骨架／实例／morph 像素验收。

证据为 `out/p6-material-profile-native-target-build.log`、
`out/p6-material-profile-d3d.log`、`out/p6-material-profile-android-build.log`、
`out/p6-material-profile-gles.log`；源码边界检查通过
`out/p6-material-profile-boundaries.log`。首次命令写错不存在的构建目标，失败日志
`out/p6-material-profile-native-build.log` 保留，改为实际 `windows_gpu_execution_probe`
目标后构建成功。没有用重跑掩盖编译／像素失败。

## 后续门槛

随后必须检查基础贴图、
法线、灯光、阴影及不同顶点变体的实际输出，才能决定是否采用 RGB 子集。
即使采用，也要完成独立资产合同、预算／生命周期、错误热更、旧包拒绝、可编辑
音乐作品与两端交付；上述尚未完成，不把 P6.1 标成已交付。
