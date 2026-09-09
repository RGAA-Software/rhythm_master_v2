# 持久演出列表与连续播放

P3 实施记录，2026-09-09。对应 `master_implementation_plan.md` P3.1–P3.5。
列表合同、原子存储与托管包/队列适配已通过两端原生检查，Windows Player 列表 UI
已交付；Android UI、音频淡化和分阶段 GPU 准备尚未交付。

## 保存语义

演出列表是用户编排的完整节目单，运行队列是它的一次播放实例。播放消费队列
不删除已保存节目单；显式加载节目单重新生成队列。编辑后的列表显式保存，失败
保留旧文件。首版一个应用管理的当前节目单，最多 16 项，与现有队列预算一致。
每项保存稳定 entry ID、作品引用、显示标题、0–5 秒过渡和 Immediate/Beat/Bar
量化设置。重复作品合法，删项再添加不能复用旧 entry ID。恢复不自动开声或切场。

作品引用分两类：内置作品保存 content ID、版本和包 SHA-256，可选择跟随当前
内置版本或精确固定；托管作品只按包 SHA-256 精确解析。内置目录同 ID 多项时
报告歧义，不能任意选择。版本或字节更新显示 Updated；固定版本变化显示 Changed；
缺失显示 Missing，可恢复后重试，不退回默认效果。托管副本进入应用持久目录，
不序列化文件选择器、Android cache、外部路径或 GPU/线程对象。

`src/performance` 保存平台无关的列表与解析合同，向内只依赖已有参数和资产值类型；
`project_io` 负责版本化 JSON 与原子写入；`player_core` 负责托管包/队列适配。
这样 project_io 不依赖已经消费 runtime_package 的 player_core，避免循环依赖。
首版文档拒绝未知版本或无法解释的字段，不能静默丢弃未来设置后覆盖保存。

## 复用核对

本地 TiXL `fbc994d923e8a0142d2ff1b772e4d12248c5b0ba`（MIT，
`https://github.com/tixl3d/tixl`）的 `Core/Settings/CompositionSettings.cs`
把播放/音频/导出配置作为可序列化值；`Operators/Lib/Symbols/render/basic/FadingSlideShow.cs`
暴露图像索引与淡化输入。前者依赖 C# Symbol/Playback/Json.NET，后者是算子 Slot
声明，两者都不是包含跨平台托管包身份和缺失恢复的演出列表。此次只参考职责划分，
不复制上游源文件或推断 Android 支持。JSON 直接复用已验证的 vcpkg nlohmann/json；
写入复用本项目 atomic_storage，包准备复用 SceneQueue/PackageLoader，身份复用
AssetId/SHA-256。新增代码仅实现项目列表合同与适配，不新增第三方依赖。

## 后续退出条件

1. 两端原生验证顺序、重复作品、删项后 ID 水位、边界和版本解析。
2. 版本化保存重开、损坏/未来版本拒绝、失败保留旧文档；托管副本离开缓存后可用。
3. Windows/Android 实际 UI 保存、重开、排序、删项和恢复；重新启动保留列表，旧场不动。
4. 同一设备时钟完成音画过渡；两个场景合计最多 4 个解码游标、256 MiB 纹理、
   240 个离屏 pass。淡化失败/取消保留当前场；已混合 PCM 同时提供两场 FFT。
5. 首次 GPU 创建先测峰值，再验证可取消的渲染线程分帧准备；长稳放 P9。

## 增量证据

- `performance_list`：Windows `out/p3-list-lexical-windows-tests.log`、Android
  原生 `performance_list_tests` 在 USB `e2b3b128` 通过。覆盖重复、排序、删除/恢复后
  水位、16 项上限、NaN 拒绝不修改原值、更新/固定/缺失/歧义和托管哈希。
- 同轮 `source_boundaries` 最初误报测试文案里的 new/delete 和 `return *current_`。
  `check-boundaries.py` 现区分注释/字符串/原始字符串与代码，排除返回解引用，补充
  array delete 检测；`test-boundary-lexer.py` 保留真实违例与文案/标识符反例。
  两份失败日志保留，最终上述 Windows 日志两项通过。这是检查器修正，不是放宽
  所有权规则。P3 UI 和持久化尚未因此验收。
- `performance_store`：`out/p3-store-windows-tests.log` 与
  `out/p3-store-android-tests.log` 通过。真实文件保存/重开保留中文、排序、重复作品、
  条目水位与量化；注入写入后/校验后失败和写锁冲突，旧文档不变、暂存文件回收。
  未来版本、未知字段、重复 JSON key、非法 ID/来源/路径、超预算、截断输入拒绝。
  仅显式保存修复文档，不在读取失败时自动覆盖默认列表。
- `WorkLibrary` 复用资产库的不可变导入、SHA-256、去重和精确修复，`Store::OpenId`
  按身份验证后返回文件 lease；PackageLoader/SceneQueue 新增文件 lease 入口。
  `out/p3-managed-asset-{windows,android}-tests.log` 和
  `out/p3-work-library-{windows,android}-tests.log` 通过。实际删除原导入文件，
  重建作品库后仍能准备并交给 Session；重复队列项身份独立，错误替换保留原包。
  外部源在导入中可能变化，因此复制前与不可变副本都做包校验；异常时不返回可写入
  列表的引用。共享文件持有只保留受限文件资源，不提前创建 16 套渲染资源。
- `PerformanceProgram` 的保存/重开/导入/解析共用一个有界后台任务，期间拒绝冲突
  编辑，只有 host Pump 修改草稿；取消解析不安装迟到结果。SceneQueue 原子装入完整
  结果，缺失行保留，不能跳过；保存的完整列表不因播放消费而删除。
  `out/p3-program-immutable-{windows,android}-tests.log` 通过，另有原队列回归。
  目录更新会复核实际字节 SHA-256；初次 Windows 失败来自测试原地改写仍被文件
  lease 持有的包，修正为实际发布使用的原子替换后通过，失败日志保留。
- Windows Player 的“演出列表”窗口：添加/复制/排序/删除、过渡与量化设置、固定或
  跟随内置版本、托管导入、保存/重开、准备到播放队列。队列明确显示缺失/更新/固定
  版本变化和字节错误；Go 使用保存的条目设置，通用时长控件在该模式禁用。
  `out/p3-windows-performance-ui-final-tests.log` 五项通过：两语言真实 ImGui 导航
  激活操作覆盖保存文件与重开/队列顺序；既有队列 GPU 切场、最终 deploy Player smoke
  与源码边界通过。UI 用例不是手机触屏证据。已生成
  `out/windows-release/src/windows_player/deploy/rhythm_player.exe`，含 20 个 DLL 和资源。
