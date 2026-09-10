# 概念作品运动组件提取

2026-09-11。P7.2 的第二个增量从已完成的四件高级作品及敦煌飞带中提取五个
可编辑的第一方语义组件，使后续作品可复用经过实际动态验证的运动结构，而不复制
整个模板或将声音作为唯一动画来源。

| 组件 | 来源 | 持续自主运动 | 三频段调制 | 公开编辑项 |
| --- | --- | --- | --- | --- |
| 环绕光冕 / Orbital aureole | 鎏光流涡 | 反向光环旋转和相机绕行 | 低频发光、中频旋转、高频倾角 | 速度、响应、环半径、金属度、视场 |
| 动势花芯 / Kinetic blossom | 瓷金绽放 | 分层花芯反向转动和相机绕行 | 低频展开、中频釉光、高频细节 | 速度、响应、花瓣半径、粗糙度、视场 |
| 矿彩流移 / Mineral advection | 层叠墨流 | 多尺度噪声连续平流 | 低频亮度、中频位移、高频纹理能量 | 速度、响应、层数、颗粒尺度、采样范围 |
| 无尽光廊 / Endless passage | 光门空间 | 径向隧道持续前进 | 低频频谱主体、中频扭转、高频强调 | 速度、响应、纵深、门数、频谱透明度 |
| 飞带星图 / Ribbon constellation | 敦煌飞带 | 三条螺旋飞带与相机绕行 | 低频发光、中频路径相位、高频细节 | 速度、响应、金属度、粗糙度、视场 |

实现由 [author-motion-components.py](../../tools/author-motion-components.py) 生成。它只复用
本仓库四件概念作品的节点结构；没有导入第三方源码或资产。精确来源文件、哈希、
输出目录和复用说明记录在
[motion_components_p7_2.json](../../provenance/motion_components_p7_2.json)。

## 构建期问题与永久检查

第一次组件运行包构建拒绝了 `mineral_advection` 的 `noise_scale` 参数范围
`2..48`：`texture.noise` 的已注册上限为 32。随后又拒绝了 `orbital_aureole` 的
不存在属性 `tubular_segments`，正确的注册属性是 `tube_segments`。根因是作者工具未将
公开参数与 `Registry` 的实际属性范围逐项对齐。

修复后，所有新组件都必须经过运行包编译和 `semantic_tests`，而不是只检查
`graph.textproto` 可解析。该测试会加载整套组件目录、插入空工程、撤销/重做、连接、
预设往返、打包以及 Null Renderer 60 帧执行，因此能覆盖公开参数、组件展开和
运行包三处契约。

## 本轮实际验证

- `python tools/audit-content-quality.py`：作者组件数从 30 增至 35；审美审核数仍为 0，
  没有把运行通过误计为品质验收。
- `python tools/build-windows.py --target semantic_tests`：五个 `.rhythmpack` 由当前源编译；
  `semantic_tests.exe` 重新链接。
- `python tools/verify_windows.py --log out/motion-components-semantic-tests.log --
  out/windows-release/src/content/semantic_tests.exe out/windows-release/content/semantic
  out/motion-component-variants`：35 个组件均通过插入、历史、包往返和 60 帧执行，
  日志为 `out/motion-components-semantic-tests.log.runs/1789060624557354700.log`。
- `python tools/verify_windows.py --log out/motion-components-thumbnails.log -- python
  tools/render-catalog-thumbnails.py --kind semantic ...`：五个新组件均以 Windows D3D11
  Player 的第 4 秒合成音频实际渲染缩略图，并写入源内容目录；日志为
  `out/motion-components-thumbnails.log.runs/1789060633393753700.log`。

此项只建立可编辑组件候选和功能证据。它没有增加高级作品数量、没有声称用户审美
验收、没有执行 Android 或长稳验收。
