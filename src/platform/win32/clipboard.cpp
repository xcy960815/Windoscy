#ifdef _WIN32

#include "platform/win32/clipboard.h"

#include <shellapi.h>
#include <shlobj_core.h>
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "platform/win32/utf.h"

namespace maccy::win32 {

namespace {

constexpr wchar_t kHtmlClipboardFormatName[] = L"HTML Format";
constexpr wchar_t kRtfClipboardFormatName[] = L"Rich Text Format";

std::string TrimTrailingNull(std::string value) {
  while (!value.empty() && value.back() == '\0') {
    value.pop_back();
  }
  return value;
}

std::optional<std::string> ReadBytesFromHandle(HANDLE clipboard_data) {
  if (clipboard_data == nullptr) {
    return std::nullopt;
  }

  const std::size_t size = GlobalSize(clipboard_data);
  if (size == 0) {
    return std::nullopt;
  }

  const auto* bytes = static_cast<const char*>(GlobalLock(clipboard_data));
  if (bytes == nullptr) {
    return std::nullopt;
  }

  std::string result(bytes, bytes + size);
  GlobalUnlock(clipboard_data);
  return result;
}

std::optional<std::string> ReadUtf8TextFromFormat(UINT format) {
  const HANDLE clipboard_data = GetClipboardData(format);
  if (clipboard_data == nullptr) {
    return std::nullopt;
  }

  const auto* wide_text = static_cast<const wchar_t*>(GlobalLock(clipboard_data));
  if (wide_text == nullptr) {
    return std::nullopt;
  }

  std::string result = WideToUtf8(wide_text);
  GlobalUnlock(clipboard_data);
  return result;
}

std::optional<std::string> ReadRawFormat(UINT format) {
  if (format == 0 || IsClipboardFormatAvailable(format) == FALSE) {
    return std::nullopt;
  }

  if (auto bytes = ReadBytesFromHandle(GetClipboardData(format)); bytes.has_value()) {
    return TrimTrailingNull(*bytes);
  }

  return std::nullopt;
}

std::optional<std::string> ReadFileListText() {
  if (IsClipboardFormatAvailable(CF_HDROP) == FALSE) {
    return std::nullopt;
  }

  const auto drop = static_cast<HDROP>(GetClipboardData(CF_HDROP));
  if (drop == nullptr) {
    return std::nullopt;
  }

  const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
  if (count == 0) {
    return std::nullopt;
  }

  std::string joined;
  for (UINT index = 0; index < count; ++index) {
    const UINT length = DragQueryFileW(drop, index, nullptr, 0);
    if (length == 0) {
      continue;
    }

    std::wstring path(static_cast<std::size_t>(length + 1), L'\0');
    DragQueryFileW(drop, index, path.data(), length + 1);
    path.resize(length);

    if (!joined.empty()) {
      joined.push_back('\n');
    }
    joined += WideToUtf8(path);
  }

  return joined.empty() ? std::nullopt : std::make_optional(joined);
}

std::string StripHtmlTags(std::string_view value) {
  std::string plain_text;
  plain_text.reserve(value.size());

  bool inside_tag = false;
  for (char ch : value) {
    if (ch == '<') {
      inside_tag = true;
      continue;
    }
    if (ch == '>') {
      inside_tag = false;
      plain_text.push_back(' ');
      continue;
    }
    if (!inside_tag) {
      plain_text.push_back(ch);
    }
  }

  return plain_text;
}

std::optional<std::size_t> ParseHtmlOffset(std::string_view value, std::string_view key) {
  const std::size_t key_position = value.find(key);
  if (key_position == std::string_view::npos) {
    return std::nullopt;
  }

  std::size_t number_start = key_position + key.size();
  while (number_start < value.size() &&
         (value[number_start] == ' ' || value[number_start] == '\t')) {
    ++number_start;
  }

  std::size_t number_end = number_start;
  while (number_end < value.size() && std::isdigit(static_cast<unsigned char>(value[number_end])) != 0) {
    ++number_end;
  }

  if (number_end == number_start) {
    return std::nullopt;
  }

  try {
    return static_cast<std::size_t>(std::stoull(std::string(value.substr(number_start, number_end - number_start))));
  } catch (...) {
    return std::nullopt;
  }
}

std::string_view ExtractHtmlFragment(std::string_view value) {
  if (const auto start = ParseHtmlOffset(value, "StartFragment:");
      start.has_value()) {
    if (const auto end = ParseHtmlOffset(value, "EndFragment:");
        end.has_value() && *start < *end && *end <= value.size()) {
      return value.substr(*start, *end - *start);
    }
  }

  const std::string_view start_marker = "<!--StartFragment-->";
  const std::string_view end_marker = "<!--EndFragment-->";
  const std::size_t start_position = value.find(start_marker);
  const std::size_t end_position = value.find(end_marker);
  if (start_position != std::string_view::npos &&
      end_position != std::string_view::npos &&
      start_position + start_marker.size() < end_position) {
    return value.substr(start_position + start_marker.size(), end_position - (start_position + start_marker.size()));
  }

  return value;
}

std::string ExtractRtfText(std::string_view value) {
  std::string plain_text;
  plain_text.reserve(value.size());

  for (std::size_t index = 0; index < value.size(); ++index) {
    const char ch = value[index];
    if (ch == '{' || ch == '}') {
      continue;
    }

    if (ch == '\\') {
      if (index + 1 >= value.size()) {
        break;
      }

      const char next = value[index + 1];
      if (next == '\\' || next == '{' || next == '}') {
        plain_text.push_back(next);
        ++index;
        continue;
      }

      std::size_t control_index = index + 1;
      while (control_index < value.size() && std::isalpha(static_cast<unsigned char>(value[control_index])) != 0) {
        ++control_index;
      }
      while (control_index < value.size() &&
             (std::isdigit(static_cast<unsigned char>(value[control_index])) != 0 || value[control_index] == '-')) {
        ++control_index;
      }
      if (control_index < value.size() && value[control_index] == ' ') {
        ++control_index;
      }

      index = control_index - 1;
      plain_text.push_back(' ');
      continue;
    }

    plain_text.push_back(ch);
  }

  return plain_text;
}

std::string BuildHistoryTitleFromMarkup(std::string_view markup) {
  return BuildHistoryTitleFromText(StripHtmlTags(ExtractHtmlFragment(markup)));
}

std::string BuildHistoryTitleFromRtf(std::string_view rtf) {
  return BuildHistoryTitleFromText(ExtractRtfText(rtf));
}

std::string BuildHistoryTitleFromFileList(std::string_view file_list) {
  std::vector<std::string> paths;
  std::string current;

  for (char ch : file_list) {
    if (ch == '\n') {
      if (!current.empty()) {
        paths.push_back(current);
      }
      current.clear();
      continue;
    }
    if (ch != '\r') {
      current.push_back(ch);
    }
  }

  if (!current.empty()) {
    paths.push_back(current);
  }

  if (paths.empty()) {
    return {};
  }

  std::string first_name = WideToUtf8(std::filesystem::path(Utf8ToWide(paths.front())).filename().wstring());
  if (first_name.empty()) {
    first_name = paths.front();
  }

  if (paths.size() == 1) {
    return first_name;
  }

  return first_name + " +" + std::to_string(paths.size() - 1) + " files";
}

std::string BuildHistoryTitleFromImage(std::string_view dib_payload) {
  if (dib_payload.size() >= sizeof(BITMAPINFOHEADER)) {
    const auto* header = reinterpret_cast<const BITMAPINFOHEADER*>(dib_payload.data());
    if (header->biWidth > 0 && header->biHeight != 0) {
      return "Image " +
             std::to_string(header->biWidth) +
             "x" +
             std::to_string(std::abs(header->biHeight));
    }
  }

  return "Image";
}

bool SetClipboardBytes(UINT format, std::string_view bytes, bool append_trailing_null = false) {
  const std::size_t size = bytes.size() + (append_trailing_null ? 1 : 0);
  HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE, size);
  if (global == nullptr) {
    return false;
  }

  void* memory = GlobalLock(global);
  if (memory == nullptr) {
    GlobalFree(global);
    return false;
  }

  if (!bytes.empty()) {
    std::memcpy(memory, bytes.data(), bytes.size());
  }
  if (append_trailing_null) {
    static_cast<char*>(memory)[bytes.size()] = '\0';
  }
  GlobalUnlock(global);

  if (SetClipboardData(format, global) == nullptr) {
    GlobalFree(global);
    return false;
  }

  return true;
}

bool SetClipboardWideText(std::string_view text) {
  const std::wstring wide_text = Utf8ToWide(text);
  const std::size_t bytes = (wide_text.size() + 1) * sizeof(wchar_t);

  HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (global == nullptr) {
    return false;
  }

  void* memory = GlobalLock(global);
  if (memory == nullptr) {
    GlobalFree(global);
    return false;
  }

  std::memcpy(memory, wide_text.c_str(), bytes);
  GlobalUnlock(global);

  if (SetClipboardData(CF_UNICODETEXT, global) == nullptr) {
    GlobalFree(global);
    return false;
  }

  return true;
}

bool SetClipboardFileList(std::string_view file_list) {
  std::vector<std::wstring> paths;
  std::string current;

  for (char ch : file_list) {
    if (ch == '\n') {
      if (!current.empty()) {
        paths.push_back(Utf8ToWide(current));
      }
      current.clear();
      continue;
    }
    if (ch != '\r') {
      current.push_back(ch);
    }
  }

  if (!current.empty()) {
    paths.push_back(Utf8ToWide(current));
  }

  if (paths.empty()) {
    return false;
  }

  std::size_t character_count = 1;
  for (const auto& path : paths) {
    character_count += path.size() + 1;
  }

  const std::size_t bytes = sizeof(DROPFILES) + character_count * sizeof(wchar_t);
  HGLOBAL global = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (global == nullptr) {
    return false;
  }

  auto* memory = static_cast<unsigned char*>(GlobalLock(global));
  if (memory == nullptr) {
    GlobalFree(global);
    return false;
  }

  auto* dropfiles = reinterpret_cast<DROPFILES*>(memory);
  dropfiles->pFiles = sizeof(DROPFILES);
  dropfiles->pt = POINT{0, 0};
  dropfiles->fNC = FALSE;
  dropfiles->fWide = TRUE;

  auto* destination = reinterpret_cast<wchar_t*>(memory + sizeof(DROPFILES));
  for (const auto& path : paths) {
    std::memcpy(destination, path.c_str(), path.size() * sizeof(wchar_t));
    destination += path.size();
    *destination++ = L'\0';
  }
  *destination = L'\0';

  GlobalUnlock(global);

  if (SetClipboardData(CF_HDROP, global) == nullptr) {
    GlobalFree(global);
    return false;
  }

  return true;
}

void AddContent(
    HistoryItem& item,
    ContentFormat format,
    std::string format_name,
    std::optional<std::string> payload) {
  if (!payload.has_value() || payload->empty()) {
    return;
  }

  item.contents.push_back(ContentBlob{
      format,
      std::move(format_name),
      std::move(*payload),
  });
}

}  // namespace

std::optional<HistoryItem> ReadHistoryItem(HWND owner) {
  if (OpenClipboard(owner) == FALSE) {
    return std::nullopt;
  }

  HistoryItem item;
  const UINT html_format = RegisterClipboardFormatW(kHtmlClipboardFormatName);
  const UINT rtf_format = RegisterClipboardFormatW(kRtfClipboardFormatName);

  AddContent(item, ContentFormat::kPlainText, "", ReadUtf8TextFromFormat(CF_UNICODETEXT));
  AddContent(item, ContentFormat::kHtml, "HTML Format", ReadRawFormat(html_format));
  AddContent(item, ContentFormat::kRtf, "Rich Text Format", ReadRawFormat(rtf_format));
  AddContent(item, ContentFormat::kFileList, "", ReadFileListText());

  if (auto dibv5 = ReadRawFormat(CF_DIBV5); dibv5.has_value()) {
    AddContent(item, ContentFormat::kImage, "CF_DIBV5", std::move(dibv5));
  } else {
    AddContent(item, ContentFormat::kImage, "CF_DIB", ReadRawFormat(CF_DIB));
  }

  CloseClipboard();

  if (item.contents.empty()) {
    return std::nullopt;
  }

  for (const auto& content : item.contents) {
    switch (content.format) {
      case ContentFormat::kPlainText:
        item.title = BuildHistoryTitleFromText(content.text_payload);
        break;
      case ContentFormat::kHtml:
        item.title = BuildHistoryTitleFromMarkup(content.text_payload);
        break;
      case ContentFormat::kRtf:
        item.title = BuildHistoryTitleFromRtf(content.text_payload);
        break;
      case ContentFormat::kImage:
        item.title = BuildHistoryTitleFromImage(content.text_payload);
        break;
      case ContentFormat::kFileList:
        item.title = BuildHistoryTitleFromFileList(content.text_payload);
        break;
      case ContentFormat::kCustom:
        break;
    }

    if (!item.title.empty()) {
      break;
    }
  }

  return item;
}

std::optional<std::string> ReadPlainText(HWND owner) {
  if (OpenClipboard(owner) == FALSE) {
    return std::nullopt;
  }

  const auto result = ReadUtf8TextFromFormat(CF_UNICODETEXT);
  CloseClipboard();
  return result;
}

bool WriteHistoryItem(HWND owner, const HistoryItem& item, bool plain_text_only) {
  if (OpenClipboard(owner) == FALSE) {
    return false;
  }

  if (EmptyClipboard() == FALSE) {
    CloseClipboard();
    return false;
  }

  bool wrote_data = false;
  if (plain_text_only) {
    wrote_data = SetClipboardWideText(item.PreferredContentText());
    CloseClipboard();
    return wrote_data;
  }

  const UINT html_format = RegisterClipboardFormatW(kHtmlClipboardFormatName);
  const UINT rtf_format = RegisterClipboardFormatW(kRtfClipboardFormatName);

  for (const auto& content : item.contents) {
    bool wrote_current = false;

    switch (content.format) {
      case ContentFormat::kPlainText:
        wrote_current = SetClipboardWideText(content.text_payload);
        break;
      case ContentFormat::kHtml:
        wrote_current = html_format != 0 && SetClipboardBytes(html_format, content.text_payload, true);
        break;
      case ContentFormat::kRtf:
        wrote_current = rtf_format != 0 && SetClipboardBytes(rtf_format, content.text_payload, true);
        break;
      case ContentFormat::kImage:
        wrote_current = SetClipboardBytes(
            content.format_name == "CF_DIBV5" ? CF_DIBV5 : CF_DIB,
            content.text_payload);
        break;
      case ContentFormat::kFileList:
        wrote_current = SetClipboardFileList(content.text_payload);
        break;
      case ContentFormat::kCustom:
        break;
    }

    wrote_data = wrote_data || wrote_current;
  }

  const auto preferred_text = item.PreferredContentText();
  if (!preferred_text.empty() &&
      std::none_of(
          item.contents.begin(),
          item.contents.end(),
          [](const ContentBlob& content) { return content.format == ContentFormat::kPlainText; })) {
    wrote_data = SetClipboardWideText(preferred_text) || wrote_data;
  }

  CloseClipboard();
  return wrote_data;
}

bool WritePlainText(HWND owner, std::string_view text) {
  if (OpenClipboard(owner) == FALSE) {
    return false;
  }

  if (EmptyClipboard() == FALSE) {
    CloseClipboard();
    return false;
  }

  const bool wrote = SetClipboardWideText(text);
  CloseClipboard();
  return wrote;
}

std::string BuildHistoryTitleFromText(std::string_view text, std::size_t max_length) {
  return BuildAutomaticTitle(text, max_length);
}

}  // namespace maccy::win32

#endif  // _WIN32
