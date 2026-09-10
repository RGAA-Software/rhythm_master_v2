# 动态作品主运动：用户反馈与第三轮

用户确认第二轮外观已很接近目标，但指出运动仍主要是抖动：必须有持续的
流动、旋转、前进和摄像机运动，再叠加音乐调制。这已写入根 AGENTS.md 的
强制规则，不以单张截图、节点数或非零像素差替代动态验收。

## 原因与实施

- 0.2 的旋转只有每秒约 2–3 度，音乐却直接叠加角度；墨流噪声源静止，
  光门摄像机固定。主运动太弱，音乐位置偏移成为画面最明显的运动。
- Cue 同时改变速度控件，而作者时钟为 `time * pace`，速度变化会改变
  已走过的相位，甚至逆向。0.3 的默认 Cue 保持 pace=1，改由响应、曝光
  与局部细节表达段落。用户手动改速仍是显式时间重映射，不宣称已有积分时钟。
- 摄像机节点原本无标量输入。现在位置与目标六个轴都可连接时间信号，
  复用现有相机 View/Projection，实现真正的摄像机运动；未连接端口保持旧属性。
- 噪声节点补充有界 X/Y 格点位移，复用 TiXL 已导入的周期 Perlin 算法。
  移动生成域而非平移夹紧的成图，避免纹理接缝和边缘拖伸；默认位移为零。

| 作品 | 静音主运动 | 音乐调制 | 空间连续性 |
| --- | --- | --- | --- |
| 流涡 | 旋臂每秒转 22.5 度，两层 GPU 粒子流动，摄像机小范围绕行 | 细线／闪点亮度、发射、流场力度 | 几何与相机闭合；粒子保持随机生命周期，不宣称每帧像素循环一致 |
| 瓷金 | 瓷瓣分层每秒 22.5／−45 度转动，持续舒展，相机绕行 | 釉光与轻微展开 | 保留原有材质和金弧，主运动不由瞬时音量决定 |
| 墨流 | 在连续噪声格点上沿闭合轨迹迁移，多尺度形成不同位移速度 | 主带幅度、侵蚀与金边亮度 | 不对非周期图片做 fract 平移 |
| 光门 | 摄像机每秒前进 4.9 单位并缓慢横移／升降 | 门宽、梁高、灯光能量 | 门段在镜头后方 8 单位处回收到远端；地面和光源跟随 |

作者运动时钟限制在 16 秒周期，避免长期运行超过节点坐标／角度范围。
默认速度下所有确定性轨迹闭合；光门相机与世界一起重置坐标基准，投影关系
保持一致。必须看实际回收与 16／32 秒边界，不以公式推断无可见跳变。
暂停、用户主动拖动时间和显式手动时间重映射与普通连续播放分别说明。

## 验证路径

1. 图与运行时相机测试覆盖六个端口、连续／重复时间、后退时间及断线属性回退。
   首次测试编译误用了不存在的 EdgeId，保留失败日志并改为既有 uint64_t。
   `out/concept-motion-camera-build.log`、`out/concept-motion-camera-build-retry.log`、
   `out/concept-motion-camera-tests.log`：修复后两项通过。
2. 噪声位移测试覆盖带符号输入、有界钳位和零位移回退；
   `out/concept-motion-domain-build.log`、`out/concept-motion-domain-tests.log` 两项通过。
3. 新增实际 GPU `--motion` 路径，连续运行 32 秒，分别输入音乐和静音，
   保存每 2 秒图像、周期前后相邻帧和逐帧相机轨迹。它是动态评审证据，
   不自动等于审美通过。普通七 PCM／控件测试继续保留。
4. 实际 Studio 导出动态视频、模板应用／保存／发布回归和 Android 当前包短测。
   完整部署继续补齐 DLL；Android 只覆盖安装。长稳仍在最后阶段。

以下分轮保留失败与成功证据；交付以最终源码身份为准，不能引用 0.2 替代。

首轮静音 Studio 导出四件均通过，实际 480 帧／16 秒，音轨解码能量检查确认为
静音，而非仅静音播放器。`out/concept-motion-silent-export.log` 与
`out/concept-review/08f76e6df93c4df891a4578b8f5148b6/`。
已看每件 8 帧接触表，旋臂／瓷瓣明显持续转动，墨流地形迁移，光门有前进视差。

随后七 PCM 检查拦截流涡低频失联：删除音量角度偏移后，低频已没有剩余输出
路径，low/silence 差值为 0。保留 `out/concept-motion-quality.log`，没有降低阈值。
把低频重新接到细线／光点能量与粒子发射，保留独立主运动，不恢复角度抖动。
其他三件通过该轮七 PCM 检查；流涡必须修复重测后才可交付。

## 后续检查与修正

- 流涡修正后的七 PCM 十项差异检查通过：
  `out/concept-motion-quality-vortex-retry.log`，
  `out/concept-review/d715e8e0da4b4fc088a68b392fd97da0/`。
- 代码复核另外发现瓷金中频只接入了不再使用它的旋转表达式。
  即使频段夹具的泄漏使像素检查通过，也不能视为有效连接。
  移除无效旋转输入，中频明确控制壳面暖光；
  `out/concept-motion-porcelain-final-quality.log` 十项、
  `out/concept-motion-porcelain-final-controls.log` 三组控件极值均通过。
- 32 秒、音乐／静音两种输入的四件主运动检查通过：
  `out/concept-motion/b1df41ce70124655ae76b039d1368be1/`。
  该轮瓷金中频暖光修正前的证据不能替代最终音乐外观；静音主运动不变。
- 周期检查增加“周期前后帧差不得超过邻近普通单帧运动的两倍”门槛，
  同时保留人工图像检查，避免用两秒大位移掩盖单帧跳切。
  光门静音 479→480 帧差 11.29，480→481 为 10.15，未见复位跳切。
  另对实际静音 MP4 全部 480 帧计算相邻差异，中位数 12.81，最大 33.96；
  复核第 72–74 帧回收点、第 443–448 帧最大差异邻域和周期前三帧，
  是近处立柱经过镜头的连续视差。此数字不等价于感知质量评分。
  数据／图像：`out/concept-motion-first-preview/corridor-adjacent.json`、
  `corridor-recycling.png`，以及上述 motion 目录。
- 全部控件检查：`out/concept-motion-controls.log`；瓷金使用最终修正重测。
- 缩略图首次失败为旧 windows_effects_gpu_tests 不识别新 camera 输入，
  没有绕过 package.operator 检查。重编该目标和 render_contract_tests 后，
  四项核心测试通过，四件缩略图重新生成；最终记录
  `out/concept-motion-final-thumbnails.log`。
- 首次完整 Windows 交付九项检查通过：`out/concept-motion-delivery-build.log`。
  瓷金暖光与最终缩略图之后再次交付，结果以最终日志为准。

## 最终 Windows 交付与当前范围

- `out/concept-motion-final-deploy.log`：Studio／Player 完整 deploy，均含 20 DLL、
  可执行程序与资源；九项强制交付测试全部通过，129.75 秒。
- 当前源码音乐导出：`out/concept-review/1c530e20a4d34fe7bac566bd538366a7/`；
  真正静音输入导出：`out/concept-review/e6db4ec6e7204b08bf06b55c0e519403/`。
  八段均为实际 Studio 导出 480 帧／16 秒 H264/AAC，静音音轨解码能量已检查。
- 换曲、保存、发布、清空、重新打开四件全部通过：
  `out/concept-review/7e1ad3eaac56470d983ce4e1d0522a20/`。
- 人工查看四件最终音乐视频的八帧序列：旋臂轨迹、瓷瓣相对转动与相机视差、
  墨流形态迁移、光门前进均可见；不是仅围绕固定位置抖动。
  此自评不代替用户播放评审，也不撤销 0.2 已记录的材质／形态差距。
- 光门导出测试期间编辑器父窗口帧耗时 p50 41.57 ms、p95 50.42 ms；
  其他三件 p95 约 16.7–16.9 ms。前者尚未达到流畅编辑的性能目标，
  固定帧率离线视频不代表实时性能达标。
- Android 原生检查目标与 APK 编译成功，覆盖安装成功，保留应用数据。
  用户随后要求先看 Windows 效果、暂不处理 Android 画面，因此停止排队中的
  手机检查与后续 UI 操作。未将未完成的手机输出检查记为通过。
  日志：`out/concept-motion-final-android-build.log`、
  `out/concept-motion-final-android-install.log`、`out/concept-motion-android-vortex.log`。

本次只修订四件作品为 0.3，不增加库存数量，不宣称剩余规划完成。
手动改速相位连续性、其余库存的新运动规则复核、光门性能和手机画面评审仍待处理。

## 最终连续运动结果与预览

最终源码身份下，四件作品各完成音乐／静音 32 秒连续运行，新增单帧接缝门槛
全部通过：`out/concept-motion/66fd09712d6a4b0eb0b953dd0fb56220/`，
总入口 `out/concept-motion-final-continuity.log`。逐帧相机数据与多时刻图像均保留。

[第三轮实际音乐／静音对比页](../design/reviews/concept_works_v3_2026-09-10/index.html)
已生成并在本机浏览器打开。页面 evidence.json 绑定当前作者源码、导出视频哈希
及运动检查结果；视频位于本机 out/concept-works-v3-preview，不纳入仓库。
可用 tools/prepare-concept-motion-review.py 从匹配源码身份的成功证据重建页面。
HTML 所有图像／视频引用已检查存在。用户最终动态审美验收仍待反馈。
