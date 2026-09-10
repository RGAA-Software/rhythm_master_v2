# 实时节奏诊断面板验证

用户报告高级作品缺少节奏感时，原音频面板只能显示系统 WASAPI 捕获快照。模板配乐或
本地音乐会停止系统采集并改由 FilePlayback 驱动图运行时，因此面板可能显示“音频输入
已停止”或没有频谱，虽然作品实际仍在接收文件音频特征。这使问题无法在 UI 中区分为
输入缺失、频段能量不足或作品映射过弱。

修复后 AudioPanel 从 Frame() 读取与运行时相同的音频特征快照，并明确显示驱动源：

- 系统声音；
- 当前作品／本地音乐。

面板使用作品当前采用的三组规范频段：0–20 低频、21–41 中频、42–62 高频，并显示
onset 强度、响度、63 段原始频谱与 BPM。低/中/高分别用暖金、青色和品红色显示，方便
与高级作品中的低频主体动作、中频流场和高频闪点对应。

## 永久检查

- AudioPanel 只能从 Frame() 读取展示特征，不能单独从系统采集读取而与当前图不同步。
- 每次增加本地或模板音频路径时，要验证 Source 标签、三组频段、onset、频谱和 BPM
  都来自同一快照。
- Studio delivery 必须运行中英文模板应用路径，确保本面板的 locale 键完整并且模板
  的音频绑定、保存、重开、发布和当前输出均可用。

## 当前结果

out/windows-release/studio-delivery-tests.log.runs/1789059016510080800.log 运行 9/9
通过，其中 template_switch_gpu_en-US 和 template_switch_gpu_zh-CN 实际加载界面和全部
Advanced 模板。当前可执行程序为
out/windows-release/src/windows_spike/deploy/rhythm_master.exe。

## 分频公开控制验证

高级概念作品公开六项控制：总音乐响应、运动速度、曝光、低频主体、中频流场和高频细节。
Dunhuang Flying Ribbons 的当前运行包用真实解码 PCM 执行全部控制的最小/最大值检查：
out/dunhuang-band-controls。低频主体、中频流场和高频细节的平均像素差分别为
13.64、11.35、8.86，证明三个旋钮实际影响运行图，而不是仅保存界面元数据。
