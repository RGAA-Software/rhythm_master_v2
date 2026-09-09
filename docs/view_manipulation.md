# P4：视图直接编辑实施记录

P3 已收口，当前推进 P4.1；此文不把坐标基础等同于已完成编辑手柄。

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
