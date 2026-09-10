# 四格概念图的实时作品交付

目标是把 `docs/design/concepts/music_visual_directions_v1.png` 的四种方向做成
可编辑、音乐驱动的作品。概念图不是运行时素材，下面的截图来自实际 D3D11
渲染。工程检查与最终审美验收分开，未代用户填写验收通过。

| 作品 | 可达节点 | 实现与音乐映射 | 自评及与概念的差距 |
| --- | ---: | --- | --- |
| 鎏光流涡 / Aureate Vortex | 110 | 浮点极坐标场、青金流线与亮点；低频扭转、中频丝线、高频闪点 | 已形成完整流涡和明暗分区；解析式图像粒子外观，没有真实体积粒子、景深和历史拖尾 |
| 瓷金绽放 / Porcelain Bloom | 208 | 共享形变瓷瓣、双层反向旋转、黄铜断环、细釉纹与 PBR；低频开合、中频旋转、高频材质亮度与珠点 | 瓷色和金环可辨；局部自阴影仍硬，花瓣较规整，与概念的柔和摄影级材质有差距 |
| 层叠墨流 / Stratified Ink | 109 | 浮点高度场、靛蓝朱红层带与细金边；低频主带、中频流形、高频金边 | 配色和斜向层叠结构成立；更接近动态等高线，未达到概念中的自然流体和细碎支流 |
| 光门空间 / Lumen Corridor | 403 | 16 层共享几何光门、局部灯光、镜头与辉光；低频宽度、中频高度、高频光带 | 纵深和双色灯带成立；结构更规整，地面没有概念中的清晰镜面反射 |

四件作品均有音乐响应、运动速度、曝光三个公开宏、三组快照、四段 Cue，
内置 16 秒原创合成音乐。63 个频带完整分组，不仅取三个孤立频点。
两件 3D 作品采用已有 2× SSAA，图像效果使用现有浮点纹理与 FXAA。
复用本仓库第一方图、模型作者和已归档渲染器；未引入新第三方库。
具体源文件、基线、作者脚本摘要和资产记录在 `provenance/<作品名>.json`。

## 截图自评与保留的失败

[实际输出评审页](../design/reviews/concept_works_2026-09-10/index.html) 并排提供概念图、
四张实际音乐输入截图和独立自评。固定截图使用 1280×720、4 秒、相同输入历史；
不能代替连续运动和听感验收。

1. `out/concept-works-authoring.log`：双参数 atan 在当前 D3D 包装编译失败。
   改为单参数 atan 与象限修正，保留现有表达式范围，没有扩大公共 Shader 合同。
2. `out/concept-works-quality-first.log` / `out/concept-review/d35c3d2559dd4a9e85c908a36032bd87/`：
   流涡量化响应通过，但截图出现象限块。根因是带符号坐标写入继承的 UNORM 纹理；
   改用浮点坐标纹理。说明像素差值不能替代人工构图检查。
3. 同次瓷金高频／静音差值 0.101089，低于原门槛 0.15；增加高频对瓷质亮度的响应。
   不降低门槛，不以低频通过代替高频验证。
4. `out/concept-works-quality-detail.log`：新增釉纹后量化通过，但截图底色发黑。
   轮廓算子输出的是带透明底的线条，不是双色不透明底图；补上瓷色底层合成，
   并重新检查七份 PCM 和三个宏的上下限。失败截图保留在
   `out/concept-review/a9470708f4f2406aab5f377c53c1cd42/porcelain_bloom/`。

## 直接包渲染验证

- 流涡最终图：`out/concept-works-quality-detail.log`，七 PCM／十比较通过；
  三宏上下限见 `out/concept-works-controls.log`。
- 瓷金最终图：`out/concept-works-glaze-quality.log` 与
  `out/concept-works-glaze-controls.log`，七 PCM／十比较与三宏上下限通过。
- 墨流、光门最终图：`out/concept-works-quality-refined.log` 与
  `out/concept-works-controls.log`，同样全部通过。
- 流涡音乐／静音 15.285，瓷金 1.930，墨流 7.777，光门 5.650；
  瓷金最弱的大小响度差值仍为 0.490，高于未更改的 0.15 门槛。
- `out/concept-works-thumbnails.log`：四张缩略图均来自 GPU 渲染。
  随后仅增加缩略图、去掉流涡一份未引用的旧 Shader 资产；图、音乐、控件不变。

永久入口 `tools/review-concept-works.py` 校验作者源和包身份，按作品保留独立日志、
结果与截图；通过 `tools/verify_windows.py` 获取串行检查租约。
Studio 强制切换回归加入四件作品，覆盖节点重编号、宏／Cue 引用、当前输出、
保存重开发布及实际鼠标控件编辑，不能用上述直接包渲染代替。

## Windows 交付工作流

`out/concept-works-delivery.log`：Studio / Player 的完整 deploy 均同步
20 DLL、程序与资源。9 项强制回归全部通过，110.30 秒，包含中英文
12 模板连续切换、8 作品共 48 次控件上下限鼠标编辑、保存重开发布，
以及大配乐替换工作流。未清构建缓存，使用 20 workers 增量构建。
本次完整测试日志：
`out/windows-release/studio-delivery-tests.log.runs/1789017991468927900.log`。

四件作品的实际 Studio 导出按钮均生成 H.264/AAC 文件，视频 480 帧，
两条流各 16 秒；FFprobe 校验记录在 `out/concept-works-preview/video-metadata.json`。
最终入口 `out/concept-works-export-final.log` 全部通过，独立证据目录
`out/concept-review/61f787e389c34c6e875b5257c54fd661/`。
每件作品的 8 个顺序导出画面已人工查看：流涡转向与亮点、瓷瓣反向旋转、
墨流层带移动、光门宽高与光带变化均能辨认。没有用静态概念图制作这些视频。

`out/concept-works-replacement-final.log`：四件作品分别实际更换配乐、保存、发布、
清除与重新打开，通过新配乐哈希、当前可达指令数、新计划和波形检查。
证据目录 `out/concept-review/16782b9f9f864be3b42acc59a4af65d2/`。

首次 export / replacement 的子检查通过，但外层日志回显失败：本地 GBK 控制台
不能编码解码替换字符 U+FFFD。失败入口 `out/concept-works-export.log` 和
`out/concept-works-replacement.log` 保留。`verify_windows.run_logged` 现在仅对
控制台不可编码字符作转义，原始日志字节和子进程退出码保持原样。
`tools/test_verification.py` 的 3 项测试通过，包括坏字节在 GBK 输出下的成功／
失败退出码与原始证据保留；修复后两个完整入口重跑均返回 0。

## Android 当前包与真实配乐

`out/concept-works-android-build.log` 完成宿主内容构建、身份核对与 APK 打包，
57 个内置效果。`out/concept-works-install.log` 覆盖安装成功，无卸载、清数据。
APK SHA256：`0f18e0aad669bcb0f41d08c273b0826ddcd915aacb8a925e83e0c574cf58a214`。
USB Redmi K40S / Adreno 650，serial `e2b3b128`。

| 作品 | 2 / 6 / 10 / 14 秒真实配乐对静音像素差 | 原生音乐 p95 ms | 原生纹理字节 |
| --- | --- | ---: | ---: |
| 流涡 | 10.948 / 12.197 / 12.856 / 11.441 | 12.708 | 9,676,804 |
| 瓷金 | 0.463 / 4.146 / 5.621 / 3.070 | 22.229 | 33,222,852 |
| 墨流 | 5.924 / 7.723 / 10.847 / 7.179 | 13.438 | 5,990,404 |
| 光门 | 3.501 / 6.980 / 10.073 / 5.739 | 28.462 | 21,147,844 |

四次均核对 APK 内包与宿主当前包、作者源身份，原生 640×360、音乐／静音各
960 帧，16 个比较全部高于原 0.15 门槛。该同步短测耗时包含等待，不能当作
应用显示帧率、完整显存或移动画质档位验收。长时间温升与稳定性仍留到最后。
日志 `out/concept-works-android-<作品名>.log`，原始证据分别位于：

- `out/android-music/6b1f9f4f99764b25aa958ea581f670aa/`
- `out/android-music/dd393e5999904a539d10b1afc7f8127d/`
- `out/android-music/a12bb78962554707a4ae3d1e954a7659/`
- `out/android-music/18de668fdc12474b82bd3f6c933d7aa9/`

`out/concept-works-android-ui.log` 与
`out/android-authored-works/b551fc93d4f44ee6a0c2e8befa72d2dd/`：
已安装 APK 摘要一致，四件作品均通过内置菜单选择；原节目单前后相同。
8 张暂停／恢复截图已逐张查看，标题、作品、16 秒时长和进度恢复正确：
流涡 3.52→4.53 秒，瓷金 2.94→3.87 秒，墨流 3.60→4.85 秒，光门 3.14→4.21 秒。
这是本次界面呈现自评，不是用户审美验收；没有声学录音或长时间性能测试。

## 本机查看

- Studio：`out/windows-release/src/windows_spike/deploy/rhythm_master.exe`，
  “选择模板”搜索上述中英文名称，均在高级类别。
- 手机：已覆盖安装，在“选择效果”中直接选择，无需浏览目录。
- [截图、概念对照与视频评审页](../design/reviews/concept_works_2026-09-10/index.html)。
  视频位于本机 `out/concept-works-preview/`，不随 Git 提交；提交包含实际 PNG
  和来源记录。可用 `review-concept-works.py --mode export` 重新生成视频证据。
- `tools/test-content-quality.py` 3 项通过，内容索引 `--check` 通过。
  57 模板 / 30 组件 / 208 非默认预设是作者内容数；最终品质验收数不因此增加。
