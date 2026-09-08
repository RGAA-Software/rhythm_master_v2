# R2 浮点、颜色、深度与景深交付

功能契约见 [颜色流程](../color_pipeline.md) 和 [深度流程](../depth_pipeline.md)。
本批使用现有 bgfx 后端，参考本地 GLM/TiXL 源码并保留 MIT 来源，没有新增后端。

## 交付范围

- 显式 RGBA8/RGBA16F/输入继承；精度与尺寸共同约束纹理缓存，浮点模糊保留高光。
- `texture.linearize`、`texture.display`：预乘 alpha 正确的 sRGB 转换、曝光、Reinhard。
- `scene.capture`、`scene.color`、`scene.depth`、`depth.linearize`、`texture.dof`：
  明确输出类型、相机深度元数据、按需附件、焦点控制和节点内预览。
- “频谱星云”24 节点采用浮点高光/柔光/SDR 显示；“频谱铸场”40 编辑节点、
  39 发布指令，4096 音柱和响度焦点。独立深度查看分支在发布时裁掉。
- Windows Studio/Player 已编译并由 Python 部署完整 20 DLL 与资源；Android
  34 个内置效果 APK 已覆盖安装，保留旧数据并通过内置 UI 选择和音乐播放。

## 短验证与实际结果

Windows D3D11 与 USB Redmi K40S / Adreno 650 / GLES 3.1 通过相同 GPU 检查：
半透明颜色往返、8 倍白色经过浮点曝光/模糊/SDR 映射；已知平面的透视/正交
和横竖尺寸深度；焦点/离焦棋盘与半透明 alpha。R0/R1 实例/计算检查同时通过。
资源失效、异设备、深度类型误用、预算和释放，图类型与运行时缓存/预览检查通过。

两作品使用实际解码 PCM，在相同场景时间比较音乐、静音、低频、高频：

| 作品 | 演示音乐/静音平均 RGB 差值 | 低频/高频差值 |
| --- | ---: | ---: |
| 浮点星云最终配色 | 10.0673 | 19.1984 |
| 浮点景深铸场 | 1.3751 | 0.5783 |

两作品均通过 120 帧带音频 MP4 导出、解码、时间戳、音频连续性/误差、重复结果一致、
静音差异与取消检查。原有实时演出和多段编排导出也通过回归。
浮点导出多帧读回曾出现 1 LSB 差异，当前采用一个在途 GPU 图像与独立异步编码队列；
原因尚未归到具体驱动缺陷，后续恢复并行需重新验证，见颜色流程中的记录。

日志：`out/dof-android-probe.log`、`out/r2-foundry-tests.log`、
`out/r2-nebula-final-tests.log`、`out/r2-export-regressions.log`。
图像：`out/r2-android-foundry.png`；实际渲染缩略图已写回两份模板。

## 交付文件

- Studio：`out/windows-release/src/windows_spike/deploy/rhythm_master.exe`
- Player：`out/windows-release/src/windows_player/deploy/rhythm_player.exe`
- APK：`out/android-arm64-release/apk/rhythm-player-release.apk`
  SHA256 `505177aec088bdc13ebad723ea13d1df6ecc9a804af5eabb1557bed2b24094ff`
- 铸场包 SHA256 `1a41c69197c90a574af407c81acc19cd5457405103cf411e49062166be8d91e3`
- 星云包 SHA256 `9edada6cde95dc1ab65ea9f04b3b1fb415161839657c139cf0adaa1da9464a92`

这是 R2 的功能闭环。视觉品质仍待审核，HDR 显示/编码、多层透明景深和任意设备性能
不在本次已验证声明中；R3–R6 与最终长稳验收继续推进。
