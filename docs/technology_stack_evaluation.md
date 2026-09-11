# 整体技术栈评估与选型状态

> 2026-09-06：汇总当前讨论和已确认决定，不代表依赖已导入或四平台已验收。
> 覆盖完整软件，不仅是网络。所有正式方案保存在本项目 `docs/`，不以临时草稿为依据。

## 1. 状态与决策规则

P6.2 GPU 点映射复用本地 TiXL MIT 独立输出算法与既有 bgfx compute 后端，
保持四 vec4 布局；D3D11／GLES 3.1 原生探针与正式 Renderer 像素对照已通过。
不新增通用计算语言或任意缓冲布局。图／Runtime、Windows Studio 作品／导出和 USB Android 内置音乐播放已验证；
详见 [GPU 属性评估](gpu_attribute_evaluation.md)。

P4.2 ImGuizmo 验证更新：vcpkg 仅新增 `imguizmo:x64-windows@1.10`，未升级其他
包。安装二进制所用 ImGui 1.91.9 non-docking 与项目 1.91.9b docking ABI 不同：
`sizeof(ImGuiIO)` 为 3032/3088，`ImGuiContext` 为 10576/11160，HoveredWindow
偏移为 5040/5368（安装包/项目）。证据 `out/p4-imgui-abi-detail.log`。拒绝直接链接
此二进制，保留现有 ImGui；通过 Python 从 vcpkg 校验过的 1.10 下载包仅提取
`ImGuizmo.cpp/.h` 和 MIT 声明，用同一项目 ImGui 编译。源码例外、固定 revision、
文件哈希见 `provenance/imguizmo.json`；私有适配器鼠标交互验证已经通过，范围为
透视/正交、父变换写回、局部/世界旋转、局部缩放和捕获取消。世界缩放明确不支持；
完整 3D 对象选择/手柄工作流仍在接入。手机 Player 不引入 ImGuizmo/ImGui。

R3 已验证 vcpkg `mikktspace 2020-10-06#3`（Zlib，静态链接），仅新增
`x64-windows` 与 `arm64-android` 包，没有升级共享安装的其他依赖。Windows 与
USB 手机通过基本几何、镜像 UV 接缝和异常输入检查，采用其切线生成；C 类型仅留在
Scene3D 私有同步适配器。版本、许可和安装 ABI 见 `provenance/mikktspace.json`。
点光/聚光、四槽材质、方向/聚光阴影以及 IBL 已通过两端像素检查。IBL 复用 Godot MIT 环境 BRDF、TiXL MIT GGX 预过滤和既有 GLM MIT 颜色转换，没有新增库；见 [环境光照](environment_lighting.md)。[GLB 内嵌 PNG/JPEG 材质](glb_material_images.md)已通过 cgltf → 既有 FFmpeg → 发布包 → 两端 GPU 检查，未新增解码器。

R2 的 RGBA16F、sRGB/线性转换、可采样 D24S8 与景深已通过 D3D11 和 USB
Adreno 650/GLES 3.1 的实际像素检查。色彩适配参考已安装 vcpkg GLM 的 MIT 分支，
Reinhard 和景深参考本地 TiXL MIT 源码；未新增解码器、Qt 或渲染后端。
范围与限制见 [颜色流程](color_pipeline.md) 和 [深度流程](depth_pipeline.md)。

R1 实例批处理及 1 千/1 万实例像素检查已通过 D3D11 与 Adreno 650/GLES 3.1；
[首批交付](validation/scene_instances_2026-09-08.md) 未引入新依赖。
GPU 粒子也已通过两端 compute 更新/绘制与实际作品检查，复用 TiXL MIT 粒子代码；
见 [GPU 粒子交付](validation/gpu_particles_2026-09-08.md)。Android 新构建默认 GLES 3.1，
已有缓存不强制切换，打包最低 GLES 要求与实际编译档位一致。

R0 已实测 D3D11 与 USB Adreno 650/GLES 3.1 的实例和计算写入/绘制；
GLES 3.0 编译档位只有实例路径。详见 [GPU 验证](validation/gpu_execution_2026-09-08.md)。
这确认了现有 bgfx 内的实现路径，不代表所有 Android 设备支持，也未更换后端。


2026-09-09 当前执行路径见 [整体实施计划](master_implementation_plan.md)，
已交付功能证据保留于 [渲染能力路线](rendering_capability_roadmap.md)。
新规划没有冻结 ImGuizmo、文字排版/矢量依赖或通用材质/compute 扩展，仍按下表状态验证。
GPU 计算/批量实例、可采样深度及新材质格式在各目标后端先做短时验证，
没有因写入规划就冻结新的依赖、渲染后端或 Android 能力声明。现有 bgfx 与 vcpkg
优先规则保持；长时间测试后置不取消待定能力的必要验证。

产品定位以 [product_scope.md](product_scope.md) 为准：音乐可视化创作与播放。
依赖评估围绕音频分析、同步控制、节点编辑、动态效果、展示/导出与跨平台 Player，
不以壁纸宿主或桌面嵌入为选型目标。透明原生窗口仍是此前后置的独立待定功能。

视觉效果的历史增量从既有 TiXL 源码快照聚焦复用了高斯模糊、降采样和 Perlin 噪声，
改编为私有 bgfx shader，不引入 TiXL 宿主。逐文件出处、MIT 许可和改动记录在
`provenance/tixl_effects.json`。2026-09-11 用户明确收紧边界：TiXL 继续用于节点逻辑、
参数和作品结构参考；核心画面算法采用 Godot 成熟实现。TiXL Gaussian/downsample
仅保留为历史参考，运行时 `texture.blur` 改用同一固定 Godot 提交的 13-tap raster
Gaussian 核和有界 mip 链。Glow/bloom 将按固定 Godot 提交
`cb41ea115914c61a8329087b4cffbad7477b8427` 的 Dual Filtering、HDR 筛选、多级
downsample/upsample 和 tone-map 合成适配，详见 [Godot 参考计划](godot_3d_reference_plan.md)。
Hazel 提交 `1feb70572fa87fa1c4ba784a2cfeada5b4a500db` 已检查，其公开代码没有 bloom/blur
后处理实现，因此不作为该算法来源。

2026-09-07 用户补充：**所有第三方依赖及构建工具优先使用 `C:/source/vcpkg`**。
先检查已安装的目标 triplet、版本、features、ABI 和许可证；缺失时优先评估
vcpkg port/feature，再考虑独立源码或维护 fork。FFmpeg 不使用项目自建源码脚本。
不能把“优先”解释成不验证即替换已验收依赖，也不批量升级共享 vcpkg 中的其他包。
现有接入和明确缺口见 [vcpkg 接入核对](validation/vcpkg_dependencies_2026-09-07.md)。

- **确认**：用户明确决定或既定产品边界；实现仍须验证。
- **首选**：当前架构推荐，需原型证明后冻结版本。
- **候选**：待比较，不等于已经引入，也不能宣称获得对应能力。
- **不采用/后置**：当前范围不加入，改变决定需明确记录理由。

已确认：零 Qt；Studio 在 Windows/macOS，Player 在 Windows/macOS/Android/iOS；
独立可构建渲染目录；源码与复杂 UI 解耦；FFmpeg 唯一媒体后端；Godot 为 3D 主要
设计和代码参考；GammaRay 自有 common 可复用；PC 主控、手机扫码参与的集群需求。
开源且追求商业级质量，不推断收费、激活或封闭源码；项目对外 LICENSE 尚未选择。

多平台是首阶段共享架构的硬约束：Studio 为 Windows/macOS，Player 为四平台。
2026-09-07 用户验收当前 Windows 版本，并因没有 Apple 设备决定最后移植 macOS/iOS。
实施顺序为 Windows → Android Player → Apple 平台；Apple 工具链、Metal 和实机验证
后置且不阻塞前两者，当前继续做公共边界和兼容性评估。Apple profile 保持待验证，
不能据此声称四平台已通过；下文全平台验证要求按此顺序分阶段执行。
共享核心、公共契约、包格式与依赖/工具链验证现在覆盖全部目标平台；Windows 验收门
约束宿主交付顺序，不把跨平台设计后置成移植任务。移动端宿主实现不提前启动。

## 2. 组合架构

```text
Studio：Win/macOS                    Player：Win/macOS/Android/iOS
UI / 节点 / 属性 / 时间线            播放 / 扫码 / 生命周期
                \                    /
                 应用服务与共享运行核心
          graph / 参数 / 时间 / 音频 / 场景 / 集群输入
                             |
                       RhythmRender
                             |
                       bgfx 私有实现
```

核心竞争力是类型化视觉编程、易用的语义节点、预制内容和可靠实时执行。
成熟库解决通用基础设施，不能把 UI 画布当作图引擎，也不能把图形后端当作完整 3D 引擎。
渲染目录必须独立构建；网络、FFmpeg、ImGui 的适配处于相应模块外边界，
不反向成为 render 公共 API 依赖。Cyber UI 独立目录，允许依赖 ImGui，禁止依赖应用业务。

## 3. 库与模块评估表

### 3.1 平台和编辑器

| 责任 | 选型/状态 | 原因与必须验证的部分 |
| --- | --- | --- |
| 窗口、输入、DPI | SDL3，首选 | 统一平台事件；透明置顶、原生标题栏/阴影和移动生命周期需平台适配，不靠选库自动实现 |
| 桌面 UI | Dear ImGui docking，首选 | 同一 GPU 表面组合 UI/预览；中文 IME、复杂文本、无障碍与多窗口另设验收 |
| 节点画布 | imgui-node-editor 自维护 fork，首选 | 验证缩放/端点/命中/撤销集成/大图裁剪；仅持有视图状态，不拥有业务图 |
| Cyber 控件 | 自有独立 ImGui 组件库 | 重建主题、输入、选框、按钮、弹窗及焦点行为；不迁移 Qt/QSS 实现 |
| 信号观察 | ImPlot，候选 | 频谱与诊断曲线；不把它当作正式时间线引擎 |
| 3D 编辑手柄 | ImGuizmo，候选 | 变换交互，不负责场景资源和层级 |
| 文件对话框 | Native File Dialog Extended，桌面首选 | 移动文件选择、权限和安全书签在原生适配中 |
| 手机宿主 | C++ 核心 + Kotlin/Swift 薄宿主，首选 | 相机/权限/分享走系统能力；具体 SDL surface/native UI 组合须原型验证 |

SDL 只承担既定平台职责，不同时启用 SDL Renderer、SDL GPU 和 bgfx 三套主渲染。
ImGui 预览采样现有 TextureHandle，禁止每节点一个原生窗口或常规 CPU 图像回读。
ImGui 自身不提供完整国际化/无障碍；引入字体不能等同于文本编辑与排版完成。
节点性能还需要可见性裁剪、分级预览、低频缩略图、稳定模型和增量编译。

### 3.2 渲染和 3D

| 责任 | 选型/状态 | 原因与边界 |
| --- | --- | --- |
| 渲染 API | 自有 RhythmRender，确认独立性、优先复用 | 保留 Null backend；审查旧组件能否适应新图契约 |
| GPU 后端 | bgfx，首选 | 验证多预览/Compute/资源绑定/透明窗口；不在业务层绕过后端 |
| Shader 工具 | bgfx shaderc，首选 | 编辑器异步编译与缓存；Player 载入目标产物，不携带整套编译工具 |
| 3D 设计与源码 | Godot，确认 | 参考场景/材质/灯光/glTF/动画，按职责适配，不嵌入全引擎 |
| 核心画面效果 | Godot，确认 | 普通 blur、glow/bloom、HDR、tone mapping 和主要后处理采用固定源码基线；TiXL 参考图逻辑与参数组织，不用简化拼装替代完整效果链 |
| 数学 | 已有 vcpkg GLM 0.9.9.8#2，已接入私有数学适配层 | Windows 数值回归通过；矩阵、四元数和法线变换直接用 GLM，公共接口保留项目值类型，见 [3D 验证记录](validation/scene_foundations_2026-09-07.md) |
| 2D 物理 | vcpkg Box2D 3.1.1 + 项目 overlay #1，Windows/Android 初始验证通过 | 修复小距离漏算半径；私有 RAII 包装；不是 3D 物理库，见 [验证记录](validation/physics2d_2026-09-07.md) |
| glTF 解析 | vcpkg cgltf 1.15，初始静态 GLB 验证通过 | Windows/Android 导入回归通过；仅自包含静态三角形及标量材质，图/应用接入待完成，见 [3D 验证记录](validation/scene_foundations_2026-09-07.md) |
| 网格预处理 | meshoptimizer，候选 | 优化放构建工具，runtime 仅保留所需解码 |
| 压缩纹理 | KTX-Software/Basis Universal，候选 | 与 bimg/texturec 评估重叠，只冻结一条资源管线 |
| 灯光/材质/Render Graph | 自有模块 + Godot 参考 | bgfx 不替我们提供完整产品层，需资源生命周期和设备 profile |

bgfx shaderc 具有自己的语言、uniform 与资源绑定契约，不承诺任意 GLSL/HLSL 原样可用。
Slang 等替代编译路线先后置；只有原型发现明确瓶颈，再比较 Diligent 等后端，
不同时维护两套主渲染器。暂不因未来 3D 扩展添加第二个物理引擎。
Godot 主参考的细节见 [3D 方案](godot_3d_reference_plan.md)。

### 3.3 音频和媒体

| 责任 | 选型/状态 | 边界 |
| --- | --- | --- |
| 解封装/解码/编码/封装 | FFmpeg，确认 | 唯一媒体后端，库级集成，不用 ffplay 子窗口 |
| 重采样/格式转换 | FFmpeg swresample/swscale | 不重复进行不必要转换；视频 GPU 路径另测 |
| 音频特征 | 自有 `src/audio/feature` 审核后复用 | 保留唯一频域映射/包络/节拍语义，不再叠加旧新 filter |
| FFT 优化 | 现有实现优先，PFFFT 候选 | 先测精度、幅值归一和目标机性能，不能为换库改变听感响应 |
| 音频设备 | SDL 音频或原生 adapter，待定 | 唯一设备管理策略，不是第二套播放器 |
| 系统声音 | 各平台 capture adapter | Windows 可复用 WASAPI；其他平台权限/可用性独立验证 |
| VLC / Qt Multimedia | 不采用 | 不作为兼容后端、不因复制 Godot/GammaRay 代码重新带入 |
| miniaudio 高级播放/解码 | 不采用 | 早先候选被 FFmpeg 决定收敛；纯设备用途也未选定 |

统一链路是 PCM -> 一套分析 -> 图输入/集群分发；网络会话不重复建音乐分析器。
FFmpeg 的唯一性不等于它自动解决设备、主时钟、seek、循环和渲染提交。
详见 [媒体方案](media_pipeline_plan.md)。

### 3.4 节点、参数和内容

| 模块 | 选型 | 不委托给画布库的职责 |
| --- | --- | --- |
| Graph Domain | 审核复用 + 自有实现 | 稳定 ID、类型/端口、子图、命令和撤销 |
| Graph Compiler/Runtime | 自有实现，借鉴参考项目 | 依赖、按需执行、增量缓存、反馈、状态和时间 |
| Parameter/Timeline | 自有元数据和受限表达式 | 常量/连接/绑定/关键帧、局部时钟、cue |
| Content Registry | 数据驱动 | 底层算子 -> 语义节点 -> 预设 -> 完整模板 |
| Inspector | 从契约生成 | 组合节点分内部职责展示，常用参数优先，不要求下钻每个原子节点 |

初期不引入完整 Python/C# 脚本环境或任意 native 插件。表达式是受限且有预算的
类型化执行，避免用动态脚本绕过跨平台/安全/线程模型。
内容数量目标和正式工作流以 [内容计划](builtin_nodes_and_presets_plan.md) 为准；
不能靠增加重复 C++ 特效类型凑数。节点能力以 [能力计划](visual_authoring_capability_plan.md) 为准。

### 3.5 存储、文字和基础库

| 责任 | 选型/状态 | 边界 |
| --- | --- | --- |
| 图/运行包 schema | Protobuf，既定方向 | 模块化 schema/codec；不把 generated headers 扩散到全工程 |
| 清单/小配置 | JSON，既定方向；nlohmann_json 优先复用 | 只在 I/O 内解析，不把 JSON 当公共运行时值系统 |
| 索引/搜索/cache | SQLite，既定方向 | cache 可重建；不再为了 common 配置加入 LevelDB |
| 包与资产 | 版本化目录/发布包、内容哈希 | 原子提交、校验、大小和解压限制；不写 exe 目录 |
| 字体 | FreeType，首选 | 栅格化和 glyph 缓存，不负责语言排版 |
| 文字塑形 | HarfBuzz，候选 | 与编辑光标/选择/IME 的整合需要专门验证 |
| 作品文字栅格化增量 | vcpkg FreeType Windows 2.12.1#3 / Android 2.14.3 | 两端原生模块及中文截图通过；[文字实施记录](text_rendering.md)。节点、资产发布及输入体验继续推进，不等于 P5 全部交付 |
| P5 矢量基础，验证中 | vcpkg Clipper2 2.0.1 / Earcut 2.2.4#1，Windows 与 Android triplet | 私有轮廓、描边和孔洞三角化适配；[验证范围](vector_graphics.md)。尚未接入图、GPU 或作品，不宣称任意 SVG 支持 |
| Unicode/语言格式 | ICU，候选 | bidi、复数/格式/locale 服务与数据裁剪评估，不是普通字符串替换 |
| 日志 | GammaRay 适配 + fmt/spdlog，首选 | 节流、脱敏、轮转；日志实现和重头文件不扩散 |
| 异步 | GammaRay async + 私有 Asio，首选 | 有界任务、取消、drain；不能一设备一线程 |
| 文件/下载/重连 | GammaRay 按模块提取 | TLS 验证默认安全，依赖私有；不用 common 总聚合目标 |

现有 common 要求 C++23，而部分可迁移核心是 C++20。全项目标准版本尚未冻结：
先跑 MSVC/Apple Clang/NDK 工具链探测，不因为一个依赖声明就假设所有语言特性可用。
网络/媒体/渲染边界的原始指针只留在 RAII adapter 内，公共 API 使用值、引用、
span、稳定句柄及有明确所有权的智能指针；Google 命名、4 空格和声明初始化不变。

### 3.6 集群与安全传输

| 责任 | 选型/状态 | 评估 |
| --- | --- | --- |
| 演出模型 | PC 主控 + 手机本地渲染，方案方向 | 复制时间/输入/场景，不默认推流视频 |
| 传输语义 | 可靠控制 + 时效数据报 + 受限资源下载，首选 | 控制不被资源队列堵塞，状态过期丢弃 |
| QUIC | MsQuic/quiche 候选，未冻结 | 比较四平台、C++ 接入、TLS、取消、资源和工具链成本 |
| WSS | 兼容路径，首选复用 Asio2 | TCP 队头阻塞无法靠 latest-value 完全消除，不能承诺同等延迟 |
| HTTP 下载 | GammaRay cpr 封装审核后适配 | cpr 负责 HTTP，不把它描述为 WebSocket 库 |
| QR 生成 | 自有封装 + Nayuki qrcodegen，复用候选 | 保留 MIT 来源，从 Windows target 分离 |
| QR 识别/权限 | 移动原生适配，待定 | 生成二维码不等于已实现摄像头扫码 |
| TLS/密钥 | 成熟传输库及平台安全存储 | 不另造密码协议，不用 MD5/FNV 作认证 |

MsQuic 官方支持页面并不能证明目标四平台均被正式支持；quiche 有移动构建入口，
但增加 Rust/FFI 构建成本，旧示例不能作为当前 SDK 验收。选库必须跑原型，
暂不冻结 TLS provider，避免同时链接无规划的多套 OpenSSL/BoringSSL。
具体同步、安全、包准备和容量门槛见 [集群方案](cluster_playback_plan.md)。

2026-09-07 增量证据：MsQuic/OpenSSL 已通过 Windows 10 与 Android API 34 真机的
双向 LAN TLS/stream/DATAGRAM 实验；quiche 的两平台 C API 内存传输实验通过。
独立 OpenSSL 3.5.8 正式版、项目有界队列和基本取消竞态已在两端通过；
候选仍未链接主应用，完整房间接入、故障/迁移和最终平台采用范围尚在验证。
详见 [QUIC 候选记录](validation/quic_candidates_2026-09-07.md)。

### 3.7 构建、验证和发布

| 责任 | 选型/状态 | 要求 |
| --- | --- | --- |
| 构建 | CMake Presets + Ninja，首选 | 20 workers、增量；分模块目标，跨平台 host tools 与 target libs 分离 |
| 依赖 | vcpkg manifest/固定 revision，首选 | 自维护 fork 独立记录；不同时无锁地从多处拉同一依赖 |
| 单元/基准 | GoogleTest / Google Benchmark，首选 | 图/协议/时间/资源测试，记录硬件和构建选项 |
| 性能 | Tracy + 自有帧/队列指标，首选 | 按后端验证 GPU 指标覆盖，不默认全平台可采集 |
| UI 自动化 | 项目命令/输入注入与截图；ImGui Test Engine 候选 | 后者单独查许可和支持范围，不能按 ImGui MIT 自动判断 |
| 崩溃 | Crashpad 桌面候选 | 不直接承诺同样覆盖全部手机；平台符号化/隐私另验 |
| 安装/更新 | 各平台打包、签名、原子更新/回滚 | 工具和服务尚待发行渠道/许可确认，不塞进渲染核心 |

源码目录和公开头按职责拆分，避免巨大 `common.h`、通用常量集合、泛型 JSON API。
PCH 只包含稳定依赖，不通过全量 unity build 掩盖模块耦合；缓存复用默认开启。
每个依赖记录用途、来源、版本、许可、大小、编译代价、平台支持证据与退出方案。

## 4. 首轮验证门与后续行动

用户最新优先级调整：透明窗口/透明后缓冲及点击穿透为整体路线最后的待定功能，
不再作为首轮验证门或普通窗口后端采用的阻塞条件。此前表格中有关透明窗口的
验证要求仅在未来确定实施该专项时适用；不因此宣称当前后端已支持透明输出。

首阶段验证与 Windows 最小闭环已获授权并已进入实现：共享核心、Windows GPU/编辑器、
事务保存已形成可构建目标，Windows 自动测试和 Android 实机共享契约测试已通过。
具体证据及尚未关闭的风险见 [验证报告](validation/phase_a_2026-09-06.md)；
来源、确切版本、SDK 差异及许可证据见 [依赖记录](../third_party/README.md)。
执行顺序、共享平台矩阵、最小契约、用例和决策条件见
[首阶段执行计划](phase_a_execution_plan.md)。依赖下载或脚本固定版本不等于验证通过。

共享验证线 P0–P4 从首阶段覆盖 Windows/macOS/Android/iOS，宿主线 W0–W5 先交付
Windows。每项共享依赖分别记录工具链编译、测试执行及真实平台功能证据；
无法取得证据的单元格保持待测，不冻结为已验证四平台支持。

以下四组仍是必须按所属模块推进的架构验证任务，尚未全部执行通过；不要求同时全面开发：

1. **UI/画布**：中文输入、组合节点 Inspector、缩放/端点、4K 大图、GPU 节点预览、
   多语言与无障碍缺口清单；明确哪部分暂不支持。
2. **图形自由度**：可编辑 shader、多纹理、多 pass/反馈、Compute、多预览、
   透明置顶窗口和设备恢复；失败再讨论后端/编译器替代，不能悄悄绕过抽象。
3. **媒体统一**：FFmpeg PCM/视频输出、设备/seek/loop/主时钟、已有频谱结果对照、
   多源负载和受限队列；硬件解码及纹理互操作单独证明。
4. **网络可移植性**：传输候选编译/安全握手/时钟/丢包/取消及模拟客户端预算；
   Windows 通过后才开始移动宿主与真机/场地容量验收。

原型之后冻结最低系统/GPU、编译器、依赖版本、媒体/profile、许可证和性能基线。
总体实施顺序仍以 [架构总览](architecture_overview.md) 为主，子计划不能各自宣布全产品完成。
3D 代码参考调查和 FFmpeg 方向已确认，不代表尚未审查的具体源文件可直接批量导入。

## 5. 评估依据与资料

本轮依据旧项目 CMake、现有独立 render/audio 模块、GammaRay common 文件检查，
以及以下已查阅的上游入口；不是完整源码审计，也没有产生性能测试结果。
正式集成须再次确认固定版本和平台证据，不能长期依赖浮动 master 文档。

- [SDL3](https://wiki.libsdl.org/SDL3/FrontPage)、[Dear ImGui](https://github.com/ocornut/imgui)、[imgui-node-editor](https://github.com/thedmd/imgui-node-editor)
- [ImPlot](https://github.com/epezent/implot)、[ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)、[Native File Dialog Extended](https://github.com/btzy/nativefiledialog-extended)
- [bgfx](https://bkaradzic.github.io/bgfx/overview.html)、[shaderc](https://bkaradzic.github.io/bgfx/tools.html)、[GLM](https://github.com/g-truc/glm)、[Box2D](https://box2d.org/documentation/)
- [cgltf](https://github.com/jkuhlmann/cgltf)、[meshoptimizer](https://github.com/zeux/meshoptimizer)、[KTX-Software](https://github.com/KhronosGroup/KTX-Software)
- [Diligent](https://github.com/DiligentGraphics/DiligentEngine)、[Slang](https://github.com/shader-slang/slang)
- [FFmpeg](https://ffmpeg.org/libavcodec.html)、[PFFFT](https://github.com/marton78/pffft)
- [Protobuf](https://protobuf.dev/support/version-support/)、[SQLite](https://www.sqlite.org/whentouse.html)、[HarfBuzz](https://github.com/harfbuzz/harfbuzz)、[ICU](https://unicode-org.github.io/icu/userguide/format_parse/messages/)
- [MsQuic 平台支持](https://github.com/microsoft/msquic/blob/main/docs/Platforms.md)、[quiche](https://github.com/cloudflare/quiche)
- [CMake Presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)、[vcpkg manifest](https://learn.microsoft.com/en-us/vcpkg/concepts/manifest-mode)
- [GoogleTest](https://github.com/google/googletest)、[Benchmark](https://github.com/google/benchmark)、[Tracy](https://github.com/wolfpld/tracy)、[ImGui Test Engine](https://github.com/ocornut/imgui_test_engine)、[Crashpad](https://chromium.googlesource.com/crashpad/crashpad/+/HEAD/README.md)

### Focused polar / sector mapping increment (2026-09-07)

Material Maker revision `ad19fcf0ee34a7caf74df709dc4de7112f0d467d` contributes only
its MIT angular-replication function, translated to the existing bgfx fragment
contract. TiXL's MIT forward polar mapping is adapted at the already pinned
revision. Exact sources, hashes and changes are in their effects provenance JSON.
These are focused source-extraction exceptions, not new package/backend adoption;
vcpkg remains preferred for dependencies and build tools. GLES compilation alone
does not freeze Android visual/performance support. See the prismatic-lotus
validation record for the remaining device gate and inspected-source exclusions.

### Windows font and template encoding correction (2026-09-07)

User requested restoration of the original font. Windows Studio/Player again read
local Microsoft YaHei (`C:/Windows/Fonts/msyh.ttc`, 18 px, ChineseFull), with the
original ImGui fallback when unavailable. No system font is redistributed. The
Noto font experiment and 32-bit ImGui character ABI change are no longer active;
its source/notice files remain as an unused reference, not a selected UI font.

The reported prismatic-lotus corruption was in authored template text: UTF-8 had
been decoded as GBK and rewritten as valid but incorrect Han characters. The
source manifest and generator were corrected with explicit UTF-8 I/O. Exact
catalog/title/package assertions replace the insufficient glyph-only diagnosis.
See `validation/template_title_encoding_2026-09-07.md`.


### 2026-09-07 validated media profile follow-up

Windows application media now uses the isolated vcpkg manifest in
`probes/media/vcpkg.json`: FFmpeg 6.1.1#11, no default/GPL/nonfree features,
avcodec/avformat/swresample/swscale/zlib, zlib 1.3.1 for the historical port's
library-name compatibility. Both DLL configurations report LGPL-2.1-or-later.
Exact hashes/configuration and matching patched source/recipe are recorded in
`provenance/media_lgpl_windows.json`; the Python application deploy carries the
matching notices/build archive. This does not select the outbound project license.
The existing shared vcpkg installation remains unchanged.

Android uses a separate isolated manifest installation and API-26 overlay triplet;
its decoder probe passes on the connected device, including PNG/embedded bytes.
This is not APK audio/image acceptance. See
`validation/media_application_2026-09-07.md` for evidence and remaining limits.

2026-09-08 follow-up: the same isolated Android vcpkg installation is now linked
into the music Player APK. Python packaging retains exact source/build materials
and a verified object/archive relink bundle for the static LGPL combination.
Media-enabled native decoder/GLES tests pass on the device; Java/SDL application
audio and lifecycle acceptance remain separate. No outbound project license is
selected. See `validation/android_music_application_2026-09-08.md`.

2026-09-08 export follow-up: the existing Windows LGPL FFmpeg SDK's MPEG-4/AAC
and H.264/Media Foundation paths pass decoded-frame and sample-duration checks.
Studio exports through the existing SDL 3.2.20 process API and a dedicated encoder
thread; integration exposed and resolved SDL STA/MF apartment incompatibility.
No new media backend or SDK build was introduced. Android MPEG-4/AAC and GPU
readback have native device evidence; Apple and general hardware interop remain
unvalidated. See `validation/studio_export_2026-09-08.md` and provenance records.

The same custom FFmpeg I/O adapter now handles package-owned audio bytes as well
as video. File/embedded audio decode and exact seek pass on Windows and Android;
an actual Studio-published soundtrack drives Adreno GLES pixels. This adds no
decoder, package-library or device-output dependency. See
`validation/work_soundtrack_2026-09-08.md` for the retained limits and pending
Android application acceptance.

## R4 图像 Shader 增量验证（2026-09-08）

R6 2026-09-09：FXAA 采用 `glsl-fxaa` 3.0.0 的独立 shader 源码提取，
精确版本、MIT AND BSD-3-Clause 通知、vcpkg 缺口和适配见
[抗锯齿契约](antialiasing.md) 与 `provenance/fxaa.json`。
已通过 Windows D3D11 / USB Adreno 650 GLES 的真实像素及发布/缓存检查；
不引入 npm 运行依赖，不冻结 Apple 支持。

2026-09-09 更新：已在项目隔离目录构建 vcpkg bgfx tools 1.129.8940-496#1。
其 shaderc 1.18.129 输出 FSH11，原生图像 profile 拒绝，不能直接替换现有 FSH12 工具。
因此采用有实测兼容差异依据的源码例外：从已记录快照提取 1,881 文件，Python 准备、
CMake/Ninja 20 workers 在项目内重建 shaderc 1.19.157。两端全部 48 个 Shader 与原工具
输出逐字节一致，编译/发布及 D3D11、USB GLES 像素验证通过。共享 vcpkg 安装、
图形 ABI 和旧仓库保持不变。源码重建待办关闭；跨机器主机 EXE 位级可复现仍未声称。
详见 [构建流程](host_shader_tool_build.md) 和 [证据](validation/host_shader_tool_2026-09-09.md)。

以下为本次验证前的历史记录：

私有词法适配直接使用 vcpkg 已安装 stb C lexer（两端版本、MIT 选择和兼容差异见 `provenance/stb_lexer.json`）。既有 shaderc 1.19.157 已验证 Windows s_5_0 / Android 300_es 的 FSH12 固定绑定、实际像素和作者编译闭环；本地 Studio 部署携带独立工具与完整已记录通知。当前 triplet 未安装 bgfx tools，已检查 port 1.129.8940-496#1，其与当前后端兼容性未验证，不更换共享包或 graphics ABI。现有工具来源、709 个编译源文件与二进制哈希见 `provenance/shaderc_host.json`；在本项目重建工具及可复现性仍待办。该结果仅确认受约束图像 profile，不确认通用材质/compute 或 Apple 编译。详见 [实现与验证](image_shader.md)。


### P6 RGB 表面表达式（2026-09-10）

限制采用现有 bgfx／Godot 适配材质路径上的 RGB 染色表达式，不增加依赖。
复用已验证的 shaderc 1.19.157 和 vcpkg stb 词法适配，场景绑定、六种顶点变体
与 D3D11／GLES 实际像素验证已通过；独立表面格式避免放宽旧图像格式。
Studio 双目标编译、错误热更、源码恢复、真实音乐作品、导出及两端内置交付
已通过。范围仅为光照前 RGB 乘数，保留透明度、法线／贴图及光照契约；
不是完整材质语言、通用 compute 或 Apple 编译支持。具体范围、失败和证据见
[材质评估](material_shader_evaluation.md)与[作者验收](validation/surface_authoring_2026-09-10.md)。

### P6.3 透明候选决定（2026-09-10）

基于本地 bgfx example 19 的单位权重累积／透射率双几何 pass 探针已在
D3D11 与 Adreno 650 GLES 验证。顺序稳定，但相交物体前后颜色错误仍显著，
手机短探针成本约 2.9 倍、附件量 2.5 倍，本轮不采用该候选，不新增依赖或
产品模式。可选 2× SSAA 保留已验证的限制采用决定。来源、测量边界和未解决
透明限制见 [画质评估](render_quality_evaluation.md)；不等同所有 OIT 方案的结论。

### P6.4 附加输出决定（2026-09-10）

1002 节点请求裁剪／预览生命周期两端合同与 D3D11／GLES 几何法线表达式
像素探针通过。保留按需显式颜色／深度；本轮不新增通用法线或对象 ID 附件，
不冻结 MRT／整数附件支持，也不宣称 GPU 变形拾取已解决。实际消费者、来源、
预算和重新进入条件见 [附加输出评估](scene_output_evaluation.md)。
