# 事件节点默认预设遗漏

2026-09-10 在文字节点集成中运行 `content_contracts`，发现此前增加的八种事件节点
没有默认预设。失败证据保留于 `out/p5-text-ui-content-tests.log`。这会使节点库的
默认预设覆盖不完整，底层事件计算测试通过不能证明内容库完整。

已补齐 input、beat、audio_onset、edge、envelope、step、gate、latch 的默认记录，
为预设读取增加有界 EventTrack 数据。检查通过记录为
`out/p5-event-defaults-tests.log`。新增文字默认项后共 200 条预设记录；这些默认项
不计入 P7 的高质量视觉预设验收数量。

遗漏原因是 Studio 交付入口只强制检查模板应用、保存发布及实际 GPU 切换，未包含
已有的默认预设覆盖检查。`tools/build-windows.py` 现在每次 Studio 交付（包括无须
重新编译的交付）构建 `content_contract_tests` 并要求 `content_contracts` 通过。

该修复不能替代实际视觉验收，也不表示所有节点已有可直接演出的优质预设。
