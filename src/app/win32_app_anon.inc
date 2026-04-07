/** 控制器窗口类名称 */
constexpr wchar_t kControllerWindowClass[] = L"ClipLoomController";
/** 弹窗窗口类名称 */
constexpr wchar_t kPopupWindowClass[] = L"ClipLoomPopup";
/** 固定编辑器窗口类名称 */
constexpr wchar_t kPinEditorWindowClass[] = L"ClipLoomPinEditor";
/** 设置窗口类名称 */
constexpr wchar_t kSettingsWindowClass[] = L"ClipLoomSettings";
/** 窗口标题 */
constexpr wchar_t kWindowTitle[] = L"ClipLoom";
/** 单实例互斥体名称 */
constexpr wchar_t kSingleInstanceMutexName[] = L"Local\\ClipLoomSingleInstance";
/** 激活已存在实例的消息 */
constexpr UINT kActivateExistingInstanceMessage = WM_APP + 2;
/** 双击修饰符触发消息 */
constexpr UINT kDoubleClickModifierTriggeredMessage = WM_APP + 3;
/** 设置窗口客户区宽度 */
constexpr int kSettingsClientWidth = 920;
/** 设置窗口客户区高度 */
constexpr int kSettingsClientHeight = 690;
/** 设置标签控件 ID */
constexpr int kSettingsTabControlId = 2100;
/** 设置保存按钮 ID */
constexpr int kSettingsSaveButtonId = 2101;
/** 设置应用按钮 ID */
constexpr int kSettingsApplyButtonId = 2102;
/** 设置关闭按钮 ID */
constexpr int kSettingsCloseButtonId = 2103;
/** 热键修饰符：Alt */
constexpr std::uint32_t kHotKeyModAlt = 0x0001;
/** 热键修饰符：Control */
constexpr std::uint32_t kHotKeyModControl = 0x0002;
/** 热键修饰符：Shift */
constexpr std::uint32_t kHotKeyModShift = 0x0004;
/** 热键修饰符：Win */
constexpr std::uint32_t kHotKeyModWin = 0x0008;

/**
 * @brief 弹窗打开触发器配置枚举
 */
enum class PopupOpenTriggerConfiguration {
  kRegularShortcut,  /**< 常规快捷键 */
  kDoubleClick,      /**< 双击修饰符 */
};

/**
 * @brief 热键选择结构体
 */
struct HotKeyChoice {
  std::uint32_t virtual_key = 0;  /**< 虚拟键码 */
  const wchar_t* label = L"";    /**< 显示标签 */
};

/** 弹窗热键可选键列表 */
constexpr HotKeyChoice kPopupHotKeyChoices[] = {
    {'A', L"A"},   {'B', L"B"},   {'C', L"C"},   {'D', L"D"},   {'E', L"E"},   {'F', L"F"},
    {'G', L"G"},   {'H', L"H"},   {'I', L"I"},   {'J', L"J"},   {'K', L"K"},   {'L', L"L"},
    {'M', L"M"},   {'N', L"N"},   {'O', L"O"},   {'P', L"P"},   {'Q', L"Q"},   {'R', L"R"},
    {'S', L"S"},   {'T', L"T"},   {'U', L"U"},   {'V', L"V"},   {'W', L"W"},   {'X', L"X"},
    {'Y', L"Y"},   {'Z', L"Z"},   {'0', L"0"},   {'1', L"1"},   {'2', L"2"},   {'3', L"3"},
    {'4', L"4"},   {'5', L"5"},   {'6', L"6"},   {'7', L"7"},   {'8', L"8"},   {'9', L"9"},
    {VK_F1, L"F1"},   {VK_F2, L"F2"},   {VK_F3, L"F3"},   {VK_F4, L"F4"},
    {VK_F5, L"F5"},   {VK_F6, L"F6"},   {VK_F7, L"F7"},   {VK_F8, L"F8"},
    {VK_F9, L"F9"},   {VK_F10, L"F10"}, {VK_F11, L"F11"}, {VK_F12, L"F12"},
    {VK_SPACE, L"Space"},
};

/** 键盘钩子目标实例指针 */
Win32App* g_keyboard_hook_target = nullptr;

/**
 * @brief 根据设置获取弹窗打开触发器配置
 * @param settings 应用程序设置
 * @return PopupOpenTriggerConfiguration 触发器配置
 */
PopupOpenTriggerConfiguration OpenTriggerConfigurationForSettings(const AppSettings& settings) {
  if (settings.double_click_popup_enabled && settings.double_click_modifier_key != DoubleClickModifierKey::kNone) {
    return PopupOpenTriggerConfiguration::kDoubleClick;
  }

  return PopupOpenTriggerConfiguration::kRegularShortcut;
}

/**
 * @brief 获取组合框当前选中索引
 * @param combo 组合框句柄
 * @param fallback_index 备用索引
 * @return int 当前选中索引或备用索引
 */
int GetComboSelection(HWND combo, int fallback_index);

/**
 * @brief 显示对话框
 * @param owner 所有者窗口
 * @param message 消息文本
 * @param flags 消息框标志
 */
void ShowDialog(HWND owner, std::wstring_view message, UINT flags) {
  const std::wstring text(message);
  MessageBoxW(owner, text.c_str(), kWindowTitle, MB_OK | flags);
}

/**
 * @brief 加载指定大小的应用图标
 * @param instance 应用程序实例
 * @param width 图标宽度
 * @param height 图标高度
 * @return HICON 加载的图标句柄
 */
HICON LoadSizedAppIcon(HINSTANCE instance, int width, int height) {
  if (instance != nullptr) {
    const auto icon = static_cast<HICON>(LoadImageW(
        instance,
        MAKEINTRESOURCEW(IDI_APP_ICON),
        IMAGE_ICON,
        width,
        height,
        LR_DEFAULTCOLOR | LR_SHARED));
    if (icon != nullptr) {
      return icon;
    }
  }

  return static_cast<HICON>(LoadImageW(
      nullptr,
      IDI_APPLICATION,
      IMAGE_ICON,
      width,
      height,
      LR_DEFAULTCOLOR | LR_SHARED));
}

/**
 * @brief 加载大尺寸应用图标
 * @param instance 应用程序实例
 * @return HICON 大图标句柄
 */
HICON LoadLargeAppIcon(HINSTANCE instance) {
  return LoadSizedAppIcon(instance, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON));
}

/**
 * @brief 加载小尺寸应用图标
 * @param instance 应用程序实例
 * @return HICON 小图标句柄
 */
HICON LoadSmallAppIcon(HINSTANCE instance) {
  return LoadSizedAppIcon(instance, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
}

/**
 * @brief 获取指定点所在显示器的工作区域
 * @param point 屏幕坐标点
 * @return RECT 工作区域矩形
 */
RECT MonitorWorkAreaForPoint(POINT point) {
  RECT work_area{0, 0, 1280, 720};

  const HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
  MONITORINFO info{};
  info.cbSize = sizeof(info);
  if (GetMonitorInfoW(monitor, &info) != FALSE) {
    work_area = info.rcWork;
  }

  return work_area;
}

/**
 * @brief 获取弹窗窗口样式
 * @return DWORD 窗口样式
 */
DWORD PopupWindowStyle() {
  return WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX;
}

/**
 * @brief 解析应用数据目录路径
 * @return std::filesystem::path 应用数据目录路径
 */
std::filesystem::path ResolveAppDataDirectory() {
  constexpr std::string_view kCurrentFolderName = "ClipLoom";
  constexpr std::string_view kLegacyFolderName = "MaccyWindows";

  const auto preferred_path = [](const std::filesystem::path& base) {
    const auto current = base / kCurrentFolderName;
    const auto legacy = base / kLegacyFolderName;
    std::error_code error;

    if (std::filesystem::exists(current, error)) {
      return current;
    }

    error.clear();
    if (std::filesystem::exists(legacy, error)) {
      return legacy;
    }

    return current;
  };

  wchar_t* local_app_data = nullptr;
  std::size_t length = 0;
  if (_wdupenv_s(&local_app_data, &length, L"LOCALAPPDATA") == 0 &&
      local_app_data != nullptr &&
      *local_app_data != L'\0') {
    const std::filesystem::path path = preferred_path(std::filesystem::path(local_app_data));
    std::free(local_app_data);
    return path;
  }

  if (local_app_data != nullptr) {
    std::free(local_app_data);
  }

  return preferred_path(std::filesystem::current_path());
}

/**
 * @brief 检查是否应使用中文界面
 * @return bool 是否使用中文界面
 */
bool ShouldUseChineseUi() {
  return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE;
}

/**
 * @brief 根据语言设置返回对应的文本
 * @param use_chinese_ui 是否使用中文
 * @param english 英文文本
 * @param chinese 中文文本
 * @return const wchar_t* 对应语言的文本
 */
const wchar_t* UiText(bool use_chinese_ui, const wchar_t* english, const wchar_t* chinese) {
  return use_chinese_ui ? chinese : english;
}

std::uint32_t ModifierFlagsForVirtualKey(DWORD virtual_key);

/**
 * @brief 获取双击修饰符键的显示标签
 * @param use_chinese_ui 是否使用中文
 * @param key 双击修饰符键
 * @return const wchar_t* 显示标签
 */
const wchar_t* DoubleClickModifierLabel(bool use_chinese_ui, DoubleClickModifierKey key) {
  switch (key) {
    case DoubleClickModifierKey::kAlt:
      return L"Alt";
    case DoubleClickModifierKey::kShift:
      return L"Shift";
    case DoubleClickModifierKey::kControl:
      return L"Ctrl";
    case DoubleClickModifierKey::kNone:
    default:
      return UiText(use_chinese_ui, L"Not Set", L"未设置");
  }
}

/**
 * @brief 从虚拟键码获取双击修饰符键
 * @param virtual_key 虚拟键码
 * @return DoubleClickModifierKey 双击修饰符键
 */
DoubleClickModifierKey DoubleClickModifierKeyFromVirtualKey(DWORD virtual_key) {
  return StandaloneDoubleClickModifierKey(ModifierFlagsForVirtualKey(virtual_key));
}

/**
 * @brief 获取双击修饰符录制器文本
 * @param use_chinese_ui 是否使用中文
 * @param key 当前修饰符键
 * @param recorder_enabled 录制器是否启用
 * @return std::wstring 录制器文本
 */
std::wstring DoubleClickModifierRecorderText(
    bool use_chinese_ui,
    DoubleClickModifierKey key,
    bool recorder_enabled) {
  if (key != DoubleClickModifierKey::kNone) {
    return DoubleClickModifierLabel(use_chinese_ui, key);
  }

  if (recorder_enabled) {
    return UiText(
        use_chinese_ui,
        L"Click here and press Ctrl, Alt, or Shift",
        L"点击这里后按下 Ctrl、Alt 或 Shift");
  }

  return DoubleClickModifierLabel(use_chinese_ui, DoubleClickModifierKey::kNone);
}

/**
 * @brief 构建托盘提示文本
 * @param use_chinese_ui 是否使用中文
 * @param capture_enabled 是否启用捕获
 * @param store 历史记录存储
 * @return std::wstring 托盘提示文本
 */
std::wstring BuildTrayTooltip(bool use_chinese_ui, bool capture_enabled, const HistoryStore& store) {
  std::wstring tooltip = L"ClipLoom";
  if (!capture_enabled) {
    tooltip += UiText(use_chinese_ui, L" (Paused)", L"（已暂停）");
  } else if (!store.items().empty()) {
    tooltip += L" - ";
    tooltip += win32::Utf8ToWide(store.items().front().PreferredDisplayText());
  }

  if (tooltip.size() > 100) {
    tooltip.resize(100);
    tooltip += L"...";
  }

  return tooltip;
}

/**
 * @brief 从托盘消息参数获取通知码
 * @param lparam 消息的 lparam
 * @return UINT 通知码
 */
UINT TrayNotifyCode(LPARAM lparam) {
  return LOWORD(static_cast<DWORD>(lparam));
}

/**
 * @brief 从托盘消息参数获取锚点坐标
 * @param wparam 消息的 wparam
 * @return POINT 锚点坐标
 */
POINT TrayAnchorPoint(WPARAM wparam) {
  const DWORD coordinates = static_cast<DWORD>(wparam);
  POINT point{};
  point.x = static_cast<LONG>(static_cast<short>(LOWORD(coordinates)));
  point.y = static_cast<LONG>(static_cast<short>(HIWORD(coordinates)));
  return point;
}

/**
 * @brief 获取虚拟键码对应的修饰符标志
 * @param virtual_key 虚拟键码
 * @return std::uint32_t 修饰符标志
 */
std::uint32_t ModifierFlagsForVirtualKey(DWORD virtual_key) {
  switch (virtual_key) {
    case VK_LMENU:
    case VK_RMENU:
    case VK_MENU:
      return kDoubleClickModifierFlagAlt;
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_SHIFT:
      return kDoubleClickModifierFlagShift;
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_CONTROL:
      return kDoubleClickModifierFlagControl;
    case VK_LWIN:
    case VK_RWIN:
      return kDoubleClickModifierFlagWin;
    default:
      return 0;
  }
}

/**
 * @brief 检查是否为键盘按键按下消息
 * @param message 消息类型
 * @return bool 是否为按键按下消息
 */
bool IsKeyboardKeyDownMessage(WPARAM message) {
  return message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
}

/**
 * @brief 检查是否为键盘按键释放消息
 * @param message 消息类型
 * @return bool 是否为按键释放消息
 */
bool IsKeyboardKeyUpMessage(WPARAM message) {
  return message == WM_KEYUP || message == WM_SYSKEYUP;
}

/**
 * @brief 获取弹窗热键标签
 * @param use_chinese_ui 是否使用中文
 * @param virtual_key 虚拟键码
 * @return const wchar_t* 热键标签
 */
const wchar_t* PopupHotKeyLabel(bool use_chinese_ui, std::uint32_t virtual_key) {
  for (const auto& choice : kPopupHotKeyChoices) {
    if (choice.virtual_key == virtual_key) {
      if (virtual_key == VK_SPACE) {
        return UiText(use_chinese_ui, L"Space", L"空格");
      }
      return choice.label;
    }
  }

  return UiText(use_chinese_ui, L"Unknown", L"未知");
}

/**
 * @brief 获取搜索模式标签
 * @param use_chinese_ui 是否使用中文
 * @param mode 搜索模式
 * @return const wchar_t* 搜索模式标签
 */
const wchar_t* SearchModeLabel(bool use_chinese_ui, SearchMode mode) {
  switch (mode) {
    case SearchMode::kMixed:
      return UiText(use_chinese_ui, L"Mixed", L"混合");
    case SearchMode::kExact:
      return UiText(use_chinese_ui, L"Exact", L"精确");
    case SearchMode::kFuzzy:
      return UiText(use_chinese_ui, L"Fuzzy", L"模糊");
    case SearchMode::kRegexp:
      return UiText(use_chinese_ui, L"Regexp", L"正则");
  }

  return UiText(use_chinese_ui, L"Mixed", L"混合");
}

/**
 * @brief 获取排序方式标签
 * @param use_chinese_ui 是否使用中文
 * @param order 排序方式
 * @return const wchar_t* 排序方式标签
 */
const wchar_t* SortOrderLabel(bool use_chinese_ui, HistorySortOrder order) {
  switch (order) {
    case HistorySortOrder::kLastCopied:
      return UiText(use_chinese_ui, L"Last Copied", L"最近复制");
    case HistorySortOrder::kFirstCopied:
      return UiText(use_chinese_ui, L"First Copied", L"最早复制");
    case HistorySortOrder::kCopyCount:
      return UiText(use_chinese_ui, L"Copy Count", L"复制次数");
  }

  return UiText(use_chinese_ui, L"Last Copied", L"最近复制");
}

/**
 * @brief 获取置顶位置标签
 * @param use_chinese_ui 是否使用中文
 * @param position 置顶位置
 * @return const wchar_t* 置顶位置标签
 */
const wchar_t* PinPositionLabel(bool use_chinese_ui, PinPosition position) {
  switch (position) {
    case PinPosition::kTop:
      return UiText(use_chinese_ui, L"Pins on Top", L"置顶项在上");
    case PinPosition::kBottom:
      return UiText(use_chinese_ui, L"Pins on Bottom", L"置顶项在下");
  }

  return UiText(use_chinese_ui, L"Pins on Top", L"置顶项在上");
}

/**
 * @brief 获取本地化的内容格式名称
 * @param use_chinese_ui 是否使用中文
 * @param format 内容格式
 * @return const wchar_t* 本地化的格式名称
 */
const wchar_t* LocalizedContentFormatName(bool use_chinese_ui, ContentFormat format) {
  switch (format) {
    case ContentFormat::kPlainText:
      return UiText(use_chinese_ui, L"Plain Text", L"文本");
    case ContentFormat::kHtml:
      return L"HTML";
    case ContentFormat::kRtf:
      return UiText(use_chinese_ui, L"Rich Text", L"富文本");
    case ContentFormat::kImage:
      return UiText(use_chinese_ui, L"Image", L"图像");
    case ContentFormat::kFileList:
      return UiText(use_chinese_ui, L"Files", L"文件");
    case ContentFormat::kCustom:
      return UiText(use_chinese_ui, L"Custom", L"自定义");
  }

  return UiText(use_chinese_ui, L"Custom", L"自定义");
}

/**
 * @brief 连接内容格式列表
 * @param use_chinese_ui 是否使用中文
 * @param item 历史记录项
 * @return std::wstring 格式列表字符串
 */
std::wstring JoinContentFormats(bool use_chinese_ui, const HistoryItem& item) {
  std::wstring joined;

  for (const auto& content : item.contents) {
    if (!joined.empty()) {
      joined += L", ";
    }

    joined += LocalizedContentFormatName(use_chinese_ui, content.format);
    if (!content.format_name.empty()) {
      joined += L':';
      joined += win32::Utf8ToWide(content.format_name);
    }
  }

  return joined;
}

/**
 * @brief 构建预览正文
 * @param use_chinese_ui 是否使用中文
 * @param item 历史记录项
 * @return std::wstring 预览正文
 */
std::wstring BuildPreviewBody(bool use_chinese_ui, const HistoryItem& item) {
  if (const auto text = item.PreferredContentText(); !text.empty()) {
    return win32::Utf8ToWide(text);
  }

  for (const auto& content : item.contents) {
    if (content.format == ContentFormat::kImage) {
      return std::wstring(UiText(use_chinese_ui, L"[Binary image payload: ", L"[二进制图像数据：")) +
             std::to_wstring(content.text_payload.size()) +
             UiText(use_chinese_ui, L" bytes]", L" 字节]");
    }
  }

  return win32::Utf8ToWide(item.PreferredDisplayText());
}

/**
 * @brief 构建预览文本
 * @param use_chinese_ui 是否使用中文
 * @param item 历史记录项
 * @return std::wstring 预览文本
 */
std::wstring BuildPreviewText(bool use_chinese_ui, const HistoryItem& item) {
  const std::wstring source_application = item.metadata.source_application.empty()
                                              ? std::wstring(UiText(use_chinese_ui, L"Unknown", L"未知"))
                                              : win32::Utf8ToWide(item.metadata.source_application);
  const wchar_t* pinned_text = UiText(use_chinese_ui, item.pinned ? L"Yes" : L"No", item.pinned ? L"是" : L"否");
  std::wostringstream preview;
  preview << UiText(use_chinese_ui, L"Title: ", L"标题：") << win32::Utf8ToWide(item.PreferredDisplayText())
          << L"\r\n";
  preview << UiText(use_chinese_ui, L"Source App: ", L"来源应用：") << source_application << L"\r\n";
  preview << UiText(use_chinese_ui, L"Copy Count: ", L"复制次数：") << item.metadata.copy_count << L"\r\n";
  preview << UiText(use_chinese_ui, L"First Seen Tick: ", L"首次记录时间戳：") << item.metadata.first_copied_at
          << L"\r\n";
  preview << UiText(use_chinese_ui, L"Last Seen Tick: ", L"最近记录时间戳：") << item.metadata.last_copied_at
          << L"\r\n";
  preview << UiText(use_chinese_ui, L"Pinned: ", L"已置顶：") << pinned_text;
  if (item.pin_key.has_value()) {
    preview << L" (" << static_cast<wchar_t>(*item.pin_key) << L")";
  }
  preview << L"\r\n";
  preview << UiText(use_chinese_ui, L"Formats: ", L"内容格式：") << JoinContentFormats(use_chinese_ui, item)
          << L"\r\n\r\n";
  preview << BuildPreviewBody(use_chinese_ui, item);
  return preview.str();
}

/**
 * @brief 宽字符高亮区间结构体
 */
struct WideHighlightSpan {
  int start = 0;   /**< 起始位置 */
  int length = 0; /**< 长度 */
};

/**
 * @brief 将 UTF-8 高亮区间转换为宽字符区间
 * @param utf8_text UTF-8 编码的文本
 * @param spans UTF-8 高亮区间列表
 * @return std::vector<WideHighlightSpan> 宽字符高亮区间列表
 */
std::vector<WideHighlightSpan> ToWideHighlightSpans(
    std::string_view utf8_text,
    const std::vector<HighlightSpan>& spans) {
  std::vector<WideHighlightSpan> wide_spans;
  wide_spans.reserve(spans.size());

  for (const auto& span : spans) {
    if (span.length == 0 || span.start >= utf8_text.size()) {
      continue;
    }

    const std::size_t clamped_length = std::min(span.length, utf8_text.size() - span.start);
    const int wide_start = static_cast<int>(win32::Utf8ToWide(utf8_text.substr(0, span.start)).size());
    const int wide_length = static_cast<int>(
        win32::Utf8ToWide(utf8_text.substr(span.start, clamped_length)).size());
    if (wide_length > 0) {
      wide_spans.push_back({wide_start, wide_length});
    }
  }

  return wide_spans;
}

/**
 * @brief 绘制文本段
 * @param dc 设备上下文句柄
 * @param x X 坐标
 * @param y Y 坐标
 * @param text 文本
 * @param text_color 文本颜色
 * @param background_color 背景颜色（可选）
 */
void DrawTextSegment(
    HDC dc,
    int x,
    int y,
    std::wstring_view text,
    COLORREF text_color,
    std::optional<COLORREF> background_color) {
  if (text.empty()) {
    return;
  }

  SetTextColor(dc, text_color);
  if (background_color.has_value()) {
    SIZE size{};
    GetTextExtentPoint32W(dc, text.data(), static_cast<int>(text.size()), &size);
    RECT background_rect{x, y, x + size.cx, y + size.cy};
    SetBkColor(dc, *background_color);
    ExtTextOutW(dc, x, y, ETO_OPAQUE, &background_rect, text.data(), static_cast<UINT>(text.size()), nullptr);
  } else {
    TextOutW(dc, x, y, text.data(), static_cast<int>(text.size()));
  }
}

/**
 * @brief 获取文本宽度
 * @param dc 设备上下文句柄
 * @param text 文本
 * @return int 文本宽度
 */
int TextWidth(HDC dc, std::wstring_view text) {
  if (text.empty()) {
    return 0;
  }

  SIZE size{};
  GetTextExtentPoint32W(dc, text.data(), static_cast<int>(text.size()), &size);
  return size.cx;
}

/**
 * @brief 从菜单命令获取搜索模式
 * @param command 菜单命令 ID
 * @return SearchMode 搜索模式
 */
SearchMode SearchModeFromMenuCommand(UINT command) {
  switch (command) {
    case 1020:
      return SearchMode::kExact;
    case 1021:
      return SearchMode::kFuzzy;
    case 1022:
      return SearchMode::kRegexp;
    case 1023:
    default:
      return SearchMode::kMixed;
  }
}

/**
 * @brief 从菜单命令获取排序方式
 * @param command 菜单命令 ID
 * @return HistorySortOrder 排序方式
 */
HistorySortOrder SortOrderFromMenuCommand(UINT command) {
  switch (command) {
    case 1031:
      return HistorySortOrder::kFirstCopied;
    case 1032:
      return HistorySortOrder::kCopyCount;
    case 1030:
    default:
      return HistorySortOrder::kLastCopied;
  }
}

/**
 * @brief 从菜单命令获取置顶位置
 * @param command 菜单命令 ID
 * @return PinPosition 置顶位置
 */
PinPosition PinPositionFromMenuCommand(UINT command) {
  return command == 1041 ? PinPosition::kBottom : PinPosition::kTop;
}

/**
 * @brief 从菜单命令获取历史记录限制
 * @param command 菜单命令 ID
 * @return std::size_t 历史记录限制数量
 */
std::size_t HistoryLimitFromMenuCommand(UINT command) {
  switch (command) {
    case 1050:
      return 50;
    case 1051:
      return 100;
    case 1053:
      return 500;
    case 1052:
    default:
      return 200;
  }
}

/**
 * @brief 设置复选框选中状态
 * @param window 复选框窗口句柄
 * @param checked 是否选中
 */
void SetCheckboxChecked(HWND window, bool checked) {
  if (window != nullptr) {
    SendMessageW(window, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
  }
}

/**
 * @brief 检查复选框是否选中
 * @param window 复选框窗口句柄
 * @return bool 是否选中
 */
bool IsCheckboxChecked(HWND window) {
  return window != nullptr && SendMessageW(window, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

/**
 * @brief 添加组合框项
 * @param combo 组合框窗口句柄
 * @param text 要添加的文本
 */
void AddComboItem(HWND combo, const wchar_t* text) {
  if (combo != nullptr) {
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
  }
}

/**
 * @brief 设置组合框选中项
 * @param combo 组合框窗口句柄
 * @param index 要选中的索引
 */
void SetComboSelection(HWND combo, int index) {
  if (combo != nullptr) {
    SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
  }
}

/**
 * @brief 获取组合框当前选中索引
 * @param combo 组合框窗口句柄
 * @param fallback_index 备用索引
 * @return int 当前选中索引或备用索引
 */
int GetComboSelection(HWND combo, int fallback_index) {
  if (combo == nullptr) {
    return fallback_index;
  }

  const LRESULT selection = SendMessageW(combo, CB_GETCURSEL, 0, 0);
  return selection == CB_ERR ? fallback_index : static_cast<int>(selection);
}

/**
 * @brief 获取搜索模式对应的组合框索引
 * @param mode 搜索模式
 * @return int 组合框索引
 */
int SearchModeComboIndex(SearchMode mode) {
  switch (mode) {
    case SearchMode::kExact:
      return 1;
    case SearchMode::kFuzzy:
      return 2;
    case SearchMode::kRegexp:
      return 3;
    case SearchMode::kMixed:
    default:
      return 0;
  }
}

/**
 * @brief 从组合框选择获取搜索模式
 * @param combo 组合框窗口句柄
 * @return SearchMode 搜索模式
 */
SearchMode SearchModeFromComboSelection(HWND combo) {
  switch (GetComboSelection(combo, 0)) {
    case 1:
      return SearchMode::kExact;
    case 2:
      return SearchMode::kFuzzy;
    case 3:
      return SearchMode::kRegexp;
    case 0:
    default:
      return SearchMode::kMixed;
  }
}

/**
 * @brief 获取排序方式对应的组合框索引
 * @param order 排序方式
 * @return int 组合框索引
 */
int SortOrderComboIndex(HistorySortOrder order) {
  switch (order) {
    case HistorySortOrder::kFirstCopied:
      return 1;
    case HistorySortOrder::kCopyCount:
      return 2;
    case HistorySortOrder::kLastCopied:
    default:
      return 0;
  }
}

/**
 * @brief 从组合框选择获取排序方式
 * @param combo 组合框窗口句柄
 * @return HistorySortOrder 排序方式
 */
HistorySortOrder SortOrderFromComboSelection(HWND combo) {
  switch (GetComboSelection(combo, 0)) {
    case 1:
      return HistorySortOrder::kFirstCopied;
    case 2:
      return HistorySortOrder::kCopyCount;
    case 0:
    default:
      return HistorySortOrder::kLastCopied;
  }
}

/**
 * @brief 获取置顶位置对应的组合框索引
 * @param position 置顶位置
 * @return int 组合框索引
 */
int PinPositionComboIndex(PinPosition position) {
  return position == PinPosition::kBottom ? 1 : 0;
}

/**
 * @brief 从组合框选择获取置顶位置
 * @param combo 组合框窗口句柄
 * @return PinPosition 置顶位置
 */
PinPosition PinPositionFromComboSelection(HWND combo) {
  return GetComboSelection(combo, 0) == 1 ? PinPosition::kBottom : PinPosition::kTop;
}

/**
 * @brief 获取历史限制对应的组合框索引
 * @param limit 历史限制数量
 * @return int 组合框索引
 */
int HistoryLimitComboIndex(std::size_t limit) {
  switch (limit) {
    case 50:
      return 0;
    case 100:
      return 1;
    case 500:
      return 3;
    case 200:
    default:
      return 2;
  }
}

/**
 * @brief 从组合框选择获取历史限制
 * @param combo 组合框窗口句柄
 * @return std::size_t 历史限制数量
 */
std::size_t HistoryLimitFromComboSelection(HWND combo) {
  switch (GetComboSelection(combo, 2)) {
    case 0:
      return 50;
    case 1:
      return 100;
    case 3:
      return 500;
    case 2:
    default:
      return 200;
  }
}

/**
 * @brief 连接多行文本
 * @param lines 文本行列表
 * @return std::wstring 连接后的多行文本
 */
std::wstring JoinMultilineText(const std::vector<std::string>& lines) {
  std::wstring joined;

  for (const auto& line : lines) {
    if (!joined.empty()) {
      joined += L"\r\n";
    }
    joined += win32::Utf8ToWide(line);
  }

  return joined;
}

/**
 * @brief 分割多行文本
 * @param text 多行文本
 * @return std::vector<std::string> 分割后的文本行列表
 */
std::vector<std::string> SplitMultilineText(const std::wstring& text) {
  std::vector<std::string> lines;
  std::wstring current;

  const auto flush_line = [&]() {
    while (!current.empty() && current.back() == L'\r') {
      current.pop_back();
    }

    if (!current.empty()) {
      lines.push_back(win32::WideToUtf8(current));
      current.clear();
    }
  };

  for (wchar_t ch : text) {
    if (ch == L'\n') {
      flush_line();
    } else {
      current.push_back(ch);
    }
  }

  flush_line();
  return lines;
}

/**
 * @brief 获取弹窗热键对应的组合框索引
 * @param virtual_key 虚拟键码
 * @return int 组合框索引
 */
int PopupHotKeyComboIndex(std::uint32_t virtual_key) {
  for (int index = 0; index < static_cast<int>(sizeof(kPopupHotKeyChoices) / sizeof(kPopupHotKeyChoices[0])); ++index) {
    if (kPopupHotKeyChoices[index].virtual_key == virtual_key) {
      return index;
    }
  }

  return 2;
}

/**
 * @brief 从组合框选择获取弹窗热键虚拟键码
 * @param combo 组合框窗口句柄
 * @return std::uint32_t 虚拟键码
 */
std::uint32_t PopupHotKeyVirtualKeyFromComboSelection(HWND combo) {
  const int index = GetComboSelection(combo, 2);
  if (index < 0 || index >= static_cast<int>(sizeof(kPopupHotKeyChoices) / sizeof(kPopupHotKeyChoices[0]))) {
    return 'C';
  }

  return kPopupHotKeyChoices[index].virtual_key;
}

/**
 * @brief 检查弹窗热键是否有效
 * @param modifiers 修饰符
 * @param virtual_key 虚拟键码
 * @return bool 热键是否有效
 */
bool IsValidPopupHotKey(std::uint32_t modifiers, std::uint32_t virtual_key) {
  return virtual_key != 0 && (modifiers & 0x000F) != 0;
}

/**
 * @brief 格式化弹窗热键文本
 * @param use_chinese_ui 是否使用中文
 * @param modifiers 修饰符
 * @param virtual_key 虚拟键码
 * @return std::wstring 格式化的热键文本
 */
std::wstring FormatPopupHotKey(bool use_chinese_ui, std::uint32_t modifiers, std::uint32_t virtual_key) {
  std::wstring text;

  if ((modifiers & kHotKeyModControl) != 0) {
    text += L"Ctrl+";
  }
  if ((modifiers & kHotKeyModAlt) != 0) {
    text += L"Alt+";
  }
  if ((modifiers & kHotKeyModShift) != 0) {
    text += L"Shift+";
  }
  if ((modifiers & kHotKeyModWin) != 0) {
    text += L"Win+";
  }

  text += PopupHotKeyLabel(use_chinese_ui, virtual_key);
  return text;
}

/**
 * @brief 描述打开触发器
 * @param use_chinese_ui 是否使用中文
 * @param settings 应用程序设置
 * @return std::wstring 触发器描述
 */
std::wstring DescribeOpenTrigger(bool use_chinese_ui, const AppSettings& settings) {
  if (OpenTriggerConfigurationForSettings(settings) == PopupOpenTriggerConfiguration::kDoubleClick) {
    return std::wstring(UiText(use_chinese_ui, L"double-click ", L"双击 ")) +
           DoubleClickModifierLabel(use_chinese_ui, settings.double_click_modifier_key);
  }

  return FormatPopupHotKey(use_chinese_ui, settings.popup_hotkey_modifiers, settings.popup_hotkey_virtual_key);
}

/**
 * @brief 读取窗口文本
 * @param window 窗口句柄
 * @return std::wstring 窗口文本
 */
std::wstring ReadWindowText(HWND window) {
  if (window == nullptr) {
    return {};
  }

  const int length = GetWindowTextLengthW(window);
  std::wstring text(static_cast<std::size_t>(length + 1), L'\0');
  if (length > 0) {
    GetWindowTextW(window, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
  } else {
    text.clear();
  }

  return text;
}

/**
 * @brief 通知已存在的实例激活
 * @return bool 是否成功通知
 */
bool NotifyExistingInstance() {
  for (int attempt = 0; attempt < 20; ++attempt) {
    const HWND existing_window = FindWindowW(kControllerWindowClass, nullptr);
    if (existing_window != nullptr) {
      PostMessageW(existing_window, kActivateExistingInstanceMessage, 0, 0);
      return true;
    }

    Sleep(100);
  }

  return false;
}

/**
 * @brief 关闭旧版本实例
 */
void CloseLegacyInstances() {
  HWND existing_window = nullptr;
  while ((existing_window = FindWindowExW(nullptr, existing_window, kControllerWindowClass, nullptr)) != nullptr) {
    PostMessageW(existing_window, WM_CLOSE, 0, 0);
  }

  for (int attempt = 0; attempt < 20; ++attempt) {
    if (FindWindowW(kControllerWindowClass, nullptr) == nullptr) {
      return;
    }

    Sleep(100);
  }
}
