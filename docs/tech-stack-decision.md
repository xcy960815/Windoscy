# Windows 版技术选型决策

日期：2026-04-06

## 结论

在“打包后体积最小”作为第一优先级的前提下，首版采用：

- `C++20`
- `Win32 API`
- `CMake`
- `MSVC`
- `SQLite`（建议后续直接使用 amalgamation 方式集成）

不采用首版主栈：

- `WinUI 3`
- `WPF`
- `Electron`

## 为什么这样选

这个项目本质上是一个强系统集成的常驻工具，核心能力几乎都贴着 Windows API：

- 监听和读写系统剪贴板
- 全局热键
- 托盘图标和菜单
- 弹出面板定位
- `SendInput` 自动粘贴
- 开机启动
- 前台窗口和来源进程识别
- 权限失败和兼容性兜底

如果把“安装包尽量小”放在第一位，原生 Win32 是最稳的路线：

- 不需要捆绑 .NET Desktop Runtime
- 不需要捆绑 Windows App SDK runtime
- 可以做成 unpackaged 的原生桌面程序
- 最终产物通常就是一个很小的原生 `exe` 加若干资源文件
- 对剪贴板、消息循环、热键、托盘、窗口样式的控制最直接

## 为什么不选 WinUI 3

`WinUI 3` 的 UI 体验和现代感更好，但它不适合当前这个“极限压体积”的目标。

主要原因：

- 它依赖 `Windows App SDK`
- 官方部署路径要么是 framework-dependent，要么是 self-contained
- self-contained 会把更多运行时内容带进发布物
- framework-dependent 则要求目标机器已有对应运行时
- 对一个剪贴板常驻工具来说，这是额外的体积和部署复杂度

对我们这个项目，`WinUI 3` 带来的收益，暂时抵不过它的分发成本。

## 为什么不选 WPF

`WPF` 是更现实的备选方案，开发速度会比原生 Win32 更快，但它依然不是“体积最小”的最优解。

主要原因：

- 需要 `.NET Desktop Runtime` 或自包含发布
- self-contained / single-file 发布会明显增大发布物
- `WPF` 的裁剪能力受限，进一步压缩体积的空间比原生程序小

如果我们的目标变成“开发速度优先，体积第二”，那 `WPF + Win32 interop` 会成为最合理备选。

## 首版打包策略

首版建议：

- 只做 `x64`
- 先做 unpackaged 发布
- 先交付 `zip` 版本
- 安装器放到后续阶段

这样做的原因：

- `x64` 覆盖面最大，能先把系统兼容问题打透
- unpackaged 最容易把体积控制在最低
- 不必在第一阶段就引入 MSIX 生态和额外运行时约束

后续如果需要更好的安装体验，再评估：

- `WiX`
- `Inno Setup`
- `NSIS`

## 编译与体积策略

Windows Release 构建建议默认按“体积优先”处理：

- 编译器优化以 size 为主
- 开启函数级和数据级裁剪
- 链接阶段开启未引用代码剔除和相同代码折叠
- 不把调试符号放进发布包
- 第三方依赖尽量少而精
- 优先系统库，避免引入重量级 UI 框架

建议目标：

- 首版核心产物尽量控制在“原生桌面工具”的量级
- 先把功能做对，再追求极限压缩

## 工程结构

建议分层如下：

- `src/core`
  - 纯 C++ 业务层
  - 历史项模型
  - 搜索
  - 去重
  - pin 分配
  - 忽略规则
- `src/platform/win32`
  - Win32 平台层
  - 剪贴板
  - 托盘
  - 全局热键
  - 窗口定位
  - `SendInput`
  - 开机启动
  - 来源应用识别
- `src/app`
  - 应用入口
  - 主消息循环
  - 面板窗口
  - 设置窗口
- `tests`
  - 先测 `core`
  - 平台层以人工回归和 Windows 集成测试为主

## 当前决定

从现在开始，默认主方案为：

- `C++20 + Win32 API + CMake`

备用方案保留为：

- `C# + WPF + Win32 interop`

只有当后续确认“开发速度比安装包体积更重要”时，才切换到备选方案。

## 参考资料

以下为本次决策参考的官方资料：

- Windows App SDK deployment overview
  - <https://learn.microsoft.com/windows/apps/package-and-deploy/deploy-overview>
- .NET deployment overview
  - <https://learn.microsoft.com/dotnet/core/deploying/>
- Single-file deployment
  - <https://learn.microsoft.com/dotnet/core/deploying/single-file/overview>
- Known trimming incompatibilities for desktop apps
  - <https://learn.microsoft.com/dotnet/core/deploying/trimming/incompatibilities>
