# Android APK 取到旧效果包

P2.4 交付检查发现：源码和 Windows 的光幕协奏已为 104 节点，重新生成的 Android
APK 却仍带 94 节点包。`out/p2-action-work-music-android-tests.log` 的音乐/静音像素
和瞬态计数精确重复上一版，只出现来源 87，缺少新低/中频来源和 event.input。
这份日志仅证明旧包可运行，不能算新作品验收。随后直接查看 APK staging 包的
operators 清单确认缺少 event.input；Windows 包已包含该算子，两者虽都为合法 ABI 5，
内容不是同一版本。

根因：Android 打包消费 Windows 预编译内容目录，只有“文件存在”的检查，没有
把最新宿主内容构建列为前置依赖。两端同时构建时，Android 在 Windows 内容更新前
完成打包。其输入指纹把新源 manifest 与旧运行包一起视为一组合法输入。CRC、内部
哈希和实际渲染检查只能证明拿到的包有效，不能证明它来自本次源码。另外，仅依赖
native POST_BUILD 会在本地库无需重链接时跳过 APK 更新。

修复：

- `build-android-player.py` 先通过 Windows 共享构建入口完成
  runtime_builtin_content 和 demo_audio_content，再获取同一跨进程租约读取并组装。
  不在持有租约时递归启动 Windows builder。
- `compile-content.py` 为编译结果记录作者输入和编译数据哈希；发布脚本记录实际
  包哈希。Android 在复制前核对作者目录、记录和包字节，拒绝仍合法但已过期的包。
  CMake 声明生成物及脚本依赖；所有程序性处理留在 Python。
- Android 的 rhythm_android 交付请求转到始终执行的 android_player_apk，使无原生
  改动的内容/Java 更新也会检查与打包。仍然覆盖安装，保留应用数据。
- 每个频段事件来源单独验证音乐计数非零、静音为零，避免只检查最后一个来源。
- 修正 rhythm_package 的显示文字：使用真正的 ABI 判定，事件包不再误报为 ABI 4。

永久负例 `tools/test-content-identity.py`：源码改变而旧包仍存在、编译结果改变却
未更新记录、发布后包字节改变均拒绝；同步重新生成后恢复成功。CTest 为
`content_identity`。后续实包构建与设备结果继续追加，不能用上述负例代替设备验收。

修复后的证据：`out/p2-action-content-freshness-android-build.log` 完成宿主依赖及新 APK；
`out/p2-current-content-noop-android-build.log` 在原生无改动时仍执行依赖校验，最后明确
报告 APK unchanged。`out/p2-content-identity-windows-tests.log` 负例通过。
Windows 发布工具现在准确报告 ABI 5、104 instructions、4 assets。

`tools/test-android-music.py` 从最终 APK 取出包，与当前作者输入记录和宿主包哈希
核对后推送；原生探针必须匹配调用者指定的 104 节点。结果见
`out/p2-current-apk-music-android-tests.log` 与
`out/android-music/5976d6b23b8a44158b131533c1d69b18/identity.json`。
音乐高/低/中频来源 87/97/103 分别为 49/51/50，静音全为 0；四个时刻均有实际
音乐/静音像素差。覆盖安装后通过内置目录重新选择作品，应用保存包 SHA256 与
当前包同为 `687b452812339dd225291676d19f9bed0153652f83ba170632243b7939dfa7ea`。
不以升级应用自动覆盖用户已导入作品；内置目录重新选择是此次实机采用的路径。
