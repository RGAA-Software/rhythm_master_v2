# 异步保存期间读取工程的竞争

文字缓存复测触发 `ios_base::failbit set: iostream stream error`，保留于
`out/p5-text-persistent-cache-tests.log`。失败发生于字体导入后的异步保存／轮询，
最终磁盘仍有完整的字体、许可和新修订，因此不能靠重跑认定没有问题。

检查发现 `project_store.cpp` 先对路径取 `file_size`，随后另外打开 ifstream。
可原子替换的 `CURRENT` 在这两步之间不保证仍是同一个文件；普通 ifstream 在
Windows 上也未提供此处所需的删除共享合同。现在复用已有 `storage::FileBytes`，
长度和全部读取来自同一个打开的文件句柄，并允许发布者原子替换旧路径。各修订
仍按原有 SHA 和身份校验，不接受半份修订；没有删除再移动的回退。

回归增加同一工程 32 次后台保存与前台持续读取，检查每次都是完整可用修订。
第一次新测试误将旧 schema fixture 的内存结构与规范化后的加载结构直接比较，
记录为 `out/p5-current-read-lease-tests.log` 中 `persistence.contract:64`；修正
测试基准为首次保存／加载后的规范结构，未放宽实际内容断言。

`out/p5-current-read-canonical-tests.log` 的并发及既有持久化合同通过。
`out/p5-current-read-lease-tests.log` 中实际 Studio 文字创作、保存发布、重开与
音乐 GPU 路径通过。该修复不声称对任意进程原地修改文件提供内容快照；发布者
仍须通过原子替换发布不可变内容。

USB Android 的同一持久化测试也通过，记录为 `out/p5-current-read-android-tests.log`。
完整两端更新交付及应用截图复测见 [文字实施记录](../text_rendering.md)。
