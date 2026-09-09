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

## PBR 绑定对照与决定

进一步把中性 RGB 表达式与未修改的场景片元逐像素比较：普通网格上的基础光照、
sRGB 基础颜色、切线法线、ORM、自发光、阴影采样和环境采样七组均保留结果；
每个绑定对照还要求相对基础光照有实际像素差，避免两边都没有生效却算通过。
Windows D3D11 与 Android GLES 中心 RGB 都依次为 `(29,29,29)`、`(7,2,16)`、
`(15,15,15)`、`(20,20,20)`、`(51,35,83)`、`(0,0,0)`、`(96,96,96)`。
此次两端均创建 24 组程序。采用确定的 1×1 纹理和固定灯光检查绑定，不声称
完成动态阴影生成、复杂环境预滤波或六种变形网格的全流程验收。

日志：`out/p6-material-profile-pbr-build.log`、`out/p6-material-profile-pbr-d3d.log`、
`out/p6-material-profile-pbr-android-build.log`、`out/p6-material-profile-pbr-gles.log`、
`out/p6-material-profile-pbr-boundaries.log`。

**决定：限制采用 RGB 表面表达式子集**，保留上述输入和基础颜色相乘位置；不新增
材质采样器、不修改几何或 alpha。独立资产必须携带两后端编译产物和场景 profile，
未知 profile／绑定／版本拒绝；失败热更保留上次已接受资产，不能发布半个双目标结果。
复用既有词法限制、shaderc、异步资产任务、Godot PBR 适配与程序 RAII，不冻结新依赖。

实现继续完成独立资产合同、预算／生命周期、实例组合键、错误热更、旧包拒绝、
可编辑音乐作品与两端交付；这些尚未完成，不把 P6.1 标成已交付。

词法基础已抽到 `shader_expression`，原图像接口转发到相同实现，生成包装器和旧
容器保持不变。Surface 允许 position／normal，拒绝图像专用 Sample／resolution；
两个 profile 都保留长度、词元、嵌套、字面量与诊断位置限制，未知 profile 拒绝。
Windows `out/p6-shared-expression-tests.log` 的四项检查通过，包含旧图像双目标
编译和容器负例。Android 当前原生二进制通过相同词法及旧图像合同，日志为
`out/p6-shared-expression-android-tests.log`、
`out/p6-shared-expression-android-image-source-tests.log`、
`out/p6-shared-expression-android-image-program-tests.log`。

产物校验抽到 `shader_artifact`，图像入口继续固定 Image profile。Surface 使用场景
varying、最多 24 个明确命名绑定、五组四元素灯光数组、一个 Mat4、六个采样器及
既有数值 uniform。D3D 寄存器按 16 字节对齐、不得重叠，常量区不超过 1024 字节
且必须覆盖已声明绑定；GLES 容器采样器寄存器字段实际为零，不能照搬 D3D 槽位。
两端仍检查固定 FSH12、容器边界、后端代码结构及尾部数据，不声称证明字节码安全。

`out/p6-shader-artifact-tests.log`、`out/p6-shader-artifact-android-tests.log` 通过三个
实际编译变体、两种目标、错误 profile／字段／长度／尾部／过小常量区拒绝。
`out/p6-shader-artifact-image-regression.log` 与
`out/p6-shader-artifact-android-image-regression.log` 保证原图像编译和容器负例仍通过。
首次构建发现测试读取器的有符号／无符号比较警告，已明确转换为 streamoff 后修正；
失败 `out/p6-shader-artifact-build.log` 保留，成功构建为
`out/p6-shader-artifact-binding-build.log` 和 `out/p6-shader-artifact-android-build.log`。

`surface_shader` 已实现独立 `RMSF001` 包：源码、编译器 SHA、Windows／GLES 产物。
未知版本、错误长度、尾部数据、非法表达式或目标产物拒绝，原图像解码器明确拒绝
该包。`surface_template.py` 从唯一场景片元生成 C++ 包装器模板，保持全部 PBR
实现来源；探针也复用该生成器，避免维护两份材质代码。GLES 表面计算指定 highp。
当前资产模块仍未接入 Studio／Renderer，不能仅凭包合同宣称材质功能交付。

`out/p6-surface-bundle-tests.log` 从新包装器重新编译两目标，检查包往返、负例及
C++ 生成包装器与实际编译源码一致，目录为
`out/surface-bundle-profile/12441af80d0b41228444e20da82db180/`。
`out/p6-surface-bundle-android-tests.log` 通过同一包合同；重新构建两端原生探针后，
`out/p6-surface-bundle-d3d.log`、`out/p6-surface-bundle-gles.log` 通过改色与七组 PBR
绑定像素对照。`out/p6-surface-bundle-boundaries.log` 通过。

## Renderer 资源与六种绘制变体

`SurfaceProgram` 为移动专有 RAII 资源，公开句柄仅含设备／槽位／代次，最终释放
限定宿主线程。每设备最多 32 个已接受表面资产、16 MiB 单目标编译字节，统计
不是驱动显存大小。Null 只检查资源与输入合同，真实后端额外进行完整产物校验。
bgfx 私有组件为每个资产持有六种场景程序；全部创建成功才发布句柄，失败回滚。
普通颜色和颜色／深度捕获都会验证引用，失效／跨设备、NaN 参数和越界时间拒绝。
实例合批键加入程序、四参数和时间；相同材质仍可批量绘制，不同材质不能互相覆盖。

`out/p6-surface-renderer-d3d.log` 和 `out/p6-surface-renderer-gles.log` 通过真实 Renderer
的非法产物拒绝并保留原资源、不同参数两次提交／相同参数一次提交及对应红绿像素。
中性表达式复用原有实例、骨架、morph 对照：1000／10000 实例可见，48 骨骼，
四个 morph 目标／3721 顶点，普通与实例模式、morph＋skin 对照均通过，骨骼和
morph 与 CPU 参考的平均像素差均为零。释放后资源计数恢复。

Windows Null／旧图像／旧场景合同在 `out/p6-surface-resources-tests.log` 通过，
原有完整 GPU 场景回归在 `out/p6-surface-renderer-existing-gpu.log` 通过。
Android 原有 GPU 场景回归在 `out/p6-surface-renderer-existing-gles.log` 通过。
Android 原生对应日志为 `out/p6-surface-android-surface_program_contract_tests.log`、
`out/p6-surface-android-image_program_contract_tests.log`、
`out/p6-surface-android-scene_render_contract_tests.log`。
首次测试使用了不存在的 CreateDepth 名称，已改为既有 CreateDepthTexture；
失败 `out/p6-surface-resources-build.log` 保留。新增私有适配器遗漏边界清单导致
`out/p6-surface-renderer-contracts.log` 失败，补齐准确文件路径后
`out/p6-surface-renderer-adapter-boundaries.log` 通过，没有放宽业务层的禁用类型规则。

仍待接入材质节点、Runtime 资产复用、Studio 编译／热更、发布与实际作品。

## 材质节点与 Runtime 基础接入

`material.shader` 为材质修饰节点：必需材质输入，可选统一时间／a／b／c／d 标量，
资产引用使用既有 AssetId 属性；新操作枚举追加，不重排旧值。Scalar 属性限 ±1e6，
运行时有限值检查／钳位与图像表达式一致。后一个表面修饰替换前一个表面程序，
其余 PBR／贴图属性继承输入材质。Scene3D 仅保存稳定生产节点 ID，不保存 GPU 句柄。

Runtime 以资产 SHA 共享程序，把独立节点参数值发布到当前帧输出；ScenePass 按
生产节点 ID 取得当前绑定。含表面表达式的 unlit 材质也计算世界法线矩阵，保证
normal 输入符合变换。显式时间输入覆盖作品时钟，暂停可缓存；参数／时间变化
复用程序，资产替换和 Reset 回收旧资源，预算失败重试条件包含表面资源集合。

`out/p6-surface-graph-tests.log` 通过类型、必需材质和参数范围检查。
`out/p6-surface-runtime-preparation-tests.log` 通过共享资源、标量／100000 秒统一时间、
三个材质／场景预览、暂停、显式时钟、资产替换、分帧准备和释放的 Null 合同。
`out/p6-surface-runtime-regression.log` 保持既有材质、2× 超采样和源码边界检查通过。
Android 当前原生对应合同为 `out/p6-surface-android-scene_graph_tests.log`、
`out/p6-surface-android-surface_runtime_tests.log`、
`out/p6-surface-android-material_runtime_tests.log`、
`out/p6-surface-android-scene_sampling_tests.log`，全部通过；这不是图内材质音乐的 GPU 验收。

首次图测试的资产命名空间拼写错误保留在 `out/p6-surface-graph-build.log`；修正
命名空间及常量节点类型后构建／测试通过。首次 Runtime 测试使用了超出既有上限
的 1000 ms 准备预算，`out/p6-surface-runtime-tests.log` 正确拒绝；改为合法的
100 ms／单节点测试预算后通过，没有放宽实际 Runtime 上限。
仍待资产加载／发布消费者、Studio 编译／热更和实际音乐作品交付。
