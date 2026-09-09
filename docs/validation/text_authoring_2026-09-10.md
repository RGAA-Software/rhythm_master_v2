# 文字作品实际创作验收

P5 文字基础的实际路径是 `from_empty_studio_text`：Studio 从空画布逐节点添加，
通过资产管理器导入完整 Noto 字体和 OFL 许可，用多行输入提交“棱镜星莲”和
`RHYTHM / 2026`，连接两路频段信号，绑定原创音乐，测试输出平移/旋转/缩放，
通过属性面板恢复交付构图，封装组件，保存、发布并重开。

当前通过证据：`out/p5-text-final-framing-tests.log`，作品目录
`out/windows-release/from-empty-studio-text/d40b1a31d3ac4524b27b059606b13239/`。
同一发布包在 D3D11 中采用真实解码的音乐、静音、低频和高频输入，四组差异分别为
6.055945、7.669595、1.261773、8.873706（RGB 每通道平均绝对差，0–255）。
已目视核对最终音乐截图，中文字、英文数字和透明合成正确，图像不是默认渐变。
`out/p5-text-authoring-component-tests.log` 同时通过既有二维作品回归。

## 保留的失败与修正

- `out/p5-text-authoring-tests.log`：测试立即读取异步保存，读到旧资产表。磁盘最终
  记录包含完整字体与许可。修正为有界等待已提交的资产表后再断言。
- `out/p5-text-authoring-save-tests.log`：组件保存有同一过早读取问题。改为等待
  已提交组件后核对内容，而非增加固定睡眠。
- `out/p5-text-legibility-tests.log`：测试的临时 JSON 遍历对象生命周期错误，使
  新增最终构图参数的输入定位失败。命名值持有整个遍历生命周期后重建并通过。
- 首个通过的画面标题偏暗且保留了手势测试的整体倾斜。没有把像素差异通过当作
  视觉可用；提升文字亮度下限，并通过实际属性编辑恢复正向构图，再完整验证。
- `out/p5-title-runtime-package-build.log`：模板内容编译器仍保留 8 MiB 旧常量，
  实际字体模板打包被拒绝。同步到 32 MiB 并补 64 项数量检查，保持 SHA 和累计
  字节校验。通过记录 `out/p5-title-content-budget-build.log`；此前直接 UI 发布
  通过不代表 Python 内容编译路径通过。

## Windows 导出与 Android 原生合同

`out/p5-title-export.log` 通过共享 FFmpeg 路径导出 640×360、30 FPS、120 帧，
完整读取视频和音轨。同一音乐重复导出逐帧哈希一致，静音视频不同，音轨均方误差
为 0.0000076297，取消导出与资源释放也通过。可播放文件：
`out/p5-title-export/630165961489500/music.mp4`。这是 4 秒短功能验证，不是长稳。

`out/p5-title-thumbnail.log` 生成了实际 D3D11 Player 缩略图，其输入是固定合成
音频特征，不能代替上面的解码 PCM 证据。
`out/p5-text-android-io-tests.log` 是 USB 设备执行的 NDK 原生 schema/ABI、UTF-8、
缓存身份和降级拒绝检查，不是 Android 应用 UI 验收。

## 当前两端交付

`out/p5-text-windows-delivery.log` 完成 Studio 和 Player 的完整 deploy，各含 20 个
DLL、可执行程序、字体/许可与内容资源。5 项强制检查全部通过：编辑合同、模板合同、
默认预设覆盖和中英文实际模板切换 GPU 流程。

Android 构建 `out/p5-text-android-delivery.log` 完成宿主内容依赖与最终 APK 校验，
`out/p5-text-android-install.log` 在 e2b3b128 覆盖安装成功；未卸载或清除数据。
APK SHA256 为 `e31bc9562ca765371fab37236c0e12ef4418394d5e24d2d0344e05c975581164`。

`out/p5-text-android-music.log` 从实际 APK 提取并核对文字包身份，在 Redmi K40S /
Adreno 650 上以 GLES 运行 16 条可达指令、音乐与静音各 960 帧，渲染 640×360，
读回 320×180。2/6/10/14 秒平均 RGB 差分别为 4.29547、6.02814、4.26517、6.10396。
纹理峰值 7,563,268 字节，音乐 RMS 峰值 0.236431。音乐 p50/p95 为 9.15432/11.3203 ms，
是原生同步诊断耗时，不是应用显示帧率或 GPU 计时。证据目录：
`out/android-music/3f82330c3b204ddfbd418822fd4be009/`。

应用内验收最终证据为 `out/p5-text-android-ui-paced.log` 和
`out/android-authored-works/28c11897cd394d8e853ffcf08c24d9b1/`。已人工核对 paused /
resumed 两张实际截图：标题为 `Prismatic Title`，画面中“棱镜星莲”和
`RHYTHM / 2026` 正确，横屏，暂停 0.56 秒后恢复至 1.55 秒，频谱和位置发生变化，
RMS 非零。原演出列表字节保持不变；不是声学回录或输入法验收。

首次 UI 脚本用 `Prismatic` 搜索后仅按中文前缀点击，误选了同中文名的旧
`Prismatic lotus`。截图审核发现后没有把该运行记为通过，原始记录
`out/p5-text-android-ui.log` / `e60a721226874a50bb55094b7af17123` 保留。脚本状态
一直是 `presentation_review_pending`，不能仅凭 `catalog_selection` 字段认定最终
作品正确。之后唯一检索词的一次突发键盘注入被设备改序为 `iTtle`，失败记录
`out/p5-text-android-ui-unique-query.log` 保留。最终采用小写唯一词、逐字符输入，
并要求只匹配一条标题，仍须核对实际主窗口标题与画面。

## 验收边界

这是功能示例，不计入 P7 高端模板品质数量。测试通过 ImGui 输入队列提交 UTF-8，
不能当作操作系统中文输入法候选窗、组合文本和撤销的实测证据。原生文字测试
不能替代上述实际应用路径。P5 矢量、桥接及跨编辑持久缓存尚未完成。
