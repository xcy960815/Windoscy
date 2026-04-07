# ClipLoom 项目说明（中文）

更新时间：2026-04-08

## 项目简介

ClipLoom 是一个 Windows 原生剪贴板管理器，目标是提供稳定的历史记录能力、快速检索体验，以及高效的键盘操作流。

核心定位：

- 常驻托盘
- 低干扰、快速唤起
- 以搜索和快捷键为主的操作模型

## 当前能力

- 剪贴板监听与历史持久化
- 文本、HTML、RTF、图片、文件列表捕获
- 精确 / 模糊 / 正则 / 混合搜索
- 搜索高亮
- 托盘图标与托盘快捷操作
- 全局快捷键打开历史面板
- 可选“双击修饰键”打开
- 预览面板
- 置顶、置顶重命名、置顶纯文本编辑
- 忽略规则、开机启动、单实例运行

## 快速开始（使用发布包）

1. 下载 Release 里的 `cliploom.zip`
2. 解压到本地目录
3. 运行 `ClipLoom.exe`
4. 使用默认快捷键 `Ctrl+Shift+C`（或你的自定义触发方式）打开历史面板

## 本地构建（Windows）

前置条件：

- Windows 10/11 x64
- Visual Studio 2022（Desktop development with C++）
- CMake 3.25+

构建与测试：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DMACCY_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

打包：

```powershell
cmake --install build --config Release --prefix package
Compress-Archive -Path package\* -DestinationPath cliploom.zip
```

## 配置与数据目录

默认数据目录：

- `%LOCALAPPDATA%\ClipLoom`

兼容迁移逻辑：

- 如果检测到旧目录 `%LOCALAPPDATA%\MaccyWindows`，且新目录尚不存在，程序会继续使用旧目录，避免历史数据丢失

持久化内容包括：

- 历史记录数据
- 置顶状态与置顶编辑内容
- 弹窗尺寸/位置与显示偏好
- 捕获与忽略规则
- 开机启动偏好

## 项目结构

- `src/core`：与平台无关的核心逻辑
- `src/platform/win32`：Windows 平台能力封装（剪贴板、输入、启动项等）
- `src/app`：应用外壳、窗口、托盘、设置、弹窗
- `tests`：核心逻辑单元测试
- `docs`：设计决策、演进计划、工程文档

## 开发流程（推荐）

1. 从 `main` 拉分支
2. 以小步提交实现功能或修复
3. 本地构建并执行测试
4. 发起 PR 合并回 `main`
5. 确认 CI 通过后再合并

建议本地检查命令：

```powershell
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## 发布流程

当前采用 tag 驱动发布：

1. 将可发布代码合并到 `main`
2. 打版本 tag（格式 `v*`，例如 `v0.2.17`）
3. 推送 tag 后触发 `release-windows.yml`
4. CI 自动构建并上传 `cliploom.zip` 到 GitHub Release

相关工作流：

- `.github/workflows/build-windows.yml`
- `.github/workflows/release-windows.yml`

## 相关文档

- `README.md`
- `docs/tech-stack-decision.md`
- `docs/ui-upgrade-options.md`
- `docs/win32-app-split-todolist.md`
- `docs/github-actions-windows-build-notes.md`
