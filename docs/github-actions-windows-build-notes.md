# GitHub Actions Windows 构建说明与记录

## 背景

当前项目目标是在 Windows 上复刻 Maccy，并且优先追求：

- 打包后体积尽量小
- 系统集成能力尽量强
- 在本地没有 Windows 构建环境时，也能通过远程 CI 验证

由于当前本地开发环境是 macOS，无法直接完成完整的 Win32 应用构建，所以需要尽早补上 GitHub Actions 的 Windows 远程构建链路。

## 参考来源

本次配置参考了源项目中的 macOS 构建工作流：

- 源项目路径：`/Users/opera/Documents/my-repositories/Maccy`
- 参考文件：`/Users/opera/Documents/my-repositories/Maccy/.github/workflows/build-macos.yml`

参考点主要包括：

- 将“远程构建”作为正式工程能力，而不是临时脚本
- workflow 内完成构建、打包、上传 artifact
- 让 CI 结果可以直接用于测试分发

## 当前 Windows 工作流

当前已新增文件：

- `.github/workflows/build-windows.yml`

当前 workflow 行为如下：

- 触发方式：
  - `push`
  - `pull_request`
  - `workflow_dispatch`
- 运行环境：
  - `windows-2022`
- 构建方式：
  - `CMake`
  - `Visual Studio 17 2022`
  - `x64`
  - `Release`
- 执行步骤：
  - checkout 仓库
  - 配置 CMake
  - 编译 Release
  - 运行 `ctest`
  - 执行 `cmake --install`
  - 生成 `maccy-windows-x64.zip`
  - 上传 GitHub Actions artifact
  - 如果当前触发是 `tag push`，则自动创建或更新对应 GitHub Release
  - 将 `maccy-windows-x64.zip` 上传为 Release asset

## 为什么这样配置

这次最初没有直接照搬源项目的发布模式，而是先做了更适合当前阶段的简化版本。

原因如下：

- 源项目当前是成熟的 macOS 应用，已经走到 tag 构建和 release 资产发布阶段
- 当前 Windows 项目还处于骨架期，先保证“每次 push 都能远程构建”更重要
- 现在先做 artifact 上传，能最快验证：
  - Windows runner 是否能编译
  - CMake 结构是否合理
  - 基础测试是否能跑
  - 安装布局是否可用

现在已经补上第一版 Release 发布能力，但仍然保持当前阶段的轻量策略：

- 普通 `push` / `pull_request` 继续只做构建验证和 artifact 上传
- `tag push` 在构建成功后自动发布 GitHub Release
- 当前 Release asset 仍然是基础 zip 包，不是安装器

## 当前配套改动

为了让 GitHub Actions 的 Windows 构建能真正落地，这次同时补了这些内容：

- 新增 `.gitignore`
- 在 `src/CMakeLists.txt` 中加入 `install(TARGETS ...)`
- 在 `README.md` 中补充 CI 说明
- 初始化当前目录的 git 仓库
- 配置远程仓库：
  - `origin = https://github.com/xcy960815/Windoscy.git`

## 当前状态

本地已经验证通过的部分：

- `cmake` 配置通过
- `core` 层可在当前 macOS 环境编译
- `ctest` smoke test 通过

当前尚未在本地直接验证的部分：

- Win32 可执行程序编译
- 托盘、热键、剪贴板监听等 Windows 专属能力

这些部分需要依赖 GitHub Actions 的 Windows runner，或者真实 Windows 开发环境继续验证。

## 与源项目当前 workflow 的差异

源项目 `build-macos.yml` 当前已经具备这些能力：

- 以 `tag` 为主要发布触发器
- 构建可分发的 macOS app zip
- 上传到 GitHub Release
- 自动更新 `appcast.xml`

当前 Windows workflow 已经补上的能力：

- tag 发布
- 自动创建 GitHub Release
- 自动上传 release asset

当前仍然没有做这些事情：

- 安装器生成
- 自动更新元数据
- 代码签名

## 当前已知限制

### 1. 产物仍然只是基础 zip 包

当前上传的是：

- `maccy-windows-x64.zip`

它适合做测试验证，但还不是正式安装体验。

### 2. 还没有签名

当前没有做：

- Windows 代码签名
- SmartScreen 相关优化

后续如果要正式分发，这一项迟早要补。

### 3. Release 目前仍然是基础发布链路

当前已经支持通过 tag 自动创建 GitHub Release，并上传：

- `maccy-windows-x64.zip`

但现在仍然属于“基础发布链路”，还没有：

- 安装器
- 签名
- 更完整的版本说明生成

### 4. 目前只做 `x64`

首版暂不考虑：

- `arm64`
- 多架构矩阵

这样可以先把主路径打通，减少 CI 变量。

## 建议的后续演进

建议按下面顺序继续补：

### 第一阶段：保持当前 push / PR 构建

先确保：

- Windows runner 稳定编译
- 产物结构稳定
- 基础测试持续可跑

### 第二阶段：增强 tag 发布

当前已经支持：

- tag 触发
- 自动创建 GitHub Release
- 自动上传 `zip`

后续可以继续补：

- release notes 自动生成
- pre-release / stable 发布策略
- 多架构产物命名规范

### 第三阶段：生成安装器

当项目从“测试产物”进入“可分发版本”阶段后，再评估：

- `WiX`
- `Inno Setup`
- `NSIS`

### 第四阶段：补签名与更新链路

如果后续做正式分发，需要继续补：

- 代码签名
- 自动更新元数据
- 版本产物命名规范

## 本次记录结论

这次已经把“Windows 版项目依赖 GitHub Actions 远程构建”正式纳入工程结构。

当前阶段的定位不是“发布系统已经完成”，而是：

- 先把 Windows 远程编译能力建起来
- 先让每次改动都能被 Actions 验证
- 在 tag 场景下，先具备最小可用的 Release 发布能力

这与源项目当前更成熟的发布链路相比，属于更早期、更轻量的一步，但方向是一致的。
