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
