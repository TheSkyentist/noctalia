#include "shell/wallpaper/wallpaper_mask_registry.h"

#include "core/log.h"

#include <unordered_map>
#include <utility>

namespace {
  constexpr Logger kLog("wallpaper-mask");
} // namespace

void WallpaperMaskRegistry::configure(
    OutputKnownCheck outputKnown, WallpaperPathLookup wallpaperPathLookup, SyncCallback sync
) {
  m_outputKnown = std::move(outputKnown);
  m_wallpaperPathLookup = std::move(wallpaperPathLookup);
  m_sync = std::move(sync);
}

void WallpaperMaskRegistry::set(
    std::uint64_t ownerId, const std::string& outputName, std::optional<OutputWallpaperMask> mask
) {
  if (ownerId == 0 || outputName.empty()) {
    kLog.warn("rejected wallpaper mask with missing owner or output");
    return;
  }

  const auto existing = m_masks.find(outputName);
  if (!mask.has_value()) {
    if (existing != m_masks.end() && existing->second.ownerId == ownerId) {
      m_masks.erase(existing);
      sync();
    }
    return;
  }

  if (mask->path.empty() || mask->wallpaperPath.empty()) {
    kLog.warn("rejected incomplete wallpaper mask for {}", outputName);
    return;
  }
  if (existing != m_masks.end() && existing->second.ownerId != ownerId) {
    kLog.warn("output {} already has a wallpaper mask owner", outputName);
    return;
  }
  if (!m_outputKnown || !m_outputKnown(outputName)) {
    kLog.warn("rejected wallpaper mask for unknown output {}", outputName);
    return;
  }
  if (!m_wallpaperPathLookup || m_wallpaperPathLookup(outputName) != mask->wallpaperPath) {
    kLog.warn("rejected wallpaper mask for stale wallpaper on {}", outputName);
    return;
  }

  mask->ownerId = ownerId;
  m_masks.insert_or_assign(outputName, std::move(*mask));
  sync();
}

void WallpaperMaskRegistry::clearOwner(std::uint64_t ownerId) {
  if (ownerId == 0) {
    return;
  }
  const auto removed = std::erase_if(m_masks, [ownerId](const auto& item) { return item.second.ownerId == ownerId; });
  if (removed != 0) {
    sync();
  }
}

void WallpaperMaskRegistry::prune() {
  if (!m_wallpaperPathLookup) {
    m_masks.clear();
    sync();
    return;
  }
  const auto removed = std::erase_if(m_masks, [this](const auto& item) {
    return m_wallpaperPathLookup(item.first) != item.second.wallpaperPath;
  });
  if (removed != 0) {
    sync();
  }
}

void WallpaperMaskRegistry::sync() const {
  if (m_sync) {
    m_sync(m_masks);
  }
}
