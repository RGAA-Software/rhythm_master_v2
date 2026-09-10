# 组件连续运动接入

2026-09-10。P7.2 的首个增量没有重复创建已有的旋涡、回声或粒子组件，而是把现有
11 个具备自主运动的输入处理组件统一改为使用 `time.phase`：流纹玻璃、浮雕、百叶、
镜像双重奏、折棱、蚀刻、运动回声、柔光、极坐标旋涡、音频虹膜和亮度窗。

每个组件继续以保存的 `pace` 常量连接 `time.phase.speed`，周期设为 1,000,000 秒。
这样音乐自然循环、静音、暂停、改速和显式定位沿用运行时已经定义的运动契约；不把
组件内部效果时钟重置为节目时间。组件的输入、公开参数、图展开、资源边界和 Player
发布方式没有改变。

验证：

- `out/component-motion-phase-tests.log`：组件、组件 IO、模板合同和内容合同共四项通过；
- `tools/build-windows.py --target studio_deploy --target player_deploy`：两套 deploy 更新，
  并重新通过九项 Windows 交付检查，日志为
  `out/windows-release/studio-delivery-tests.log.runs/1789034668775893500.log`；
- `out/component-motion-ui-tests.log`：组件库交互、英文和中文时序 UI、组件交互四项通过。

这是 30 个现有组件的连续性升级，不把候选组件数错误记为增加。下一批从四件作品提取
相机绕行、循环穿行、分层旋转和粒子能量调制中尚未具备独立公开接口的结构，目标仍为
至少 40 个具有独立用途和真实动态验证的组件。
