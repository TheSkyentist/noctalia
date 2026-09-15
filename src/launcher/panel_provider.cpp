#include "launcher/panel_provider.h"

#include "core/deferred_call.h"
#include "i18n/i18n.h"
#include "scripting/plugin_registry.h"
#include "shell/control_center/control_center_panel.h"
#include "shell/panel/panel_manager.h"
#include "util/fuzzy_match.h"
#include "util/string_utils.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

  constexpr std::size_t kMaxResults = 50;
  constexpr std::string_view kFallbackGlyph = "apps";
  // Synthetic id prefix for a Control Center tab row ("cc-tab:media"). Never
  // collides with a real panel id: built-ins have no colon and plugin ids are
  // "author/plugin:entry" (a slash always precedes the colon).
  constexpr std::string_view kControlCenterTabPrefix = "cc-tab:";

  struct BuiltinPanelMeta {
    std::string_view id;
    std::string_view titleKey;
    std::string_view glyph;
  };

  // Every panel PanelManager registers for core (see application_ui.cpp). Core
  // panels ship no manifest to read a name/glyph from, so this is the only source
  // of truth for them; keep it in sync if a core panel id is added or renamed.
  constexpr std::array kBuiltinPanels = {
      BuiltinPanelMeta{"clipboard", "launcher.providers.panel.builtin.clipboard", "clipboard"},
      BuiltinPanelMeta{"control-center", "launcher.providers.panel.builtin.control-center", "adjustments"},
      BuiltinPanelMeta{"launcher", "launcher.providers.panel.builtin.launcher", "search"},
      BuiltinPanelMeta{"polkit", "launcher.providers.panel.builtin.polkit", "shield-lock"},
      BuiltinPanelMeta{"session", "launcher.providers.panel.builtin.session", "power"},
      BuiltinPanelMeta{"setup-wizard", "launcher.providers.panel.builtin.setup-wizard", "wand"},
      BuiltinPanelMeta{"test", "launcher.providers.panel.builtin.test", "bug"},
      BuiltinPanelMeta{"tray-drawer", "launcher.providers.panel.builtin.tray-drawer", "apps"},
      BuiltinPanelMeta{"wallpaper", "launcher.providers.panel.builtin.wallpaper", "wallpaper-selector"},
  };

  struct PanelDescription {
    std::string title;
    std::string subtitle;
    std::string glyph;
  };

  // Resolves a raw panel id ("clipboard", "author/plugin:entry") to a display
  // title/subtitle/glyph: the builtin table above, the owning plugin's manifest
  // name/icon (entry id as subtitle), or the raw id untouched as a last resort.
  [[nodiscard]] PanelDescription describePanel(std::string_view panelId) {
    const auto builtin = std::ranges::find(kBuiltinPanels, panelId, &BuiltinPanelMeta::id);
    if (builtin != kBuiltinPanels.end()) {
      return PanelDescription{
          .title = i18n::tr(builtin->titleKey), .subtitle = {}, .glyph = std::string(builtin->glyph)
      };
    }

    const auto colon = panelId.find(':');
    if (colon == std::string_view::npos) {
      return PanelDescription{.title = std::string(panelId), .subtitle = {}, .glyph = std::string(kFallbackGlyph)};
    }

    const std::string_view pluginId = panelId.substr(0, colon);
    const std::string_view entryId = panelId.substr(colon + 1);
    const auto* manifest = scripting::PluginRegistry::instance().findManifest(pluginId);
    if (manifest == nullptr) {
      return PanelDescription{
          .title = std::string(pluginId), .subtitle = std::string(entryId), .glyph = std::string(kFallbackGlyph)
      };
    }
    return PanelDescription{
        .title = manifest->name,
        .subtitle = std::string(entryId),
        .glyph = manifest->icon.empty() ? std::string(kFallbackGlyph) : manifest->icon
    };
  }

} // namespace

PanelProvider::PanelProvider(PanelManager* panelManager, ControlCenterPanel* controlCenterPanel)
    : m_panelManager(panelManager), m_controlCenterPanel(controlCenterPanel) {}

std::string PanelProvider::displayName() const { return i18n::tr("launcher.providers.panel.title"); }

std::vector<LauncherResult> PanelProvider::query(std::string_view text) const {
  if (m_panelManager == nullptr) {
    return {};
  }

  std::vector<std::pair<std::string, PanelDescription>> entries;
  const std::vector<std::string> ids = m_panelManager->availablePanelIds();
  entries.reserve(ids.size());
  for (const auto& panelId : ids) {
    entries.emplace_back(panelId, describePanel(panelId));
  }

  if (m_controlCenterPanel != nullptr) {
    const std::string controlCenterTitle = i18n::tr("launcher.providers.panel.builtin.control-center");
    for (const auto& tab : m_controlCenterPanel->visibleTabsForLauncher()) {
      entries.emplace_back(
          std::string(kControlCenterTabPrefix) + std::string(tab.key),
          PanelDescription{
              .title = i18n::tr(tab.titleKey), .subtitle = controlCenterTitle, .glyph = std::string(tab.glyph)
          }
      );
    }
  }

  if (entries.empty()) {
    return {};
  }

  struct ScoredPanel {
    std::string id;
    PanelDescription description;
    double score = 0.0;
  };

  const std::string query = StringUtils::toLower(StringUtils::trim(text));
  std::vector<ScoredPanel> scored;
  scored.reserve(entries.size());
  for (auto& [panelId, description] : entries) {
    double score = 0.0;
    if (!query.empty()) {
      const std::string searchable = StringUtils::toLower(description.title + " " + panelId);
      score = FuzzyMatch::score(query, searchable);
      if (!FuzzyMatch::isMatch(score)) {
        continue;
      }
    }
    scored.push_back(ScoredPanel{.id = std::move(panelId), .description = std::move(description), .score = score});
  }

  if (query.empty()) {
    std::ranges::sort(scored, [](const auto& a, const auto& b) { return a.description.title < b.description.title; });
  } else {
    std::ranges::sort(scored, [](const auto& a, const auto& b) { return a.score > b.score; });
  }
  if (scored.size() > kMaxResults) {
    scored.resize(kMaxResults);
  }

  std::vector<LauncherResult> results;
  results.reserve(scored.size());
  for (auto& entry : scored) {
    LauncherResult result;
    result.id = std::move(entry.id);
    result.title = std::move(entry.description.title);
    result.subtitle = std::move(entry.description.subtitle);
    result.glyphName = std::move(entry.description.glyph);
    result.score = entry.score;
    results.push_back(std::move(result));
  }
  return results;
}

bool PanelProvider::activate(const LauncherResult& result) {
  if (m_panelManager == nullptr) {
    return false;
  }
  if (!result.providerId.empty() && result.providerId != id()) {
    return false;
  }

  PanelManager* panelManager = m_panelManager;
  std::string panelId = result.id;
  std::string context;
  if (panelId.starts_with(kControlCenterTabPrefix)) {
    context = panelId.substr(kControlCenterTabPrefix.size());
    panelId = "control-center";
  }

  // Defer to the next main-loop iteration so LauncherPanel's own close (right
  // after activate() returns) doesn't immediately undo this panel's open.
  DeferredCall::callLater([panelManager, panelId = std::move(panelId), context = std::move(context)]() {
    if (context.empty()) {
      panelManager->togglePanel(panelId);
    } else {
      panelManager->togglePanel(panelId, PanelOpenRequest{.context = context});
    }
  });
  return true;
}
