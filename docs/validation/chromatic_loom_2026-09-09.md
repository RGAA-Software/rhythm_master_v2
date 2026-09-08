# 织光机：可编辑音乐编织雕塑

日期：2026-09-09。`content/templates/chromatic_loom` 为高级品质候选，
206 节点、292 连线，1280×720，schema 5 / runtime ABI 3，面向 Windows/Android。

十根青蓝经线和十根金色纬线使用四份静态螺旋路径/管状网格。
路径节距等于两个线间距，奇偶相位交替，使基准构图的经纬在交点前后错开。
已有 GPU twist 负责音乐形变，避免逐帧重建路径；四十个金属线扣、深色边框、
两枚游走光梭与分组闪光组成完整画面。材质、环境光、阴影和 FXAA 均可编辑。
这不是布料/碰撞仿真，任意扩大参数不保证线条永不相交。

三个宏为音乐响应、编织节奏、曝光；Threading / Weaving / Shimmer 三套快照和
0、3、8、12 秒四段 Cue 构成 16 秒演出，复用项目原创双 FLAC 四片段编排。
图结构与资源生成来源见 `tools/author-chromatic-loom.py` 和
`provenance/chromatic_loom.json`，没有导入新的第三方算法或素材。

## 验证

首轮 188 节点结构可读但运动克制，增加两枚光梭与四组相位闪光后重新测试最终图。
真实 PCM、80 Hz、3500 Hz 对静音及两音调间平均 RGB 差分别为
0.7845、3.7355、15.9227、15.5347；206 指令全部可达，四份管状网格，
Windows 稳定纹理 49,350,852 B。证据 `out/r6-loom-music-final.log`。

最终实际 Studio 导出按钮完成 480 帧 H.264、非静音编排 PCM 及时间戳检查，
输出 `out/r6-loom-export/562922353350300/Exports/音画验收.mp4`。
已查看四时段实际导出画面 `out/r6-loom-motion.png`。

USB Redmi K40S / Adreno 650 通过最终包 60 帧有效输出、非黑像素、预算和设备重建。
Balanced 960×540 短测 420 帧取后 300 帧，同步 p50 16.3672 ms、p95 18.6743 ms，
稳定纹理 31,610,052 B。记录为 `out/r6-loom-{android,balanced}.log`，
不构成独立 GPU 计时或长时间温控保证。

最终缩略图来自实际 GPU 运行包，元数据保留包哈希，证据目录为
`out/catalog-thumbnails/82468227980d4003b26cb7145965fdbe`。
作品保持 `visual-review-pending`；完整作品候选变为 45 项，
作者分级为 6 基础、15 高级、24 功能示例。28 个组件与 189 条预设记录保持。

三项目录契约通过，Windows Studio/Player 完整部署资源及 20 个 DLL。
APK 覆盖安装成功；应用内搜索 Chromatic Loom 返回 1/45 项，选择后正确横屏、
16 秒配乐播放且 RMS 非零。手机搜索与画面分别记录在
`out/r6-loom-picker.xml`、`out/r6-loom-phone.png`。
