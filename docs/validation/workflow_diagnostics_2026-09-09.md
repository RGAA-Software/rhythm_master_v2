# P0 真实工作流诊断与串行检查

2026-09-09，接续整体计划 `b18ea53` 的 P0.2–P0.3。

`Studio::Workflow()` 发布 UI 线程上的值快照：当前编译请求代次、已安装计划代次、
图诊断代码、导出状态/阶段、已完成帧数及错误。没有暴露媒体、JSON、Protobuf 或
图形后端对象。测试 journal 在状态/动作变化时写入并 flush，导出临时工作目录释放
后仍保留证据。模板实际操作检查继续要求当前计划有效，而不是曾有过可运行计划。

导出后台另记 preparing、launching、rendering、finalizing、publishing、complete；
失败/取消保留最后到达阶段。finalizing 表示已观测全部编码帧、仍在等待工作进程完成，
不是对每个封装器内部步骤的跟踪。导出 UI 检查区分窗口关闭、超时和明确错误；
超时请求截图。完成必须由后台成功状态与已发布文件同时证明，文件存在本身不算成功。

复用现有 `ExportJobs`、进度文件、Studio 测试和标准库文件锁，没有新增第三方依赖。
`tools/verify_windows.py` 与 Windows 构建脚本共用项目输出下的 OS 文件锁；进程退出
自动释放所有权。直接 CTest/EXE 命令不受协作锁控制，因此工程规则要求使用入口。
锁不阻止手动使用应用，也不是跨进程应用运行协议，更不涉及暂停的通信模块。

每次执行保存独立 `*.log.runs/<timestamp>.log` 和 JSON 命令/返回码/耗时记录；
原日志路径仍指向最后一次结果供现有入口读取。不自动重试、不覆盖之前的失败档案。

验证：

- `python tools/test_verification.py`：独立进程互斥、等待超时、被杀进程自动释放、
  非零返回码保留、后一次成功不能擦除前一次失败，均通过。
- `export_jobs`：完成、取消、工作进程错误和已有目标拒绝；新增阶段检查通过，
  `out/p0-export-phase-tests.log`。
- 每次 Studio 交付的四项模板回归通过，`out/p0-diagnostics-delivery.log`。
- `ink_tide_export_ui_gpu`：真实按钮→480 帧 H.264；视频时间戳和 16 秒非静音
  音轨通过，`out/p0-export-ui-tests.log`。完整阶段 journal：
  `out/windows-release/ink-tide-export-ui/572299202698600/workflow.log`。
- `export_failure_ui_gpu`：真实 UI 启动导出，准确报告 `export.destination_exists`、
  保留 preparing 阶段与已有文件，截图和 journal 位于
  `out/windows-release/export-failure-ui/572305597318600/`。

此前波形增量中首次导出未完成的根因仍未确认。本次受控检查未复现，新增证据使后续
再次出现时可定位；不能把本次通过写成已证明此前失败原因。没有重装 Android，
本增量改动的是 Windows Studio/工具诊断，未改变运行包或共享媒体解码合同。

用法：

```powershell
python tools/build-windows.py --target rhythm_master
python tools/verify_windows.py --match '^(ink_tide_export_ui_gpu|export_failure_ui_gpu)$' --require ink_tide_export_ui_gpu --require export_failure_ui_gpu --log out/export-workflow-check.log
```

其他受影响 CTest 用例用 `--match`；独立工具用 `--log <path> -- <exe> <args...>`。
GPU 检查不与另一个直接启动的 GPU 用例混跑。
