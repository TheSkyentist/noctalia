#pragma once

#include "shell/wallpaper/wallpaper_mask.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

// Tracks per-output wallpaper masks with single-owner semantics, validating each mask against the
// output's current wallpaper before accepting it. The output-liveness and wallpaper-path lookups are
// injected so this stays usable from both the desktop and lock screen widget controllers.
class WallpaperMaskRegistry {
public:
  using OutputKnownCheck = std::function<bool(const std::string& outputName)>;
  using WallpaperPathLookup = std::function<std::string(const std::string& outputName)>;
  using SyncCallback = std::function<void(const OutputWallpaperMaskMap&)>;

  void configure(OutputKnownCheck outputKnown, WallpaperPathLookup wallpaperPathLookup, SyncCallback sync);

  void set(std::uint64_t ownerId, const std::string& outputName, std::optional<OutputWallpaperMask> mask);
  void clearOwner(std::uint64_t ownerId);
  void prune();

private:
  void sync() const;

  OutputWallpaperMaskMap m_masks;
  OutputKnownCheck m_outputKnown;
  WallpaperPathLookup m_wallpaperPathLookup;
  SyncCallback m_sync;
};
