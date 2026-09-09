# 作品文字实施记录

P5 当前增量是独立文字栅格化模块，尚未接入节点、Studio 或 Player，不能视为文字
创作已经交付。现有编辑器 UI 字体保持不变。

## 依赖与复用

- `src/text_layout` 私有适配 vcpkg FreeType；Windows 当前安装为 2.12.1#3，
  Android 为 2.14.3。使用 FreeType License 路径，原许可在相应 triplet 的
  `share/freetype/copyright`。本软件的文字栅格化使用 FreeType Project 的 FreeType。
  上游地址 `https://gitlab.freedesktop.org/freetype/freetype`，版本分别对应
  `VER-2-12-1` / `VER-2-14-3`；没有复制或修改上游实现。核对安装头文件中
  `FT_New_Memory_Face`、`FT_Load_Glyph`、Unicode charmap、bitmap pitch 与像素格式合同。
- UTF8-CPP 3.2.3（`https://github.com/nemtrif/utfcpp`，tag `v3.2.3`）负责校验和解码，
  使用 vcpkg `utfcpp` 安装的头文件和 Boost Software License 1.0；原许可在
  `share/utfcpp/copyright`，未复制修改。Android 缺少该 header-only 安装，因此复用
  已安装 Windows triplet 的同一平台无关头文件，由 NDK 编译，不链接 Windows 二进制。
- 作品字体测试使用现有完整 Noto Sans CJK SC Regular，来源、精确 revision、SHA256
  和 SIL OFL 1.1 已记录于 `third_party/notices/noto-cjk/`。它是作品字体候选资产，
  不是重新替换 UI 字体。后续发布必须随作品携带字体与许可，不能依赖用户系统目录。
- HarfBuzz / ICU 仍是候选。当前限定横向 CJK/Latin、FreeType kerning 和按码点换行，
  不宣称支持 bidi、复杂文字塑形、字素级编辑或排版软件行为。

## 合同和预算

`Rasterizer` 拥有不可变字体字节、FreeType 对象和 glyph 缓存，只在单个工作线程使用；
渲染端接收值类型灰度 mask。所有 FreeType 资源由私有 RAII 释放，公共头文件没有
FreeType、图形 API 或宿主类型。缓存不随文字颜色和图变换改变；字号和字形索引属于键，
字体身份由 rasterizer 实例隔离。

字体上限 32 MiB，文字上限 16384 UTF-8 字节 / 4096 码点，画布每边最大 2048，
字号 8–256 px，glyph 缓存最多 2048 项 / 8 MiB，单次合成最多处理 16 Mi glyph 像素。
超过预算或无效 UTF-8 显式拒绝；缺字记录计数并采用字体 replacement/notdef 字形，
不能把有方框的成功栅格化当作字形覆盖通过。被画布裁剪的非零覆盖像素显式报告。

## 接下来的集成

Windows/Android 原生模块检查和混合中文截图已通过，再接入图文字节点、后台准备、
字体资产打包与实际 Studio 输入/保存/发布路径。位置、颜色和音乐控制复用现有变换与
合成节点，避免按帧重新排版或重复上传静态字形。

2026-09-10 证据：`out/p5-text-layout-build.log`、`out/p5-text-layout-tests.log`、
`out/p5-text-layout-android-build.log`、`out/p5-text-layout-android-tests.log`。
手机 `e2b3b128` 运行的是 NDK 编译的原生测试程序，不是 Android 应用 UI。
已逐字检查 `out/p5-text-layout-windows.png` 与 `out/p5-text-layout-android.png`，
“棱镜星莲 / Rhythm”和“音乐：Pulse，2026！”均正确。两端 FreeType 版本不同，
不要求任意字号/任意字体逐像素一致；字形覆盖、布局合同与预算拒绝分别验收。
