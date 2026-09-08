# R4 受约束图像 Shader 创作

2026-09-08 功能增量：Studio 源码编辑、异步双目标编译、失败诊断、成功替换、
撤销/重做、保存/重开、运行包、节点预览、音乐参数、MP4 和 USB Android 播放。
这是图像表达式 profile；材质 Shader、用户 compute、骨架/morph/动画混合仍未由此交付。

## 使用与语义

在节点菜单中添加 `texture.shader`，或打开“相位织光”并选中其中一个图像 Shader。
右侧可编辑源码，点击“编译并应用（Windows + Android）”。两个目标均成功后生成
不可变资产并通过普通编辑历史替换节点绑定；编译期间及编译失败时继续使用旧资产。
参数拖动中的事务先完成，编译结果再应用，不覆盖进行中的参数编辑。
未连接到输出的节点也独立异步加载源码；重新编译后，无其他引用的旧版本从当前发布
资产清单移除，历史快照与不可变文件仍支持撤销，不因持续编辑累积占满包预算。

| 输入 | 语义 |
| --- | --- |
| source | 可选纹理；未连接时为白色，`Sample(uv)` 返回非预乘 RGBA |
| time | 可选标量；未连接时使用作品统一秒数，暂停/跳转与作品一致；限制 0–1000000 秒 |
| a/b/c/d | 可选标量；支持音频节点和已有参数绑定，未连接时用属性值；有限值限制 ±1000000 |
| uv / resolution | 左上原点归一化 UV / 当前输出像素尺寸；内联预览使用自己的尺寸 |

源码是一个返回非预乘 RGBA 的表达式，可以使用 `vec2/3/4`、`Sample`、常见数学函数、
条件表达式与分量选择；不是完整 GLSL 文件。示例：

```glsl
mix(Sample(uv), vec4(uv.x, uv.y, sin(time) * 0.5 + 0.5, 1.0), clamp(a, 0.0, 1.0))
```

包装器负责预乘输出 Alpha；Alpha 限制 0–1，RGB 限制 Float16 有限范围，NaN 归零。
不隐式转换颜色空间；沿用显式 linearize/display 节点和纹理精度选择。
GLES 包装器使用高精度浮点，实际像素检查包含 100000 秒。

源码上限 8192 字节、1024 词元、32 层括号和 16 次采样；拒绝预处理、注释、声明、
赋值、循环、用户函数及非 ASCII 源码。词法限制由私有 vcpkg stb C lexer 适配器处理，
语法和类型诊断由现有 bgfx shaderc 产生。界面显示词法位置及原始编译器诊断。

## 资源、线程与兼容

- `image_shader` 持有表达式、profile 与二进制容器校验；没有 UI/native 类型。
- `shader_authoring` 使用已有 foundation 有界执行器，一个工作任务串行编译两个目标。
  私有 SDL 进程适配器隐藏 Windows 控制台，负责取消、30 秒单目标限时、日志上限和回收。
  子进程无可用退出码时，必须取得独立临时目录中的完整、有效产物才允许发布。
- `prepared_assets` 在工作线程校验 SHA、MIME、源表达式和双目标容器，发布不可变资源。
  Runtime 以内容 SHA 缓存 RAII 程序；同一资产跨节点共享，参数变化不重新创建程序。
- Renderer 公共接口仅暴露稳定 `ImageProgramHandle` 和有限参数；本机对象留在 bgfx 适配器。
  Null 后端只验证资源契约，不执行或声称验证本机字节码。
- 单目标产物最多 512 KiB，设备最多 64 个程序、16 MiB 产物字节；该计数不是驱动显存测量。
  原有运行包普通资产总计 8 MiB 限制仍生效，不能因 Shader 自身预算更大而突破运行包限制。
- MIME 为 `application/x-rhythm-image-shader`；`RMSH001` 包含源码、编译器 SHA、Windows
  `s_5_0` 与 Android `300_es` 两份产物。固定 FSH12、四边形输入签名、一个采样器、两个
  vec4 uniform，拒绝未知绑定/版本/截断/尾部数据。图和运行 ABI 沿用 2，旧 Player
  按原有算子能力检查明确拒绝未知 `texture.shader`，旧作品继续可读。
- 容器/绑定校验不是证明本机字节码与所声明源码相同，也不是任意 GPU 程序的安全证明。
  内容资产由作者编译流程产生；最终执行仍由对应驱动负责。

模板编译工具现在验证并复制 manifest 引用的不可变资产；CMake 跟踪资产文件依赖。
Windows Python 部署在 exe 同级及 `deploy/shader_tools` 同时携带编译器、依赖 DLL、
包含文件和来源记录。Player 包含目标产物，不需要安装 Python 或编译器。

## 复用与依赖状态

直接使用已安装 stb C lexer，MIT 选项，保留双许可文本。Windows/Android 安装修订不同，
逐项比较发现旧 Windows 版本的 identifier `string_len` 缺陷；适配器使用其有界 NUL
结尾存储，不修改或升级共享 vcpkg。[精确记录](../provenance/stb_lexer.json)。

现有 bgfx/bx UI 顶点、shaderc 包装、SDL 进程与第一方有界执行器被复用。
已检查 vcpkg 的 bgfx tools port；当前 triplet 没有已安装 shaderc，port 与当前 bgfx
容器兼容性尚未验证。本增量保留实测可用的 1.19.157 工具，读取旧仓库已生成文件，
不修改旧仓库、不升级 vcpkg。它作为独立工具随本地 Studio 验收目录携带。
[工具记录](../provenance/shaderc_host.json) 固定二进制 SHA、旧仓库提交、709 个编译源文件
和构建工程哈希；保留 glslang、SPIR-V、Dawn/Tint、bx 等实际许可和文件版权，包括原始
文本中的例外。尚未在本项目重建编译器，不声称二进制可复现；这仍是独立构建来源待办，
不据此选择项目对外许可证或把未验证的平台编译能力标为完成。

“相位织光”参考 TiXL `RadialGradient.hlsl` 的中心 UV、宽高比和径向距离，
干涉/调色组合为第一方编排，见 [作品来源记录](../provenance/phase_loom.json)。

## 验证与交付

- 源限制/双目标容器、截断/错误签名/绑定/profile；Windows/GLES 实际像素对照。
- 程序所有权、错误设备/失效句柄、数量/字节预算、设备失效与释放。
- Runtime 音乐/时间 uniform、Float16、共享缓存、暂停、资产替换、缺资源与清理。
- 原生双目标编译、取消、错误诊断、失败保留旧文件、保存/重开、发布/资源准备。
- 中英文 UI 实际点击和文字输入、源码替换、撤销/重做。
- 23 节点、28 条连线、一个共享 Shader 资产的“相位织光”，真实音乐/静音/低/高频
  图像对照与 MP4 检查通过。RGB 0–255 平均差异分别为 31.4833、35.3395、11.9487、35.2807。
- Android Adreno 650 / API 34 覆盖安装，内置列表直接选择；实际音乐截图
  `out/r4-shader-android-music.png` 显示 12.48/16.00 秒、RMS 0.0181001。
  运行包 SHA `21beb1c43b52ef93c8569e0cb8de54cb0fd1e77cc628cf2cfea4a2623872c865`
  与 Windows 一致；APK SHA `371115ca26f3988a40f259f8be57cc98745121a1655e06e066ce339ca0a825d7`。

部署工具独立调用也通过同一编译/诊断/保存/发布检查，见 `out/r4-shader-deployed-tool-tests.log`。
主程序通过系统 PATH、无关工作目录、20 个本地 DLL 的部署检查；早期脚本仍期望旧的
5 个预览计数，已更新为实际已有的 5 个纹理与 3 个信号预览，并重新通过。

主要日志：`out/r4-shader-authoring-tests.log`、`out/r4-shader-ui-tests.log`、
`out/r4-shader-runtime-android-tests.log`、`out/r4-shader-work-tests.log`、
`out/r4-image-program-gpu-tests.log`、`out/r4-image-program-gpu-android-tests.log`。
构建失败或测试夹具早期失败的日志保留，以上结论依据修正后的通过结果。

现有共 38 个可运行示例。作品数量与 R6 基础/高级各 50 的品质目标分别跟踪。
模型动画、R5/R6、Apple 和长稳仍未完成；不把该 profile 当作完整自定义 GPU 平台。
