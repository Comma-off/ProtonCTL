#pragma once

#include <vector>

#include "protonctl/config.hpp"

namespace protonctl {

// Locates and validates Steam installations across the three well-known
// Linux layouts, plus arbitrary user-supplied directories.
class SteamPathResolver {
 public:
  // Probes the standard locations (Flatpak, Debian/derivative, native) and
  // returns one candidate per layout, each flagged valid/invalid depending
  // on whether it actually looks like a Steam install.
  static std::vector<SteamCandidate> DetectCandidates();

  // Validates an arbitrary path supplied by the user in the wizard's
  // "custom path" field.
  static SteamCandidate ValidateCustomPath(const fs::path& path);

  // True if `path` contains `steamapps/` and either `steam.sh` or
  // `ubuntu12_32/steam` (native client marker).
  static bool LooksLikeSteamRoot(const fs::path& path);

  // Resolves (and creates if missing) `<steam_path>/compatibilitytools.d`.
  static fs::path CompatibilityToolsDir(const fs::path& steam_path);

  // Best-effort location of Proton prefixes:
  // `<steam_path>/steamapps/compatdata/<appid>/pfx`.
  static fs::path CompatDataDir(const fs::path& steam_path);

 private:
  static SteamCandidate MakeCandidate(SteamInstallType type,
                                       const fs::path& path,
                                       const std::string& label);
};

}  // namespace protonctl
