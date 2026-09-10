# 导出进度读取与 Windows 原子发布共享冲突

共振拱廊 247 指令作品从 Studio 导出按钮执行 16 秒编排时失败，最后观察到
403/480 帧，错误为 `ios_base::failbit set: iostream stream error`。
原始 `out/p7-resonant_arcade-wide-export.log` 和
`out/p7-wide/resonant_arcade/export/659219992465000/workflow.log` 保留。
磁盘当时还有约 18 GB 可用；失败发生在 rendering 阶段，未发布不完整 MP4。

检查发现进度读取使用普通 `std::ifstream`，子进程则以原子替换更新进度文件。
Windows 发布者的 DELETE 访问与未共享删除访问的普通流冲突；这个可确定复现的
读取缺陷符合此次阶段和异常。原始失败未抓到内核句柄轨迹，不声称记录了其
全部线程时序，也不将任意文件错误都归为此问题。

修复复用 `storage::FileBytes`：一次打开保留一个不可变文件版本，Windows
共享删除访问，所有原生句柄继续私有 RAII。保留 8192 字节、JSON 深度及帧数／
纹理边界；不通过吞掉解析异常或工作进程错误来让任务继续。
请求和最终结果也使用相同有限读取合同。未新增文件后端或网络依赖。

新增 `export_job_metadata` 确定性测试持有与原子发布相同的 DELETE 访问：
确认旧 `ifstream` 无法打开，同路径的正式新读取能得到正确进度，不靠竞态重跑。
另查进度缺失、最终帧、真实工作进程错误、损坏／超大／越界元数据。
已有 `export_jobs` 继续覆盖完成、禁止覆盖目标、取消和子进程失败。

构建 `out/p8-export-metadata-build.log` 无新警告；检查
`out/p8-export-metadata-contracts.log` 三项（含音频 fixture）全部通过。
修复后同一共振拱廊源码的实际 Studio 导出按钮完成 480 帧 H264 有声 MP4，
`out/p8-export-metadata-arcade-ui.log`，产物
`out/p8-export-metadata/arcade/659630703758800/Exports/音画验收.mp4`。
父 UI 帧耗时 p50/p95 16.2783/16.8722ms，不是纯 GPU 时间。
最终完整 Windows Studio/Player deploy 和五项强制模板验收全部通过，
`out/p8-export-metadata-final-delivery.log`，检查 41.60 秒。

此前短导出多数未撞到共享窗口，且未模拟发布者持有 DELETE 访问，因而漏测。
永久保留文件合同测试和实际导出回归，原始失败不因后续成功而删除。
