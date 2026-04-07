/**
 * @file settings.h
 * @brief 应用程序设置结构体定义
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/double_click_modifier.h"
#include "core/history_store.h"
#include "core/search.h"

namespace maccy {

/**
 * @brief 忽略规则结构体
 * @details 定义了哪些应用程序、格式和内容模式应该被忽略
 */
struct IgnoreRules {
  bool ignore_all = false;                  /**< 是否忽略所有复制操作 */
  bool only_listed_applications = false;   /**< 是否只允许列表中的应用 */
  bool capture_text = true;                 /**< 是否捕获纯文本 */
  bool capture_html = true;                 /**< 是否捕获 HTML */
  bool capture_rtf = true;                  /**< 是否捕获 RTF */
  bool capture_images = true;               /**< 是否捕获图片 */
  bool capture_files = true;                /**< 是否捕获文件列表 */
  std::vector<std::string> ignored_applications;  /**< 忽略的应用程序列表 */
  std::vector<std::string> allowed_applications; /**< 允许的应用程序列表 */
  std::vector<std::string> ignored_patterns;    /**< 忽略的内容模式列表 */
  std::vector<std::string> ignored_formats;      /**< 忽略的格式列表 */
};

/**
 * @brief 弹窗设置结构体
 * @details 定义了历史记录弹窗的位置和显示选项
 */
struct PopupSettings {
  bool show_search = true;              /**< 是否显示搜索框 */
  bool show_preview = true;             /**< 是否显示预览面板 */
  bool remember_last_position = true;    /**< 是否记住上次位置 */
  bool has_last_position = false;        /**< 是否有上次位置记录 */
  int x = 0;                             /**< 弹窗 X 坐标 */
  int y = 0;                             /**< 弹窗 Y 坐标 */
  int width = 760;                      /**< 弹窗宽度 */
  int height = 420;                     /**< 弹窗高度 */
  int preview_width = 280;              /**< 预览面板宽度 */
};

/**
 * @brief 应用程序设置结构体
 * @details 包含所有用户可配置的应用程序选项
 */
struct AppSettings {
  std::size_t max_history_items = 200;    /**< 最大历史记录数量 */
  PinPosition pin_position = PinPosition::kTop; /**< 固定项位置 */
  HistorySortOrder sort_order = HistorySortOrder::kLastCopied; /**< 排序方式 */
  SearchMode search_mode = SearchMode::kMixed; /**< 搜索模式 */
  std::uint32_t popup_hotkey_modifiers = 0x0006; /**< 弹窗热键修饰符 */
  std::uint32_t popup_hotkey_virtual_key = 'C';  /**< 弹窗热键虚拟键码 */
  bool double_click_popup_enabled = false;        /**< 双击弹窗是否启用 */
  DoubleClickModifierKey double_click_modifier_key = DoubleClickModifierKey::kNone; /**< 双击修饰符键 */
  bool show_startup_guide = true;              /**< 是否显示启动指南 */
  bool capture_enabled = true;                  /**< 是否启用捕获 */
  bool auto_paste = true;                       /**< 是否自动粘贴 */
  bool paste_plain_text = false;                /**< 是否只粘贴纯文本 */
  bool start_on_login = false;                  /**< 是否开机启动 */
  bool clear_history_on_exit = false;           /**< 退出时是否清除历史 */
  bool clear_system_clipboard_on_exit = false;  /**< 退出时是否清除系统剪贴板 */
  PopupSettings popup;                          /**< 弹窗设置 */
  IgnoreRules ignore;                           /**< 忽略规则 */
};

/**
 * @brief 保存设置到文件
 * @param path 设置文件路径
 * @param settings 要保存的设置
 * @return bool 保存是否成功
 */
[[nodiscard]] bool SaveSettingsFile(
    const std::filesystem::path& path,
    const AppSettings& settings);

/**
 * @brief 从文件加载设置
 * @param path 设置文件路径
 * @return AppSettings 加载的设置
 */
[[nodiscard]] AppSettings LoadSettingsFile(
    const std::filesystem::path& path);

}  // namespace maccy
