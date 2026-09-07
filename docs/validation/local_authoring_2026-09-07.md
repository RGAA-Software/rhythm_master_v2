# 本地二维合成与表达式增量

通信保持暂停。本记录对应主线 1–3 的可用增量，不代表七项任务全部完成。

## 二维画面

- `texture.affine`：连线控制统一缩放、顺时针角度、画布比例位移和不透明度；
  独立水平/垂直缩放支持镜像，可调整轴心。连线覆盖常量，数值按参数范围限幅。
- `texture.shape`：矩形、椭圆、圆环和正多边形，透明底图，合并绘制。
- `texture.mask`：Alpha 遮罩和反向 Alpha 遮罩；不是亮度遮罩。
- `texture.composite`：保留透明度的正常叠加与加色叠加，强度支持标量端口。
- GPU 内部改用预乘 Alpha。公共上传和顶点颜色仍为 straight alpha，适配器转换；
  上传、顶点着色、清屏及多遍采样共享一致约定，修正半透明图层重复变暗。
- 原 `texture.transform` / `texture.blend` 的包结构与既有行为保留。

几何测试比较四种形状的独立解析面积、索引和边界；二维变换验证 90 度旋转、
镜像、轴心和连接覆盖。USB GLES 检查完整 16×16 像素：半透明红图多遍叠加绿底、
变换后覆盖区域、零缩放清除、正反遮罩和加色/正常混合。两次设备生命周期重复通过。
Windows 42 组、Android 34 组通过；这是前一形状增量的完整结果。

## 安全表达式

`scalar.expression` 使用不可变、有界的后缀指令程序。变量 `a/b/c/time` 来自
图端口或常量；`time` 不偷偷访问全局时钟。支持四则运算、余数、括号、pi/tau 和
sin/cos/abs/floor/ceil/sqrt/min/max/clamp/mix，不提供脚本、文件、网络或任意函数入口。
源码最多 1024 字节，嵌套最多 32 层，程序最多 256 条指令；运行时栈固定大小。
每步限制到 ±1e12；除零、零模数和负数平方根返回零；clamp 自动排列上下界，
mix 的比例限制到 [0,1]。浮点三角函数以数值容差验证，不承诺跨平台逐位相同。

属性编辑提交前编译校验；无效草稿留在输入框，图继续使用上次有效公式。
有效编辑进入既有撤销/重做、保存、发布路径。连线控制的常量字段禁用并提示来源。
Protobuf 新增表达式属性字段，载入前限制字符串长度；发布包中的新算子由能力清单识别。
旧图、旧包测试继续通过。

验证包括运算优先级、纯函数、非法语法和名称、深度/文本/指令上限、异常数值、
运行时端口覆盖与缓存、程序序列化及 Windows 发布模板在共享运行核心的执行。
当前完整 Windows 43 组、Android 35 组 native 测试通过。
真机目录 `/data/local/tmp/rhythm-master-phase-a-20260907012356128`。

证据：`out/alpha-build-windows.log`、`out/alpha-android-device.log`、
`out/affine-build-windows.log`、`out/affine-android-device.log`、
`out/shapes-build-windows.log`、`out/shapes-android-device.log`、
`out/expression-build-windows.log`、`out/expression-android-device.log`。
表达式运行时附加断言随后在 Windows 定向 scalar_contracts 与 Android 全组通过。

内容现在为 37 预设、10 模板。完整参数模式、命名绑定、时间线与可复用组件仍需后续开发；
图片/视频/音乐文件的 FFmpeg 集成和真实 APK 安装验收仍未完成。

## 时间线基础面板

工具栏“时间线”可打开可停靠面板。支持播放/暂停/重新开始、秒/帧/拍显示、
预览范围和无状态图循环定位；帧模式定位按帧取整。预览参数只保存在当前编辑会话。
面板列出 `scalar.curve` 轨道，复用已有关键帧编辑器；曲线通过现有图持久化和发布。
曲线键仍使用其输入时钟的秒数，局部时钟与项目时钟的含义在 UI 中明确区分。
编辑草稿预览、提交、撤销复用同一历史；切轨道或关闭面板会结束已有事务。

共享 FrameClock 增加定位；连续循环保留每帧增量，不漏掉边界之后的一帧时间。
Studio 暂停同时冻结外部输入快照和反馈历史写入。包含反馈的图禁用任意定位与循环，
避免把清空历史冒充正确的历史重放；暂停与从头开始仍可用。

Windows 44 组测试、Android 35 组 native 测试通过。新增 ImGui 鼠标交互检查播放/
暂停、连续循环、关键帧一次提交及撤销；共享运行时检查定位和反馈暂停/恢复。
证据：`out/timeline-final-windows.log`、`out/timeline-android-device.log`。
真机目录 `/data/local/tmp/rhythm-master-phase-a-20260907013531574`。
此面板尚不包括完整多轨道图形拖拽、切线编辑、事件轨道或组件时间作用域。

## 颜色调整

`texture.color_adjust` 支持曝光、对比度、饱和度和反相，四项均可接标量端口。
颜色滤镜在 GPU 采样处处理，保留 Alpha；无每帧 CPU 读回。先按 Rec.709 权重
计算亮度并调整饱和度，再按 0.5 中点调整对比度、乘曝光增益、反相与限幅。
这是当前 RGBA 数值空间的效果运算，不等同于已完成线性/HDR 色彩管理。

自有 shader 已验证 D3D11/GLES profile；Python 工具负责编译和嵌入，CMake 只声明
依赖并调用。现用只读 shaderc 1.19.157，SHA-256
`6f310cf7091937aab1f1dc0b4bdafcc5c317bc789d0fc2b82f46a28e29f143b5`。
当前开发构建启用已测 profile；该工具完整来源/可重建发行仍待补齐，不能据此冻结
最终发行工具链。编译器、源文件、varying/include 和产物哈希在
`out/{windows,android-arm64}/generated/render/color_adjust_shader.json`。

纹理绘制构造已移入 `runtime/texture_ops`，缓存/求值器继续拥有执行状态和 GPU 生命周期。
Windows 45 组与 USB Android 36 组 native 测试通过。GLES 使用固定像素期望检查原色、
曝光、灰度、零对比度、反相与半透明叠加；Windows 独立 Player 完成“呼吸光卡”30 帧
D3D11 播放，Android 完成同一 Windows 发布包 60 帧 GLES 播放。
内容达到 41 预设、11 模板，仍未达到最终库目标。

证据：`out/color-build-windows.log`、`out/color-android-device.log`、
`out/color-player-windows.log`、`out/color-package-android.log`。
真机目录 `/data/local/tmp/rhythm-master-phase-a-20260907015156087`。
