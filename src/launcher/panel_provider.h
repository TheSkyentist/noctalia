#pragma once

#include "launcher/launcher_provider.h"

class PanelManager;
class ControlCenterPanel;

// Lists every panel PanelManager knows about (built-in or plugin-registered) and
// toggles the selected one. Built-in panels get a small hardcoded name/glyph
// table since they ship in core and have no manifest; plugin panels ("author/
// plugin:entry") are described from the owning plugin's manifest name/icon.
// Also lists Control Center's currently-visible tabs (Audio, Network, ...) as
// their own rows, so a user can jump straight to one instead of landing on
// whichever tab Control Center last had open.
class PanelProvider : public LauncherProvider {
public:
  PanelProvider(PanelManager* panelManager, ControlCenterPanel* controlCenterPanel);

  [[nodiscard]] std::string_view defaultPrefix() const override { return "pan"; }
  [[nodiscard]] std::string_view id() const override { return "Panels"; }
  [[nodiscard]] std::string displayName() const override;
  [[nodiscard]] std::string_view defaultGlyphName() const override { return "rectangle"; }
  [[nodiscard]] bool trackUsage() const override { return true; }

  [[nodiscard]] std::vector<LauncherResult> query(std::string_view text) const override;

  bool activate(const LauncherResult& result) override;

private:
  PanelManager* m_panelManager = nullptr;
  ControlCenterPanel* m_controlCenterPanel = nullptr;
};
