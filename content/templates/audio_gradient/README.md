# 音乐渐变 / Music gradient

Windows Studio：播放音乐，在检查器的“音频输入”展开项点击“开始监听系统声音”。
音频特征节点输出规范响度，范围映射节点调整灵敏度，渐变节点实时响应。
默认不打开音频设备；没有音频时输出静态底色。此模板不附带或录制音乐文件。

Play music, then start system audio in the Windows Studio inspector. The audio
feature drives gradient amount through an editable range mapper. The capture
starts only on request. No music file is bundled or recorded.

The graph is portable; its current source adapter is Windows system loopback.
Android audio source integration and music-file playback are subsequent work.
