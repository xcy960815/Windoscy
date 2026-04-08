# `win32_app.cpp` 拆分 TODO

日期：2026-04-08

## 背景

当前文件规模：

- `src/app/win32_app.cpp`：约 4000 行
- `src/app/win32_app.h`：约 600 行

当前单文件承担了过多职责：窗口创建、托盘菜单、设置页 UI、弹窗渲染、剪贴板事件、键盘钩子、Pin 编辑器、路径与持久化等。后续继续迭代会明显降低可维护性和回归效率。

## 拆分目标

- 将 `win32_app.cpp` 按职责拆分为多个实现文件
- 不改变对外接口（`Win32App` 头文件尽量稳定）
- 每个阶段都保持可编译、可回归、可回滚

## 当前进度（2026-04-08）

- [x] 新增 `src/app/win32_app_internal.h/.cpp` 承载共享 helper 与内部常量
- [x] 新增 `win32_app_lifecycle_tray.cpp` / `win32_app_popup_input.cpp` / `win32_app_message_handlers.cpp` / `win32_app_settings.cpp` / `win32_app_window_procs.cpp`
- [x] 已将原有 `.inc` 内容全部并入对应 `.cpp`，并删除所有 `.inc` 文件
- [x] 主文件 `src/app/win32_app.cpp` 收敛为入口与少量核心逻辑
- [ ] 下一步继续把模块边界从“职责拆分”推进到“语义拆分”，例如把 `popup`、`pin editor`、`tray`、`input` 再拆成更细的 `.cpp`

## 目标目录结构（拆分后）

- `src/app/win32_app.cpp`（入口与主循环、最小编排）
- `src/app/win32_app_lifecycle.cpp`（初始化/关闭、窗口注册、单实例）
- `src/app/win32_app_tray.cpp`（托盘图标、托盘菜单、菜单命令）
- `src/app/win32_app_settings.cpp`（设置窗口、控件同步、Apply 流程）
- `src/app/win32_app_popup.cpp`（弹窗显示/定位/列表/预览）
- `src/app/win32_app_pin_editor.cpp`（Pin 编辑窗口与交互）
- `src/app/win32_app_input.cpp`（全局键盘、双击修饰键、剪贴板更新）
- `src/app/win32_app_window_procs.cpp`（各窗口 `WndProc` 与消息分发）
- `src/app/win32_app_internal.h`（仅 app 内部使用的常量/内部声明）

## TODO 列表

### 阶段 0：准备与护栏

- [ ] 在 `docs` 记录拆分边界和文件职责（本文档）
- [ ] 在 `CMakeLists.txt` 里预留新增 `src/app/*.cpp` 编译入口
- [ ] 保证当前 `main` 基线可构建（避免在坏状态下开始切分）

验收标准：

- 本地 `cmake --build build` 通过
- 本地 `ctest --test-dir build` 通过

---

### 阶段 1：先抽纯工具函数（最低风险）

- [ ] 抽取匿名命名空间中的“无状态工具函数”到 `win32_app_internal.h/.cpp`
  - 文本/格式化辅助
  - 组合框/菜单映射辅助
  - 键位映射与描述辅助
- [ ] 保持 `Win32App` 成员函数定义暂不移动，只先减少主文件顶部噪音

验收标准：

- 行为无变化
- `win32_app.cpp` 行数明显下降（目标先降到 < 3200）

---

### 阶段 2：拆设置模块（高收益、低耦合）

- [ ] 将以下函数迁移到 `win32_app_settings.cpp`
  - `OpenSettingsWindow`
  - `CloseSettingsWindow`
  - `ApplySettingsWindowChanges`
  - `SetSettingsDoubleClickModifierSelection`
  - `SyncSettingsWindowControls`
  - `ShowSettingsPage`
  - `HandleSettingsWindowMessage`
  - `HandleSettingsDoubleClickModifierMessage`
- [ ] 保持 `Win32App` 成员和字段不改名，减少跨文件冲突

验收标准：

- 设置页所有交互回归通过（尤其双击修饰键录制）
- `win32_app.cpp` 继续下降（目标 < 2500）

---

### 阶段 3：拆弹窗与预览模块

- [ ] 将弹窗行为迁移到 `win32_app_popup.cpp`
  - `ShowPopup` / `HidePopup` / `PositionPopupNearCursor`
  - `RefreshPopupList` / `UpdatePreview` / `SyncPopupLayout`
  - 列表绘制与选中逻辑
- [ ] 将搜索输入消息处理一起迁移，保证局部闭包

验收标准：

- 弹窗开关、搜索、选中、预览、自动粘贴行为一致
- `win32_app.cpp` 目标 < 1800

---

### 阶段 4：拆托盘与输入事件模块

- [ ] 将托盘逻辑迁移到 `win32_app_tray.cpp`
  - `SetupTrayIcon` / `RemoveTrayIcon` / `UpdateTrayIcon` / `ShowTrayMenu`
- [ ] 将输入与监听迁移到 `win32_app_input.cpp`
  - `HandleGlobalKeyDown` / `HandleGlobalKeyUp`
  - `HandleDoubleClickModifierTriggered`
  - `HandleClipboardUpdate`
  - 热键注册/双击监听相关函数

验收标准：

- 托盘菜单命令与图标状态正常
- 热键与双击修饰键触发行为一致

---

### 阶段 5：拆窗口过程与生命周期收尾

- [ ] 将各 `Static*Proc` 与 `Handle*Message` 按模块归位
- [ ] `win32_app.cpp` 保留主入口和最小装配
- [ ] 更新 `src/app/CMakeLists`（或顶层 `src/CMakeLists.txt`）源文件列表
- [ ] 补充拆分说明到开发文档（模块边界图）

验收标准：

- 最终 `win32_app.cpp` 控制在 800~1200 行
- 全量测试通过，Windows 手工回归关键路径通过

## 回归检查清单（每阶段都跑）

- [ ] 全局热键打开弹窗
- [ ] 双击修饰键打开弹窗
- [ ] 托盘菜单各 toggle 有效
- [ ] 设置页 Apply/Save/Close 行为一致
- [ ] 搜索/预览/pin/删除/自动粘贴无回归
- [ ] 退出时持久化与下次启动读取正常

## 风险点

- 大量匿名命名空间函数跨模块复用，拆分时容易出现重复定义或链接冲突
- `WndProc` 分发关系复杂，稍有调整就可能破坏焦点和键盘消息
- 设置页子窗口控件通知链路容易在重构中被意外改坏

## 执行策略

- 每个阶段独立提交，不做超大“总提交”
- 每阶段都先迁移、再编译、再回归、再提交
- 遇到行为风险优先“保留旧路径 + 新路径开关”过渡
