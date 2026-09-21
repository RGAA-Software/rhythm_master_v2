# 琉璃天门（Glazed Celestial Gate）交付验证 — 2026-09-21

P7.3 批次第二件高级作品。青绿琉璃环门、鎏金航标与黑曜石柱持续环行，相机绕行，
细尘/火花双层 GPU 粒子持续发射；低频推动门体呼吸，中频调节流场，高频经事件
包络点亮金色细节。最终图 344 节点，发布包 runtime ABI 5、344 指令、2 资产
（原创合成音乐 Luminous Concerto 双 stems）。

## 交付路径（全部经 `tools/verify_windows.py` 跨进程租约）

- `tools/build-windows.py` 交付构建 9/9 通过
  （`studio-delivery-tests.log.runs/1789965205134161900.log`）：中英文
  `template_switch_gpu` 自动枚举全部高级模板，琉璃天门经真实 Studio 搜索、
  选择、应用、保存、重开、发布与当前输出截取；合同、控件交付、媒体重开同轮通过。
- 七段 PCM 画质（`out/glazed-quality.log.runs/1789965520646576400.log`）：
  resonance/silence 1.62、low/silence 3.17、high/silence 34.81、mid/silence 2.03、
  quiet/loud 0.32，十项均超 0.15 下限；静音帧保持完整门体与双层粒子。
- 六控件极值（`out/glazed-controls.log.runs/1789965539023655800.log`）：
  1: 3.07、2: 16.57、3: 29.51、4: 3.06、5: 1.16、6: 0.64，全部超 0.15。
- 32 秒动态复核（`out/glazed-motion.log.runs/1789965579535348300.log`，证据
  `out/concept-motion/026e00006c9042c9be5cf8235b51a6c5/`）：音乐与静音各 961 帧，
  每 2 秒持续位移 11.4–35.2；16/32 秒周期接缝 1.5–1.7，与相邻帧行程同量级，
  无回收跳变；逐帧相机轨迹确认持续绕行。接触表逐帧查看：静音下环门旋转、
  柱阵环绕、粒子流动持续；音乐下金色闪烁随高频事件起伏且主运动不被取代。
- Studio 编排导出（`out/glazed-export.log.runs/1789965620984107300.log`）：
  真实导出按钮路径产出 480 帧 H.264 MP4，父窗口帧 p50 16.39 ms；静音编排导出
  （`.../1789965632886856000.log`）确认音轨能量为零。
- 音乐替换（`out/glazed-replace.log.runs/1789965644384699900.log`）：Studio 绑定
  新音乐、保存、发布、清除、重开后恢复原编曲，344 指令一致。
- GPU 目录缩略图按最终图重渲染（`out/glazed-thumbnail.log.runs/1789965507214641600.log`），
  包身份与源逐字节绑定（`content_identity`）。

## 首轮拦截与修复（保留中间失败）

1. **循环边界跳变**：首版 16 秒时钟下，环速度 13/−18/25/−8/33/−12°/s、相机 20 秒
   正弦、柱摆动 12 秒周期均不在 16 秒边界闭合，接缝差值约 15（与 2 秒行程同量级）。
   修复：`concept_work_common.start(clock_duration=16)` 增加周期参数，本作品用 32 秒；
   全部角速度量化为 11.25°/s（360/32）的整数倍，相机/摆动/浮动周期均整除 32。
   修复后接缝 1.5–1.7 ≈ 相邻帧行程。
2. **高频细节控件不可见**：控件极值检查首轮 control_6 差值 0.042（下限 0.15）。
   实测 demo 音乐在 4 秒捕获点的平滑高频段能量≈0（镲片 30ms 衰减落在分析窗之间），
   纯频段路径无论如何放大都不够；且大面积发射材质在色调映射压缩区，系数翻 4 倍
   像素仅动 1.5 倍。修复：高频 onset（42–62 频段）+ 长释放包络（release 2.0，
   sustain 0.4，稀疏事件间不归零），由高频控件直接门控，驱动金色材质、火花发射
   与全屏暖色微光层（`texture.composite` 的 amount 标量端口）。control_6 升至 0.637。
3. **导出复核宿主过期**：`export_ui_gpu_tests.exe` 还是 09-10 的旧二进制，
   对新图 Compile 报 `graph.template_invalid`；它不在 `build-windows.py` 的
   目标清单里，交付构建不会更新它。已把 `export_ui_gpu_tests` 加入清单并重建，
   之后编排/静音导出均通过。

## 关联

- 本轮首次交付构建还暴露了媒体 profile 错配（0461be3 的哈希测自另一检出的
  重建 SDK），已还原并记录在 `media_profile_revert_2026-09-21.md`。
- Android 实机复核与用户审美验收按 P7.5/P9 保留在最后阶段；本作品
  maturity 维持 `visual-review-pending`。
