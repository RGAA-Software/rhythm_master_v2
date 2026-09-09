# Windows 系统中文输入法验收

## 实际路径

`system_ime_tests` 启动可见的真实 Studio，使用生产文本编辑器和 SDL3／Dear ImGui
输入链路。测试准备两节点文字工程、完整 Noto 字体和 OFL 资产，通过 UI 查找并
聚焦文字属性。Python 仅向匹配测试进程 PID 和标题的前台窗口发送系统 VK 按键
N/I/H/A/O、空格、Enter，没有粘贴或 `AddInputCharactersUTF8` 注入最终汉字。

当前 Windows 已安装简体中文输入法。脚本向测试窗口请求切换中文键盘，结束前
恢复该窗口原来的键盘布局；窗口失去前台时拒绝继续输入，不向其他应用发送字符。
系统候选确认形成“你好”草稿，此时图请求代数保持不变；Enter 才提交到编辑历史
并触发新计划。随后实际 UI 保存、发布、重开，分别核对工程、运行包和当前计划。

## 证据和失败记录

- `out/p5-system-ime-build.log` 保留测试源码首次缺少 package 声明头的编译失败；
  `out/p5-system-ime-include-build.log` 补齐明确依赖后通过。
- `out/p5-system-ime-tests.log` 保留首轮自动化拒绝：测试进程有两个可见窗口，
  原判定只按 PID，不能任意选择。改为同时要求主窗口标题 `Rhythm Master`。
- `out/p5-system-ime-host-window-tests.log` 已通过实际输入与持久化，但截图工具
  受 DPI 虚拟化影响，错误截到桌面其他区域；此轮截图不能作为输入法界面证据。
  Python 进程设置 Per Monitor V2 DPI awareness，使窗口边界与 FFmpeg gdigrab
  的物理坐标一致；没有修改用户显示缩放配置。
- `out/p5-system-ime-dpi-capture-tests.log` 通过，证据目录
  `out/system-ime/2bb1ae59f83f473f9f00841dcdfa9593/`。人工逐张查看：
  `candidate.png` 显示 `ni'hao` 与第一个候选“你好”；
  `committed-draft.png` 文本框是“你好”，尚未应用的节点输出仍空白；
  `reopened.tga` 转为 PNG 后可见重开状态、文本框及节点预览中的“你好”。
  `result.json` 检查草稿隔离和保存／发布／重开通过，`workflow.log` 保留代数变化。
- 看图发现字体选择框沿用了“模型资源”标签，已改成双语专用“字体资源 / Font
  asset”，稳定 UI ID 保持不变。最终 `out/p5-system-ime-font-label-tests.log` 再次
  通过，目录 `out/system-ime/295b8c92e9bd46e09abae380e5f1d84d/` 的候选、草稿和
  重开截图逐张核对，字体标签和“你好”节点输出均正确。

这与既有混合中文／标点、字体覆盖、包内字体和两端渲染检查互补，不宣称所有
Windows 输入法、复杂文字塑形或移动端节点编辑器已验证。没有改变第三方 SDL／
ImGui 源码；复用其现有 IME 适配接口。

## 后续复测入口

需要已登录的交互式 Windows 桌面和已安装中文输入法，不能在无桌面的 CI 中
将跳过作为成功。先构建 `system_ime_tests`，然后运行：

```powershell
python tools/verify_windows.py --log out/system-ime-check.log -- python tools/test-system-ime.py --executable out/windows-release/src/windows_spike/system_ime_tests.exe --resources out/windows-release/src/windows_spike --output out/system-ime
```

每轮生成独立目录，失败保留；成功仍需人工查看系统候选和最终输出截图。模板应用
强制五项检查继续由每次 Studio 交付构建执行，不由这个输入法专项替代。
