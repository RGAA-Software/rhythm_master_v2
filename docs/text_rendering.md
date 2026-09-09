# 作品文字实施记录

P5 已实现文字栅格化、`texture.text` 节点、后台资源准备、图存储和运行包合同，
并接入 Studio 多行输入与字体资产选择。实际 UI 创作、GPU 画面、MP4 导出和 Android
应用短功能验收已通过，详见 [文字创作证据](validation/text_authoring_2026-09-10.md)。
现有编辑器 UI 字体保持不变，P5 整体尚未完成。

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

完整 CJK 字体触发了旧 8 MiB 普通资产限制，失败记录是
`out/p5-text-preparation-tests.log` 的 `package.asset_bytes`。此次将共享普通资产预算
提升为 32 MiB、内存归档上限 48 MiB、含独立 256 MiB 音乐的文件归档上限 304 MiB；
数量仍限 64 项，程序仍限 8 MiB，解压后字节和累计字节仍严格检查，不能绕过压缩炸弹
边界。旧版本可继续拒绝超出其能力的大包；文字作品另以 schema 8 / ABI 6 拒绝降级。
既有文档中的 8/16/272 MiB 数字是此前版本的预算，当前以 `project/package.h` 为准。
文件归档测试使用 40 MiB 素材跨越新内存资产边界，继续检查流式读取及失败保留原文件。

`Rasterizer` 拥有不可变字体字节、FreeType 对象和 glyph 缓存，只在单个工作线程使用；
渲染端接收值类型灰度 mask。所有 FreeType 资源由私有 RAII 释放，公共头文件没有
FreeType、图形 API 或宿主类型。缓存不随文字颜色和图变换改变；字号和字形索引属于键，
字体身份由 rasterizer 实例隔离。

字体上限 32 MiB，文字上限 16384 UTF-8 字节 / 4096 码点，画布每边最大 2048，
字号 8–256 px，glyph 缓存最多 2048 项 / 8 MiB，单次合成最多处理 16 Mi glyph 像素。
超过预算或无效 UTF-8 显式拒绝；缺字记录计数并采用字体 replacement/notdef 字形，
不能把有方框的成功栅格化当作字形覆盖通过。被画布裁剪的非零覆盖像素显式报告。

## 接下来的集成

Windows/Android 原生模块检查和混合中文截图已通过。Windows 的 graph/schema 8、
program ABI 6、字体资源准备与旧版本拒绝测试已通过；Android 原生程序也通过完整
字体包准备测试（`out/p5-text-preparation-android-tests.log`），不等于应用画面验收。

Studio 的多行输入按应用或 Enter 提交，Ctrl+Enter 换行，避免每个按键都触发历史和
后台准备。字体节点匹配准备结果后显示缺字和裁剪信息。内置字体按钮在一个异步任务
中导入完整 Noto 和 OFL 许可，只有全部成功才把两项元数据加入同一次历史；失败可能
留下内容寻址的孤立 blob，但不会留下半套作品资产记录。

位置、颜色和音乐控制复用现有变换与合成节点，避免按帧重新排版或重复上传静态文字。
派生图像按字体 SHA 和精确文字/布局键区分。后台 Loader 已接入跨文字修改的
持久 glyph 缓存，由唯一工作线程拥有，销毁先取消并 join，再释放缓存。最多两套
字体，常驻字体字节不超过 64 MiB、glyph 存储不超过 16 MiB；新字体先构造再淘汰
旧字体，构造期间最多另加 32 MiB。缓存不绕过源记录和 SHA 校验，不发布到 UI。

Windows `out/p5-text-persistent-cache-tests.log` 中缓存合同通过：重排已有字符
不增加栅格化次数，字号改变产生新字形，淘汰字体后正确重建，热缓存拒绝损坏源。
实际 UI 路径复测发现并修复了 [并发工程读取问题](validation/concurrent_project_read_2026-09-10.md)。
后续已增加其他不变图片／模型／视频／Shader 和完整文字布局的准备复用，
Windows 与 Android 原生检查通过，见
[局部准备与边界](validation/preparation_reuse_2026-09-10.md)。
仍未实现动态文字信号输入；系统拼音候选、草稿隔离、确认提交与保存／发布／
重开已通过真实 OS 按键和人工截图核对，见
[系统输入法证据](validation/system_ime_2026-09-10.md)。本次 Windows 完整部署／
实际创作复测及 Android 覆盖安装／应用内播放已通过，见上述局部准备记录末段。

`out/p5-text-persistent-cache-android-tests.log` 与 `out/p5-current-read-android-tests.log`
分别通过 USB Android 原生缓存和并发持久化合同。更新交付见
`out/p5-cache-current-windows-delivery.log`（5 项强制检查通过）与
`out/p5-cache-current-android-delivery.log`。新 APK 已覆盖安装，SHA256 为
`d8de4f5b7d6d3bd82debcfe0d18b4fcdf1e51702243c1c2bb5afe57aca59b348`。
`out/p5-cache-current-android-ui.log` 保存实际 UI 复测，目录
`out/android-authored-works/eb27e6e56b9b48fda7da20fef7209912/`；已人工核对正确的
`Prismatic Title`、中文和频谱，暂停 0.59 秒后恢复至 1.57 秒，原节目单保持不变。

2026-09-10 证据：`out/p5-text-layout-build.log`、`out/p5-text-layout-tests.log`、
`out/p5-text-layout-android-build.log`、`out/p5-text-layout-android-tests.log`。
手机 `e2b3b128` 运行的是 NDK 编译的原生测试程序，不是 Android 应用 UI。
已逐字检查 `out/p5-text-layout-windows.png` 与 `out/p5-text-layout-android.png`，
“棱镜星莲 / Rhythm”和“音乐：Pulse，2026！”均正确。两端 FreeType 版本不同，
不要求任意字号/任意字体逐像素一致；字形覆盖、布局合同与预算拒绝分别验收。
