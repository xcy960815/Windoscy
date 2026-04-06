#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "core/history_persistence.h"

namespace {

maccy::HistoryItem MakeItem() {
  maccy::HistoryItem item;
  item.id = 42;
  item.title = "Pinned title";
  item.title_overridden = true;
  item.pinned = true;
  item.pin_key = 'b';
  item.metadata.source_application = "Code\tEditor";
  item.metadata.copy_count = 7;
  item.metadata.first_copied_at = 100;
  item.metadata.last_copied_at = 200;
  item.metadata.from_app = true;
  item.metadata.modified_after_copy = false;
  item.contents = {
      maccy::ContentBlob{maccy::ContentFormat::kPlainText, "", "Line1\nLine2"},
      maccy::ContentBlob{maccy::ContentFormat::kCustom, "my.custom", std::string("\x01\x02\x03", 3)},
  };
  return item;
}

}  // namespace

int main() {
  namespace fs = std::filesystem;

  const auto unique_suffix = std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count());
  const fs::path path = fs::temp_directory_path() / ("maccy_history_" + unique_suffix + ".dat");

  const std::vector<maccy::HistoryItem> original = {MakeItem()};
  assert(maccy::SaveHistoryFile(path, original));

  const auto loaded = maccy::LoadHistoryFile(path);
  assert(loaded.size() == 1);
  assert(loaded[0].id == original[0].id);
  assert(loaded[0].title == original[0].title);
  assert(loaded[0].title_overridden == original[0].title_overridden);
  assert(loaded[0].pinned == original[0].pinned);
  assert(loaded[0].pin_key == original[0].pin_key);
  assert(loaded[0].metadata.source_application == original[0].metadata.source_application);
  assert(loaded[0].metadata.copy_count == original[0].metadata.copy_count);
  assert(loaded[0].metadata.first_copied_at == original[0].metadata.first_copied_at);
  assert(loaded[0].metadata.last_copied_at == original[0].metadata.last_copied_at);
  assert(loaded[0].metadata.from_app == original[0].metadata.from_app);
  assert(loaded[0].contents.size() == original[0].contents.size());
  assert(loaded[0].contents[0].text_payload == original[0].contents[0].text_payload);
  assert(loaded[0].contents[1].format_name == original[0].contents[1].format_name);
  assert(loaded[0].contents[1].text_payload == original[0].contents[1].text_payload);

  std::error_code error;
  fs::remove(path, error);
  return 0;
}
