# 新增高级模板未进入 Studio 切换夹具（2026-09-11）

## 失败与根因

`Astral Forge` 首次加入后，Windows Studio 交付的中英文模板切换检查均失败：
`selected template switch fixture missing`。失败日志为
`out/windows-release/studio-delivery-tests.log.runs/1789088326157420100.log`。

Python 外层检查已按 manifest 自动枚举全部高级模板，但 C++ UI 驱动测试仍使用手写模板目录名单。
新模板会被外层传入，却无法在内层名单中找到。此前的固定名单与目录清单发生了漂移；这不是新模板
图、ID 重映射或渲染失败，但它会让真实 Studio 应用路径对新增作品失去覆盖。

## 永久修复

`template_switch_gpu_tests` 现在直接使用 `project::ScanTemplates` 的 `advanced` 条目，并以
`ContentEntry.directory_` 的目录名对应 Python 传入值和模板浏览器索引。新增高级模板无需同步维护
第二份名单；未知的指定目录仍明确失败。测试仍经真实 Studio 模板窗口进行搜索、选择、应用、保存、
重开、发布和当前输出截取。

## 修复后交付证据

`out/windows-release/studio-delivery-tests.log.runs/1789090639431426500.log` 通过 9/9：

- 中英文 `template_switch_gpu` 各自遍历全部高级模板，分别用时 52.53 秒与 49.76 秒；
- `template_contracts`、控件交付、媒体重开和合同检查同轮通过；
- 交付实际重新部署 Studio 与 Player，两个 deploy 目录保留可执行程序、DLL 和资源。

以后任何 Studio delivery build 继续强制执行这两项真实模板应用检查；单独包播放、静态缩略图或
直接加载模板不能替代它。
