# 官方组件资源与墨绘地形

2026-09-09：官方组件此前只能包含无资产节点。现在 Studio 点击“插入组件”时，
会在既有组件库后台任务中准备图片、Shader 等所需资源，然后通过原有历史事务插入。
第一份实际资源组件为“墨绘地形 / Ink cartography”。这不是完整作品数量的新增。

## 实现与边界

- 从用户组件捕获函数提取既有的嵌套定义、布局及 AssetId 引用遍历，供两条路径复用。
  官方组件保留 `component.official.*` 身份；用户保存仍使用内容派生身份。
- 复用 `ComponentLibrary` 的一个有界工作线程、`assets::Store::CopyFrom` 的哈希校验
  和 Protobuf/工程编解码。没有增加资源格式、线程池或第三方依赖。
- 只携带选中组件及嵌套定义引用的资源，预览工程的独立节点和资源不会进入作者工程。
  单次准备沿用 64 条记录、8 MiB 的普通资源预算。文件复制不在 UI/渲染线程执行。
- 完成结果带工程 ID、修订号和最初的插入位置。Studio 沿用现有的工程切换、修订号、
  参数预览和未提交标题检查；不匹配时拒绝应用。资源记录和组件编辑属于同一次撤销。
  缺失资源及定义/记录冲突返回错误；主状态区能展示后台失败，不依赖用户展开组件库面板。
- 撤销或过期结果不会删除已复制的不可变内容块。它们不再被工程引用，沿用资产维护流程。
- CMake 仅追踪语义组件源资产的构建依赖，复制和哈希检查继续由现有 Python
  `compile-content.py` 完成；Windows 完整部署仍自动调用 Python。

## 实际组件

`content/semantic/ink_cartography` 共 15 个内部节点，预览含 17 条可执行指令。
输入图像作为高度场；低频影响墨量/岸线，高频改变刻线。公开速度、响应、柔化、
线数、刻线混合及岸线混合六个参数，提供默认和“细绘图谱”变体。
预览使用灰度噪声示范，作者插入后连接自己的图像。

直接复用“墨潮”中两个已验证的可编辑 Shader 包，原字节未修改：

| 资源 SHA-256 | 字节 |
| --- | ---: |
| `18c55484fac8fb9bf262a135d655c0a1fefb5e15dac6f5d5442a4f91f2492001` | 4282 |
| `1d7409bc2408c2a896574e531e43462db7005670871a31e94a76b155a8e17bfb` | 3667 |

作者脚本为 `tools/author-ink-component.py`，来源和改动见
`provenance/ink_cartography.json`。图和表达式为项目自有内容，未新增第三方代码导入；
既有渲染节点的来源与许可继续适用，项目对外许可证仍未选择。

## 验证

- Windows 增量编译使用 20 workers，无新增项目代码编译告警。
  `user_components`、`performance_workflow`、`semantic_catalog`、`content_contracts`、
  `component_library_interactions`、`windows_deploy_smoke` 六项通过。
  日志：`out/r6-official-final-tests.log`。
- 组件资源测试覆盖稳定身份、排除无关资源、后台队列忙碌拒绝、预期工程/修订号、
  缺失文件、冲突记录及一次撤销。Android NDK 增量编译同一测试目标并在 USB 手机通过，
  日志：`out/r6-official-native-android.log`。
- 29 个组件均通过插入、参数默认/变体、定义冲突、发布和空渲染器运行检查。
  资源组件在插入后的工程中读取实际资产，发布后准备 Shader，未绕过资源检查。
- 中英文实际 D3D11 组件浏览器通过选择、GPU 预览、固定窗口大小、显式插入、
  同一后台面板准备、图及资产撤销和关闭释放。截图/日志位于
  `out/r6-ink-component-browser/`，中文截图已查看。
- 最终默认预览通过实际解码 PCM 驱动的 GPU 检查：真实配乐/静音平均 RGB 差
  6.1769774，低频/静音 28.5902962，高频/静音 3.7248137，低频/高频 30.6357400。
  此测试纹理占用 29,491,204 字节；日志和像素位于 `out/r6-ink-component-music/`。
  初稿有额外条纹示范，最终改用连续灰度场后重新执行上述检查。
- Redmi K40S / Adreno 650 / Android 14 / GLES 3.1：Windows 发布的最终默认包和
  插入工程生成的变体包各通过 60 帧及设备重建检查。
  日志：`out/r6-ink-component-default-android.log`、`out/r6-ink-component-variant-android.log`。
  这是可复用组件，未向 Android 完整作品列表添加重复预览项；本增量未重装 APK。
- 最终缩略图由真实 GPU 生成，记录目录为
  `out/catalog-thumbnails/a9adfde34463402ca24249a7915800c3/`。
  缩略图使用标准合成特征输入，与上述真实 PCM 检查分开记录。
- Studio/Player 最终部署均自动包含 20 个 DLL 和完整资源；
  `out/r6-official-final-deploy.log`，部署冒烟通过。

当前库存为 29 个语义组件、58 条组件预设、133 条原生预设，共 191 条记录；
完整作品仍为 45 个候选。40 组件、120 个独立视觉验收预设和 50 基础 + 50 高级作品
目标尚未完成。本文不宣称视觉品质、长稳、其他 Android 设备或 Apple 平台已验收。
