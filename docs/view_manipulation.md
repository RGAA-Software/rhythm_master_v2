# P4：视图直接编辑实施记录

当前推进 P4.5（2026-09-09）。最新状态如下；后文按开发顺序保留阶段证据，
其中“当时未交付”的说明不覆盖本表和末尾的更新。

| 工作 | 当前已验证范围 | 仍需推进 |
| --- | --- | --- |
| P4.1 | 根图 affine 四种手柄、吸附、事务、实际 Studio 像素/保存发布 | 非坐标保持图像路径继续明确限制 |
| P4.2 | 根图 3D 作者手柄、静态几何拾取、当前帧相机/对象、两端共享基础 | GPU 变形/骨骼/morph 拾取受限，不用未变形几何猜测 |
| P4.3 | 精确组件作者路径、共享/独立实例编辑、生成批次显式保护、真实像素/持久化 | 组件内部目前通过作者属性编辑；不是逐生成实例网格写回 |
| P4.4 | 固定当前值、曲线局部时间插帧、双语言 UI、实际 GPU 和 Windows 部署 | 不含手柄自动录制；先固定值再拖动，或显式数值插帧 |
| P4.5 | 待实施的大图定位和输出域检查 | 退出作品、完整创作流程与两端验收 |

Windows 已包含 P4.4 增量；Android 当前仍是 `31d43eb` 阶段的共享运行版本，
本次只增加编辑器操作；共享编辑合同另外通过手机原生测试。详见文末证据。

## 已实现的基础

`geometry2d` 负责项目值类型的仿射组合、逆变换、枢轴保持、画布和视口映射；
不依赖 UI、renderer 或原生类型。内部复用已验证 vcpkg GLM 0.9.9.8#2，来源与
许可见 `provenance/view_manipulation.json` 和既有 `provenance/glm.json`。
现有 `texture.affine` 渲染已改用共享计算，保留已有端口/属性及边界语义。

坐标 +Y 向下、顺时针角度；平移/枢轴是作者画布比例，旋转在像素空间，保留
非方形画布含义。父变换乘在左侧；有符号/非均匀缩放可逆时支持反变换，奇异
变换明确返回无逆。更换枢轴补偿平移，保持同一图像。屏幕命中先排除留黑区域，
捕获后的拖动允许离开图像，DPI 由调用方提供的同一显示坐标矩形体现。

Windows `out/p4-geometry2d-tests.log`、
`out/p4-shared-affine-runtime-tests.log`（geometry2d、原 affine 几何合同、实际
三场 UI/像素及边界检查）通过。Android 原生
`out/geometry2d_tests-presentation-android.log`、
`out/p4-shared-affine-runtime-android-tests.log` 通过。此基础尚未作为新的 Studio
手柄交付，Android 当前已安装 APK 仍是 P3 收口版本。

## 接续实施

### P4.1 编辑事务与输出视图（进行中）

`editor_application::CanvasEdit` 已通过 Windows 和 USB Android 原生检查：
四种变换、镜像/非均匀父变换、像素/角度/比例吸附、枢轴保持、单次撤销、
过期 revision 和连接/绑定保护。证据为 `out/p4-canvas-edit-tests-retry.log`、
`out/p4-canvas-edit-android-tests.log`。Android 原生检查是共享事务测试，
不代表手机增加编辑器或新的 APK 已交付。

Studio 新增独立 `OutputCanvas`：在最终输出开启画布编辑，选中作者的
`texture.affine` 节点后操作移动、旋转、缩放和枢轴；吸附为 16 像素、15 度、
0.1 倍。输出手柄不使用节点图的平移操作。Esc、失焦、切换选择、切入其他编辑、
隐藏输出或外部 revision 变化会取消草稿；一次拖动最多写入一次 History。
首个拖动必须对应当前成功编译的输出，已有拖动允许异步更新中间预览。
保存/发布前取消未完成的捕获，不把鼠标仍按下的草稿静默写入工程。

支持根图 affine 经唯一 affine/composite/output 路径的作者变换。选择使用
作者画布矩形，不是逐像素 alpha 拾取；一般合成遮挡并未作为可见性证明。
非坐标保持后处理、多个可见分支、组件内部及被驱动的父变换明确拒绝，后续
按 P4.3/4.4 补语义，不能把这一小范围称为全部 2D/3D 直接编辑已完成。

中英文 `output_canvas` 检查实际注入 ImGui 鼠标输入，覆盖四种手柄、取消、旧输出
禁止起拖、保存/重开属性及发布 program 一致性：`out/p4-output-canvas-tests-wire.log`
通过。此检查没有 OS 窗口/GPU 像素，完整 Studio 图选择/拖动/像素检查仍在接入。
测试校准记录：鼠标坐标由 ImGui 取整，输入映射允许一个屏幕像素的误差；数学
测试仍使用严格容差。持久化会保留原始 protobuf 记录为扩展数据，比较稳定编码
及变换属性，不用包含该缓存的内存 Document 与新建空扩展 Document 直接相等。

完整 Studio `output_canvas_gpu` 已通过（`out/p4-output-canvas-gpu-tests-rect.log`）：
从实际节点图选中 affine，鼠标拖动最终输出，观察 requested/installed 代次，
保存/发布、撤销、重新打开，并核对移动前后左右两块 7×7 像素区域。
证据目录 `out/windows-release/output-canvas-gpu/01d1859954114ad5a04e0bae43110561/`
含截图、实际 Image 矩形和工作流记录。首次 GPU 测试的取框辅助函数错误地把原点
并入包围盒，导致测试鼠标点错位置；修正为从首个图像顶点初始化后通过，失败
记录仍保留。该问题没有通过忽略“未产生新编译”来放过。

Windows Studio 已完成交付构建 `out/p4-output-canvas-delivery.log`，Python 自动部署
可执行文件、DLL 与资源，并通过强制四项模板应用/保存/重开/发布回归。
路径：`out/windows-release/src/windows_spike/deploy/rhythm_master.exe`。
P4.1 的根图静态 affine 工作流可验收；P4.2–P4.5 继续推进，不把受保护的驱动
参数或暂不支持的组件/3D 路径算作已完成。

Android 同步交付共享 affine 渲染基础，覆盖安装 APK SHA256
`54bbcaa8755212b8d3b051ece61d17beed171ddb899efac07b84a9e0137c6cc4`。
`out/p4-shared-affine-android-delivery.log` 包含主机内容重建/哈希检查，
`out/p4-shared-affine-android-program.log` 通过实际内置目录连续节目，证据目录
`out/android-continuous-program/6f3b4a581a6e487a8eb2f70ebe1b99b1/`。
三场的方向为 1280×720 / 720×1280 / 1280×720，队列从 3 个耗尽，横竖切换和
后台恢复通过，保存的节目单不变。此项没有声学回录，不替代最终长稳/听感验收。

### P4.2 ImGuizmo 私有适配器（已验证，场景编辑仍在接入）

ImGuizmo 1.10 的安装二进制与现有 docking ImGui ABI 不兼容；精确测量与源码
例外见 [技术栈记录](technology_stack_evaluation.md) 和 `provenance/imguizmo.json`。
`tools/prepare-imguizmo.py` 只从 SHA512 固定的 vcpkg 下载包提取两份未修改源码及
MIT 许可，固定上游提交 `b796ac3b861afc6e91ca74e4611effd9c9527367`。未升级现有
ImGui、未导入另一份 ImGui、没有把 SDK 安装成功当作 ABI 兼容证明。

`studio::Gizmo` 以项目矩阵、相机、视口和稳定身份接入；原生/第三方类型只在
私有同步 UI 边界。世界到局部写回参考已有 Godot Node3D 的逆父矩阵语义，实际
运算复用项目 GLM 适配器。上游 SetRect 的 YMax 使用了 XMax，故适配器另行限制
初始命中，不允许留黑/视口外开始捕获；捕获后可拖出视口。Esc、身份/模式变化、
失焦和错误取消捕获。

`out/p4-gizmo-tests-axis-contract.log`、`out/p4-gizmo-tests-cancel-errors.log`
通过真实 ImGui 鼠标队列测试：透视/正交、旋转且非均匀父变换下的屏幕移动及
局部矩阵写回、局部/世界旋转、局部缩放、取消和视口边界。此项仍没有完整 Studio
3D 对象选择/保存/像素证据，不能宣称 P4.2 完成。

上游缩放始终使用局部轴，不支持世界轴缩放，适配器显式返回
`gizmo.world_scale_unsupported`，后续 UI 必须明确提示，不能显示“世界缩放”却
静默沿局部轴操作。带剪切的矩阵也不能直接等同现有 TRS 作者参数，接续事务需要
可表示性验证，不把程序生成的中间网格写回作者资产。

### P4.2 共享 3D 作者事务（已验证，UI 接入中）

`scene::EulerPose` 将平移 × Rz × Ry × Rx × 缩放作为唯一计算约定，内部复用已安装
GLM `gtx/euler_angles.hpp` 的组合/提取，仍使用 `provenance/glm.json` 的 MIT 分支
与完整许可。`Runtime scene.transform` 已采用同一组合函数；没有新 schema 或 ABI。
分解包含镜像和万向节锁测试，剪切/奇异矩阵拒绝，不把近似分解静默写回作者参数。

`SceneEdit` 保存稳定作者 ID、基线 revision、相机和唯一输出路径，四种方面分别
处理：有驱动则保护；非 TRS 或超范围则拒绝；多次移动保留一个草稿；完成后只产生
一个 History。平移不改写可等价表达的原始欧拉角分支，保留已被运行时夹限的原有
轴因子。当前根图 `scene.transform -> transform/merge -> scene.render -> output`
可追踪；组件、复制分支和图像后处理暂明确拒绝，接续 P4.3/4.5 补语义。

Windows `out/p4-scene-pose-tests.log`、`out/p4-shared-scene-pose-tests.log`
（含实际场景 GPU 像素）、`out/p4-scene-edit-tests.log` 通过；USB Android 原生
`out/p4-scene-pose-android-tests.log`、`out/p4-shared-scene-pose-android-tests.log`、
`out/p4-scene-edit-android-tests.log` 通过。此处新增的 3D 事务还未出现在已交付
Studio 窗口，最新已安装 APK 仍是前文 `54bb...` 的 P4.1 交付版本。

### P4.2 Studio 手柄交付（对象拾取继续推进）

后续 `SceneCanvas` 已接到最终输出：根图直接 scene.render 输出自动切换为 3D
编辑视图，在节点图选择 scene.transform 后可拖动。支持移动、旋转、局部缩放、
世界轴切换和 0.1 单位/15 度吸附；世界缩放置灰并说明原因。另一面板草稿、旧输出、
失焦、Esc、选择/revision/模式变化和视口尺寸改变会取消，失败结果不提交。
同次补上 2D 输出视口尺寸变化时取消拖动，避免窗口布局变化导致图形跳动。

`out/p4-scene-canvas-tests.log` 同时通过实际 Studio 的 2D 和 3D 图选择、鼠标拖动、
新输出像素、保存/发布、撤销和重开；3D 证据在
`out/windows-release/scene-canvas-gpu/11053232b705438d89d4a9cc1a27fe53/`。
`out/p4-output-resize-tests.log` 验证中英文 2D 捕获后调整窗口尺寸不产生提交。
Windows `out/p4-scene-canvas-delivery.log` 已自动部署 20 DLL、程序和资源，强制
四项模板应用回归通过；交付路径仍为前文的 Studio deploy 目录。

这里还不是 P4.2 全部完成：从视图点击对象来拾取、P4.3 实例/组件身份与编辑
范围、P4.4 自动化策略、P4.5 不同输出域及大图定位仍继续推进。当前不支持的
scene route/图像后处理明确提示，不能当作与竞品全部直接编辑能力等价。

1. `editor_application` 增加变换编辑事务，保留基线 revision/稳定节点 ID；一次
   鼠标拖动只提交一次 History，Esc/失焦/删除或外部 revision 变化取消草稿。
2. Studio 最终输出的坐标操作与节点图平移分开；先支持 affine 作者节点的
   移动/旋转/缩放/枢轴、吸附，明确可追踪的下游坐标映射与不支持情况。
3. 自动化/连接驱动的属性显示来源，不能默默覆盖。先保护绑定，再提供显式
   常量修改/关键帧策略（P4.4）。运行中间结果不能当作作者资产直接写回。
4. 验证 vcpkg ImGuizmo 与当前 ImGui 版本；P4.2 按 Godot 参考计划实现 3D
   选择及手柄，透视/正交/局部/世界轴和父变换一起测试。
5. P4.3/4.5 补组件/实例编辑和大图定位；完成空图 2D 音乐构图、3D 音乐雕塑，
   保存重开/发布/Android 输出验收。手机不增加节点编辑器。


### P4.2 几何拾取增量（2026-09-09，UI 验证进行中）

共享场景实例新增作者来源：producer、最近的 transform、点 element 和 generation。
几何上传身份不作为作者身份；外层组变换保留最近的对象变换，合并和点阵保留来源。
Windows `out/p4-instance-origin-tests.log` 与手机原生
`out/p4-instance-origin-android-tests.log` 已通过。

`scene3d/picking` 直接复用已安装 GLM 的射线三角形求交和矩阵逆；Godot Camera3D
为坐标参考。记录见 `provenance/view_manipulation.json`。相机生成近远裁剪段，
使用当前场景实例及模型节点姿态求交，局部射线不归一化，保证非均匀缩放下距离
可比较。背面规则与后端的镜像绕序修正一致。共享网格包围盒只在单次点击内建立；
默认最多 16,384 实例、65,536 节点/引用工作、1,000,000 顶点、250,000 三角形。
超预算或无法检查的对象使整个拾取失败，不返回未证实的部分最近结果。

`out/p4-scene-picking-tests.log` 与手机原生
`out/p4-scene-picking-android-tests.log` 通过：裁剪、透视/正交、最近对象、隐藏、
镜像/非均匀缩放、身份/预算，以及 8,192 个共享实例只进入 12 个三角形精测。
此处是几何选择，不是逐片元对象 ID：不评估纹理 alpha/遮罩；当前 GPU twist/taper、
骨骼和 morph 场景明确拒绝，不用未变形网格猜测。组件内部作者映射继续在 P4.3 补齐。

前一 3D 作者事务版本已覆盖安装 APK SHA256
`3004179bf4feee695fcb1370440c58a58aa617005d8fe6aa7d494b781af2aea5`。
`out/p4-scene-pose-android-program.log` 的实际内置横/竖/横节目单通过；证据目录
`out/android-continuous-program/9e9d10d256414a7f8d2af0daf3dcf858/`。
这份 APK 是共享 Euler 渲染交付，不包含手机编辑器，也不是本次拾取 UI 的证据。


实际 Studio 拾取已通过 `out/p4-scene-selection-tests.log` 中的 `scene_canvas_gpu`：
从最终输出点击立方体选中 transform，随后用手柄移动，校验新计划代次、画面像素、
保存/发布、撤销/重开。证据目录
`out/windows-release/scene-canvas-gpu/ee26acecd014446aae38e9ba31251b09/`。
两语言 UI 回归 `out/p4-scene-selection-fixture-tests.log` 通过，补充旧输出不可拾取、
点击选择不提交文档、使用当前帧相机/实例、未知内部 ID 不误映射到根节点。
失败记录保留：首次新增测试漏链接 graph_runtime；随后 fixture 跨工程 ID 提交被
History 正确拒绝，测试改为同工程替换图，未削弱产品校验。

Windows Studio/Player 已完成本次 Python 自动部署，`out/p4-scene-selection-delivery.log`
含强制四项模板应用回归全通过；Studio deploy 路径同上。当前依然只有 Windows
Studio 提供视图编辑，手机共享模块原生测试通过，不把这些测试说成 Android 编辑 UI。


### P4.3 作者身份和编辑范围（进行中）

组件展开现在返回每个可执行 ID 的 `AuthorNode {instance_path, node}` 元数据，
包括保留为根实例 ID 的嵌套输出；它不能仅凭“ID 在根图存在”就当根作者节点。
元数据随确定性的同一次展开建立，作用域预览和预留生成 ID 不改变作者路径。
不修改运行包 ABI，不把中间结果写回资产。Windows
`out/p4-component-authors-tests.log` 与手机原生
`out/p4-component-authors-android-tests.log` 已验证嵌套、同源多实例和输出别名。
后续将此映射接入选择/定位，并复用现有 `DetachComponent` 的完整嵌套副本事务
提供独立实例编辑入口；目前还不把上述元数据测试算成范围 UI 已交付。


作者映射已改为随后台 `CompilerWorker` 的同一次展开生成，组件预览请求开关不会
改变它。`PreviewRouting` 只在对应计划/资源 Commit 时发布映射；点击不重编译或
重展开组件。`out/p4-author-compilation-tests.log`、
`out/p4-author-compilation-android-tests.log` 通过。

P4.3 的视图范围入口已接入：组件对象提供“编辑共享定义”和“将此实例独立”，
前者定位精确的嵌套作者节点，后者复用 `DetachComponent` 的完整嵌套副本事务后
定位相同作者路径。源工程、其他实例和原定义不变。点阵/同作者多个对象在点击
后必须显式勾选整个批次编辑才启用手柄；不是单点网格写回，也不宣称已有逐生成
实例持久化覆盖层。命中保留的是拾取时的 element/generation，编辑目标仍是作者。

`out/p4-scene-scope-ui-tests.log` 六项通过：双语言拾取/范围选择、批次默认保护和
显式启用/Esc、映射随计划切换、嵌套作者定位/失效路径保护/独立副本，及根图 3D
实际 GPU 拖动回归。完整 Studio `scene_scope_gpu` 通过
`out/p4-scene-scope-gpu-tests.log`：点击左侧组件实例 → 独立 → 内部属性输入
translate_x → 应用 → 新画面 → 保存/发布 → 撤销 → 重开。像素同时检查被编辑实例
离开的区域、移入的区域和另一个保持原位的共享实例；保存内容检查原定义仍为
-0.4，独立定义为 -0.65，发布 program 与保存图一致。证据目录
`out/windows-release/scene-scope-gpu/4cc998ef85e34d8193b3052b6c1b2a8c/`。

P4.4 自动化直接编辑策略、P4.5 输出域聚焦及大图定位继续推进；组件内部目前使用
作者节点属性编辑，尚未把组件坐标路径全部接到 2D/3D 视图手柄。

本增量已交付 Windows Studio deploy：`out/p4-scene-scope-delivery-retry.log`
四项模板回归通过。首次链接因正在运行的本项目 exe 占用而失败，按既有授权
核对路径并结束该进程后完成增量构建；失败日志保留。
Android 构建与宿主内容验证：`out/p4-scene-scope-android-delivery.log`；覆盖安装
`out/p4-scene-scope-android-install.log` 成功，APK SHA256
`f96e225807a93435f0e9256dc922abe75475565f4f695d35c438129adec85c0c`。实际内置横/竖/横节目单、
后台恢复、队列消费和保存列表不变检查通过：`out/p4-scene-scope-android-program.log`，
证据 `out/android-continuous-program/386d90c2280f4c53aaf80053467f0eac/`。
手机验证的是共享运行与原有 Player，不含移动编辑器或声学回录。


### P4.4 参数来源和显式编辑（进行中）

复用现有 Registry/ResolveEdges、History 和已获验证的 `parameters::Curve`，
没有第二套表达式或曲线求值器。Curve 的既有 TiXL 参考和 MIT 记录继续见
`provenance/curves.json`。新增编辑层只处理当前帧观测值、目标输入和作者事务。

“保留当前值并断开驱动”只移除所选 transform 的数值输入和命名绑定，按该算子的
范围固定实际观测值，保留生产者、信号定义及其他消费者。曲线操作显式修改共享
源的关键帧，使用 Curve 输入端的观测时间；插入/替换键保留其他键、已有切线和
连线。不尝试反解表达式、录制任意音频源或偷偷覆盖绑定。当前仍先固定变换再用
画布手柄；曲线入口是明确的数值插帧，不宣称已实现拖动手柄自动录制关键帧。

`out/p4-transform-drivers-tests.log` 与手机原生
`out/p4-transform-drivers-android-tests.log` 通过：命名/普通连接、按算子限幅、
其他消费者不变、局部时间、插入/替换键、键数预算、旧 revision 和无效值保护。
`out/p4-transform-automation-ui-tests.log` 双语言及边界检查通过：实际数字输入、
记录键、定位来源、旧输出禁用、固定值、撤销、保存重开和运行包曲线一致性。
驱动列表放在限高滚动区域，结构说明按 revision/选择缓存，只在展开时读取所需
来源值；当前已接受计划仍是启用操作的前提。

`out/p4-transform-automation-gpu-tests.log` 两项通过：实际 Studio 从连接驱动的
affine 固定当前值，确认新计划和画面位置保持，再鼠标拖动、保存/发布、撤销和
重开；同时运行根图 3D 拖动回归。证据目录
`out/windows-release/transform-automation-gpu/2116bd39ad32489ab687fc8b849360df/`。
曲线数值插帧由上述双语言实际输入和包数据检查覆盖，不用常量 GPU 用例冒充
曲线画面或音频响应测试。

Windows Python 增量构建和完整 deploy 已完成：
`out/p4-transform-automation-delivery.log`，四项强制模板应用回归全通过。
验收入口 `out/windows-release/src/windows_spike/deploy/rhythm_master.exe`。
