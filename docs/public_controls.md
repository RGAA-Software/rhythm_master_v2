# 公开宏控件与参数快照

R5 功能增量。`control.scalar` 是作品主图中的标量来源，可连接多个节点、命名信号
或组件输入；不绑定 UI 对象地址，不向图或运行时引入平台类型。控件保留在主图，
组件内部使用已有标量输入/公开参数；把宏包进组件会明确拒绝。

## 创作与演出

- 在数值分类添加“公开宏控件”，设置 value、control_minimum、control_maximum。
  右侧“演出控件与快照”显示全部主图宏；展开命名区修改名称，回车保存。
- 滑块拖动提供实时预览，释放提交一次，可撤销/重做。快照保存全部宏的当前值；
  召回 A、A/B 混合和删除 A 都通过原有工程历史事务，不旁路保存系统。
- Windows Player 显示发布作品中参与最终效果的控件。Android 在现有 UI 的
  “演出控件”中显示相同控件、快照与混合滑块，无须目录选择。
- 播放暂停时仍可调宏，音乐特征、参与者输入和播放时间继续保持暂停时的值。
  暂停调参是重新求值，不推进动画。表面重建保留宿主持有的控件值；换作品恢复
  新作品默认值。Android 用作品代次拒绝旧对话框操作。

## 约定与限制

一个作品最多 64 个宏、64 套快照。名称 1–128 UTF-8 字节，不含控制字符。
范围必须满足 -1e6 ≤ minimum < maximum ≤ 1e6；默认值和快照值必须在范围内。
运行时不悄悄截断非法输入，验证失败原子拒绝。修改范围使现有快照越界时需恢复
范围或修正快照；不能把未通过验证的编辑图发布。

快照保存公开宏的数值，不捕获文件、GPU 状态或任意节点对象。缺少新加入宏的旧
快照使用该宏的当前默认值。删除宏同步清理所有快照引用；最后一个宏删除后清空
快照。发布时裁剪未接入最终输出的宏及其快照条目，编辑源工程保留它们。

手动混合是纯数值线性插值，进度范围 [0,1]；控件模型没有第二套时钟。
定时快照过渡已由 [Cue 编排](cue_arrangement.md) 接入同一播放时间；跨作品资源准备
与双场景合成仍属于 R5 后续任务。

## 保存与平台边界

GraphProject 字段 11、CompiledProgram 字段 7 保存 ControlMetadata；控件范围/
默认值从 control.scalar 节点读取，元数据只存显示名称及快照。
现有 schema 2/3/4 和 runtime ABI 2 的无 Cue 工程继续有效；新增可选字段不改变
旧字段语义。旧运行器不认识 control.scalar，必须以未知操作符拒绝这类新运行包，
不能忽略控件后错误播放。有 Cue 的新工程使用 schema 5 / ABI 3，旧播放器须拒绝。
前置 wire 检查限制集合/名称大小，编辑工程保留未知字段。

参数模块持有不可变 ControlBank。graph 负责作者数据验证与发布裁剪；project_io
负责格式；runtime 接收值快照；Player 负责暂停求值；桌面 control_ui 与 Android
JNI/Java 是私有宿主适配器。Android JSON 仅用于进程内 UI 描述，未添加网络通信。

## 可编辑作品与验证

“晶瓣合唱”现为 86 节点、115 连线，音乐响应、环绕速度、曝光、辉光四个宏连接
原有表达式、显示与合成节点，含 Quiet/Concert/Peak 三套快照。默认值保留原画面。
工具 `tools/author-crystal-choir.py` 生成可编辑源；该扩展不计作新的独立模板。

Windows 参数、图、格式、Player 暂停/缓存/表面恢复、鼠标滑块/命名捕获测试和
两款程序 deploy/smoke 已通过。Android 四组原生测试及 Redmi K40S UI 实机检查通过，
APK 已覆盖安装、保留所选作品和数据。暂停于 11.488 秒修改曝光得到不同画面但
时间不变；Quiet/Peak 50% 混合得到 [1.175, 5.5, 0.7, 0.4]，召回 Quiet 的辉光
恢复为 0.15。旧作品无宏时显示说明，重新选择内置作品读取新增宏。

晶瓣合唱真实 PCM 对比的平均 RGB 差异（0–255）：演示/静音 1.75717、低频/静音
2.07796、高频/静音 5.16258、低频/高频 6.79796；与新增宏前默认画面一致。
新缩略图已从实际 D3D11 渲染更新。MP4 检查通过，不代表长稳或视觉品质验收。

日志：`out/r5-controls-final-tests.log`（9 项）、
`out/r5-controls-final-ui-gpu-tests.log`（历史/音乐/导出含 PCM fixture）、
`out/r5-controls-android-native-tests.log`。实机截图为
`out/r5-controls-android-paused-before.png`、`out/r5-controls-android-paused-after.png`、
`out/r5-controls-android-mixed-values.png`、`out/r5-controls-android-recall-a.png`。

参考 TiXL Variation 的快照预览和单次撤销提交设计，见
`provenance/public_controls.json`；未导入其引擎、全局 UI 状态或通信实现。
