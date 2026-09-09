# 模板已切换但最终输出仍为默认画面

2026-09-09，用户实际验收发现。“墨潮”等模板的节点和标题已进入编辑器，
最终输出仍是默认渐变；界面提示“宏控件范围或快照引用无效”。

## 原因与验证遗漏

`editor::InstantiateTemplate` 为模板根节点分配新的稳定 ID，并同步改写连线、
最终输出、命名信号、绑定和布局，却遗漏 `control_titles_` 的键和
`control_snapshots_.values_` 的键。这些键也是节点 ID。
新图因此违反宏控件引用合同，编译报 `graph.controls`。Studio 保留之前可运行的
计划，于是显示“新节点图 + 旧渐变输出”。受影响的是通过模板选择器应用的含宏模板，
不是 Shader 或 GPU 算法失效。

此前的模板测试直接加载原始模板并发布、运行；原始 ID 自洽，因此通过。
浏览器测试仅检查选择和预览，不应用模板。部分完整 Studio 测试将模板预先保存为
工程后启动，也没有经过应用时的 ID 重映射。旧 `HasValidPlan()` 又只检查曾经有过
一个计划，无法证明当前图已编译成功。

因此，此前的结果只证明了被测路径，不能证明真实的模板切换流程可用。
这是测试覆盖和“成功”判据的遗漏，不能用用户环境或模板单独可播放来解释掉。

## 修复

- 应用模板时同步改写宏名称、快照值中的节点引用。Cue 指向的是快照 ID，保持不变。
  组件内部 ID 属于定义自身作用域，同样不参与根节点重映射。
- 重映射后的图再次编译校验，失败时拒绝编辑事务，防止节点图先被替换。
- Studio 记录已安装计划的编译代次；`HasValidPlan()` 必须对应当前编译请求且没有
  诊断，旧输出保留不能冒充新图验证成功。

## 永久回归

1. `editor_contracts`：含宏、快照、Cue 的模板使用新节点 ID 实例化，检查宏引用、
   Cue 采样和撤销；修复前明确复现 `template.remapped_controls_do_not_compile`。
2. `template_contracts`：遍历全部内置模板，连续应用到已有工程，故意使用不相交的
   高位 ID；检查编译、撤销/重做、媒体保留、保存重开、发布宏控件和 60 帧运行。
   此次覆盖 45 个模板，未来新增条目自动纳入，不能只跑已知出错的“墨潮”。
3. `template_switch_gpu_en-US`、`template_switch_gpu_zh-CN`：启动真实 Studio 默认
   工程，通过模板窗口搜索、选择并点击使用，依次切换墨潮、织光机、晶瓣合唱。
   检查当前编译计划、节点数量、资源预算、保存及发布，捕获实际编辑器最终输出。
   没有直接调用模板加载替代用户操作。
4. `tools/build-windows.py` 的默认或 Studio 交付构建自动构建并执行上述四项检查。
   无需重编译时也执行。脚本先核对四项测试均已注册；缺失或失败返回非零，不输出
   构建交付成功。结果写入构建目录 `studio-delivery-tests.log`。

受影响工作流的工程规则已写入 `AGENTS.md`。以后报告验证必须说明运行的是原始
模板、Player 包还是 Studio 的真实应用路径，不把它们互相替代。

## 本次结果与使用

Windows Release 增量编译及完整 Python 部署完成，Studio/Player 各带 20 个 DLL。
四项强制回归通过。部署路径：
`out/windows-release/src/windows_spike/deploy/rhythm_master.exe`。
重新启动该程序，在模板选择器重新应用模板即可；不需要修改模板素材或重新编写图。

日志：`out/template-switch-repro-build.log`、`out/template-switch-catalog-tests.log`、
`out/template-switch-ui-tests.log`、`out/template-switch-ui-english-tests.log`、
`out/template-switch-delivery.log`、构建目录 `studio-delivery-tests.log`。
再次执行无 C++ 重编译的 Studio 交付命令，四项仍实际运行并通过，耗时约 13.6 秒；
日志 `out/template-switch-noop-delivery.log`。最终 `windows_deploy_smoke` 同样通过。
中文实际截图位于 `out/windows-release/template-switch-gpu/567109645305500/`，
英文位于 `567129850076700/`；中文墨潮截图已查看，最终输出为墨色地形，宏控件正常。

这是 Windows Studio 模板应用与验证流程修复。没有改变播放器包格式、媒体解码或
渲染后端，没有因此重装 Android，也没有声称全部规划或品质验收已完成。
