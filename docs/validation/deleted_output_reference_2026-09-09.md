# 删除输出节点后无法重建输出

在 P4 从空图创作验收准备中发现：删除当前最终输出节点会移除节点和连线，
却保留 `Document::output_` 指向已删除 ID。`AddNode` 只在没有输出时绑定新建的
输出节点，因此重建链路后仍然编译失败。历史视图可能仍显示上次已接受计划，
不能把这个保留画面当作新图成功。

修复位于 GraphCanvas 现有删除事务：删除的恰为当前输出时同时清零引用。
不自动选择其他输出，也不重编号节点。新建输出沿用 AddNode 的既有规则；
撤销在同一次 History 事务中恢复输出节点、连线和原引用。

以前的画布删除测试只删除普通节点，缺少“删除输出→添加替代输出→连接→编译”
路径。永久增加 `canvas_interactions::ReplaceDeletedOutput`：通过真实 ImGui 鼠标
选择输出和 Delete 键删除，校验引用清除；用共享编辑命令添加、连接新输出并
确认编译成功，检查一次撤销恢复原输出。后半段是编辑命令合同，不冒充实际
Studio 鼠标连线或 GPU 像素验证。

修复前 `out/p4-delete-output-negative-tests.log` 失败，修复后
`out/p4-delete-output-fix-tests.log` 三项通过，含原有画布交互、组件和源码边界。
保留失败证据；后续空图/删除输出工作流必须带此回归。

`out/p4-delete-output-delivery.log` 已完成 Studio 增量构建、完整 Python deploy
及四项强制模板检查。Android 无编辑器删除路径，未改变 Player 运行合同。
