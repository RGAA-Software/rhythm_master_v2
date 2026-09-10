# 敦煌飞天粒子与物理充实验证

范围：`dunhuang_ribbons` 的 Windows 优先内容修订。此增量只复用既有
`gpu.particles`、`gpu.map`、`point.emitter`、`point.physics2d`、`point.render` 和
`texture.trail`；没有新增渲染或物理运行时，也没有扩大 Android 宣称范围。

## 作品改动

- 背景为 49,152 个低速 GPU 星尘，独立流场与时间旋转保证静音持续运动。
- 前景为 16,384 个高速流光，和背景分层，避免单一抖动式颗粒。
- 金箔碎片使用 384 个盒形刚体、边界碰撞、反弹和摩擦；低频提高重力，高频提高释放。
  384 小于运行时 `point.physics2d` 的 512 点上限。
- 三层飘带和镜头环绕保持为主运动；音乐只调制流场、火花和碎片能量。

## 本次发现和永久检查

首次打包被 `rhythm_package` 正确拒绝：`texture.trail` 不声明
`texture_precision` 属性。该属性来自相邻纹理节点的错误套用，尚未写入运行包。
移除后必须重新生成图，并通过资源包编译；不能只检查作者脚本的 Python 语法。

首次持续运动检查还发现飘带路径和粒子映射在 16 秒相位边界没有走完整数圈，导致
可见跳变。路径速率现为每周期 1、2、3 整圈，粒子映射为 1 整圈；物理与 GPU 粒子
继续使用不回退的播放运动时钟。修订后必须重新运行第 5 项循环检查。

后续每次改动此作品须依次运行：

1. `python tools/author-concept-works.py --name dunhuang_ribbons`
2. `python tools/audit-content-quality.py --check`
3. `python tools/build-windows.py --target studio_deploy --target player_deploy`，其强制执行
   全部 Advanced 模板的 Studio 界面应用、ID 重映射、保存、重开和发布。
4. 以 161 条指令对当前 `dunhuang_ribbons.rhythmpack` 执行 `tools/test-music-gpu.py --quality`，
   检查静音、真实低中高频和响度输入的实际画面差异。
5. 运行 `tools/review-concept-motion.py --name dunhuang_ribbons`，检查静音和音乐下的持续
   运动、镜头轨迹以及 16 秒循环边界。

功能、音乐和视觉证据分别记录。静态截图或旧包播放都不能代替当前模板应用和移动输出审核。

## 本次结果

- 当前 Windows Studio/Player delivery：`out/particle-physics-delivery-1789048581813.stdout.log`；
  9/9 通过。中英文全 Advanced 模板 Studio 应用检查分别为 46.40 秒与 47.58 秒。
- 当前包音乐检查：`out/dunhuang-particle-physics-music.log.runs/1789048310628493200.log`；
  161 条可达指令、63 音频频段、2 个 GPU 粒子场。静音对真实音乐／低／中／高频的
  平均像素差依次为 28.58、33.15、19.26、41.25。
- 当前连续运动检查：`out/concept-motion/a6f77314daa845819e283712c4d7b7db`；音乐与静音
  均通过。静音 16 段两秒采样差为 29.09–42.11，循环相邻帧差为 8.43–8.65；音乐循环
  相邻帧差为 8.77–13.91，没有边界跳变。`silence-contact.png` 已作移动输出人工查看。
- 本次人工查看确认画面具有背景金尘、中心飘带、前景流光和金箔碎片四层，但不把
  工程自评或工具通过提升为用户的最终视觉验收。
