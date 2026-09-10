# 高级作品连续粒子生成验证

范围：aureate_vortex、porcelain_bloom、stratified_ink、lumen_corridor 和
dunhuang_ribbons 的 Windows 作品修订。

## 用户报告的问题与根因

初版将音频 onset 映射到 burst，即使 GPU 粒子设置了 initial_fill=0，每次 onset
仍一次写入一批粒子。它表现为首批/节拍时突然喷满，随后等待下一次 burst，不符合
“每秒连续生成、音乐提高速率”的要求。

同一版还在五件作品中都加入 point.physics2d。这把“存在物理能力”误解为“每个作品
都需要刚体”，削弱了墨流、瓷器和建筑光廊自身的材质与空间逻辑。

移除 burst 后，敦煌作者图遗留两个未接入输出的事件节点。运行包正确裁剪为 170 条
可达指令，而作者图声明为 172，review-concept-motion.py 正确以
music.full_graph_not_reachable 拒绝检查。

## 修复后的结构

- 所有五件作品的 GPU 粒子都以 initial_fill=0 和正的 emission_rate 开始。
  emission 每帧作为每秒速率的乘数；低、中、高频支路持续提高速率和流场，不再接入
  burst 端口。
- 鎏光、瓷金、墨流、光门均没有 2D 刚体。敦煌保留 384 个边界碰撞金箔刚体，因为这
  是该作品明确的材质动作；它同样用连续 emission，低频只提高重力和释放速率。
- tools/concept_work_common.py 现在在生成阶段强制：
  1. 全部作者节点可反向到达最终输出；
  2. GPU 粒子从空场连续生成且禁止 burst；
  3. 点粒子禁止 burst；
  4. 除敦煌外禁止概念作品引入 point.physics2d；
  5. 每个 GPU 粒子层必须经过带 point_size_scale 输入的 gpu.map，使节拍立即作用于
     已存在的粒子，而非只等待新粒子生成。
  任一违反都会在生成内容前失败。

## 后续节奏感修订

连续发射修复后，真实 PCM 像素差异虽通过，但用户的视觉评审指出节奏感仍弱。根因是
仅改变发射速率会被粒子寿命平均。当前图在保留连续速率的同时，将低/中/高 onset
包络接到现存粒子的 point_size_scale、流场和短辉光曝光；瓷金花瓣和光门建筑的
低频呼吸幅度也同步提高。当前重点检查日志为
out/aureate-rhythm-pulse-quality 和 out/dunhuang-rhythm-pulse-quality。

## 当前证据

- 当前 delivery：out/windows-release/studio-delivery-tests.log.runs/1789054281336765200.log；
  9/9 通过，包含中英文 Studio Advanced 模板应用、保存、重开、发布与当前输出检查。
- 连续帧审查：out/concept-motion/51d59b876ffd4ad1819f74bd1f8b5412；五件均通过静音/
  音乐持续运动和 16 秒循环边界检查。silence-contact.png 是逐件 0–16 秒的人工查看
  证据。
- 当前真实 PCM 分频检查：out/aureate-rate-music-quality、
  out/porcelain-rate-music-quality、out/stratified-rate-music-quality、
  out/lumen-rate-music-quality 和 out/dunhuang-rate-music-quality。每个低/中/高
  夹具都产生超过 0.15 的画面差异；onset 日志显示各夹具只激活其预期频段。
- 当前缩略图：out/five-advanced-rate-thumbnails.log.runs/1789054758098867700.log；
  五张图均由当前 Windows D3D11 包实际渲染，记录在各模板 thumbnail.json。

每次修改这五件作品都必须重新生成作者图、运行 audit-content-quality.py --check、
执行 build-windows.py --target studio_deploy --target player_deploy，然后以
verify_windows.py 运行分频 PCM 检查与 review-concept-motion.py。旧包、单张图或
只检查静态文本都不能证明持续生成与模板应用正确。
