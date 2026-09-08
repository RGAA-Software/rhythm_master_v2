# Android 全屏演出

Player 控制区的“全屏演出”隐藏控制区与系统栏，作品按画布比例铺入整个可用屏幕，
保留必要的留边，不裁剪或拉伸。轻触画面可唤出“显示控制区”，按钮三秒后自动隐藏；
点按钮或返回键恢复控制区。全屏模式不改变音乐位置、暂停状态、公开参数或演出队列。
横竖方向仍由所选作品决定，全屏不强制横屏。

`PlayerPresentation` 独立负责响应式视口、全屏状态和临时按钮生命周期；
Activity 负责转发生命周期与输入。没有增加渲染器、播放时钟或媒体接口。
后台移除隐藏任务，回来恢复系统栏策略；新启动默认显示控制区。

## 复用

直接调用既有 SDL 3.2.20 的 `SDLActivity.setWindowStyle`，沿用其沉浸窗口和
系统栏恢复实现。已检查本地 `SDLActivity.java`、`SDLSurface.java`，来源为
`https://github.com/libsdl-org/SDL` revision
`96292a5b464258a2b926e0a3d72f8b98c2a81aa6`，zlib 通知继续由既有部署携带。
上游源码保持不变，没有新增依赖或复制 Android 框架实现。

两个实机发现已处理：SDL 的 Surface 先接收返回按键，故全屏在 Activity
分发边界处理完整按下/抬起序列；SDL 恢复时会重设 Surface 触摸监听器，
故 Activity 只观察触摸来唤起按钮，原事件仍交给 SDL，不替换其监听器。

## 2026-09-09 短验收

覆盖安装最终 APK，USB Redmi K40S / Android 14 / Adreno 650：

- 织光机横屏进入全屏，控制区和系统栏隐藏，画面保持比例并正常显示。
- 三秒隐藏按钮后，轻触重新显示；点按钮恢复控制区。
- 返回键恢复控制区，应用保持运行。暂停位置在切换前后均为 0.544 秒。
- 选择内置“暮色流光”，自动竖屏；全屏维持竖屏画布比例。
- 竖屏全屏按 HOME 后返回，画面与全屏恢复；轻触仍能显示按钮并恢复控制区。
- 原有搜索返回 1/45 项，选择、暂停和作品方向策略继续可用。

构建证据 `out/r6-presentation-final-build.log`。最终截图：
`out/r6-presentation-final-{paused,reveal,controls,back}.png`、
`out/r6-presentation-portrait-{controls,full,resumed,returned}.png`。
SDL 生命周期与视口尺寸日志在 `out/r6-presentation-device.log`。

持续更新的主控制区不满足 uiautomator 的 idle 等待，相关 dump 未生成，
不把失败的 XML 采集当成通过；上面的行为使用实际输入及截图检查。
本批不包含长稳、其他手机/Android 版本或系统手势导航专项验收。
