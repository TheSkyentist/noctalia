#pragma once

#include "launcher/launcher_provider.h"

class PanelManager;

// Lists every panel PanelManager knows about (built-in or plugin-registered) and
// toggles the selected one. Built-in panels get a small hardcoded name/glyph
// table since they ship in core and have no manifest; plugin panels ("author/
// plugin:entry") are described from the owning plugin's manifest name/icon.
class PanelProvider : public LauncherProvider {
public:
  explicit PanelProvider(PanelManager* panelManager);

  [[nodiscard]] std::string_view defaultPrefix() const override { return "pan"; }
  [[nodiscard]] std::string_view id() const override { return "Panels"; }
  [[nodiscard]] std::string displayName() const override;
  [[nodiscard]] std::string_view defaultGlyphName() const override { return "layout-bottombar"; }
  [[nodiscard]] bool trackUsage() const override { return true; }

  [[nodiscard]] std::vector<LauncherResult> query(std::string_view text) const override;

  bool activate(const LauncherResult& result) override;

private:
  PanelManager* m_panelManager = nullptr;
};
