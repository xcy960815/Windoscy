#include <cassert>
#include <string>

#include "core/ignore_rules.h"

namespace {

maccy::HistoryItem MakeItem(std::string title, std::string text, std::string source_application = "") {
  maccy::HistoryItem item;
  item.title = std::move(title);
  item.metadata.source_application = std::move(source_application);
  item.contents = {
      maccy::ContentBlob{maccy::ContentFormat::kPlainText, "", std::move(text)},
      maccy::ContentBlob{maccy::ContentFormat::kHtml, "", "<b>ignored html</b>"},
  };
  return item;
}

}  // namespace

int main() {
  {
    maccy::AppSettings settings;
    settings.ignore.ignored_applications = {"slack.exe"};

    const auto decision = maccy::ApplyIgnoreRules(settings, MakeItem("Title", "content", "Slack.EXE"));
    assert(!decision.should_store);
    assert(decision.reason == maccy::IgnoreReason::kIgnoredApplication);
  }

  {
    maccy::AppSettings settings;
    settings.ignore.allowed_applications = {"Code.exe"};

    const auto blocked = maccy::ApplyIgnoreRules(settings, MakeItem("Title", "content", "Notepad.exe"));
    assert(!blocked.should_store);
    assert(blocked.reason == maccy::IgnoreReason::kNotAllowedApplication);

    const auto allowed = maccy::ApplyIgnoreRules(settings, MakeItem("Title", "content", "Code.exe"));
    assert(allowed.should_store);
  }

  {
    maccy::AppSettings settings;
    settings.ignore.ignored_patterns = {"password\\s*=\\s*\\w+"};

    const auto decision = maccy::ApplyIgnoreRules(settings, MakeItem("Secret", "password = abc123"));
    assert(!decision.should_store);
    assert(decision.reason == maccy::IgnoreReason::kIgnoredPattern);
  }

  {
    maccy::AppSettings settings;
    settings.ignore.capture_html = false;

    const auto decision = maccy::ApplyIgnoreRules(settings, MakeItem("Title", "plain text"));
    assert(decision.should_store);
    assert(decision.item.contents.size() == 1);
    assert(decision.item.contents.front().format == maccy::ContentFormat::kPlainText);
  }

  {
    maccy::AppSettings settings;
    settings.ignore.capture_text = false;
    settings.ignore.capture_html = false;

    const auto decision = maccy::ApplyIgnoreRules(settings, MakeItem("Title", "plain text"));
    assert(!decision.should_store);
    assert(decision.reason == maccy::IgnoreReason::kIgnoredFormat);
  }

  {
    maccy::AppSettings settings;
    settings.ignore.ignore_all = true;

    const auto decision = maccy::ApplyIgnoreRules(settings, MakeItem("Title", "plain text"));
    assert(!decision.should_store);
    assert(decision.reason == maccy::IgnoreReason::kIgnoreAll);
  }

  return 0;
}
