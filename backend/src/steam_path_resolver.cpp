#include "protonctl/steam_path_resolver.hpp"

namespace protonctl {

bool SteamPathResolver::LooksLikeSteamRoot(const fs::path& path) {
  std::error_code ec;
  if (!fs::is_directory(path, ec) || ec) return false;

  bool has_steamapps = fs::is_directory(path / "steamapps", ec);
  bool has_native_marker =
      fs::exists(path / "steam.sh", ec) || fs::exists(path / "ubuntu12_32" / "steam", ec);

  // steamapps/ existing is the load-bearing signal; the native launcher
  // marker is a nice-to-have that many valid installs (e.g. freshly
  // created Flatpak prefixes before first run) may lack.
  return has_steamapps;
}

SteamCandidate SteamPathResolver::MakeCandidate(SteamInstallType type, const fs::path& path,
                                                 const std::string& label) {
  SteamCandidate candidate;
  candidate.type = type;
  candidate.path = path;
  candidate.label = label;
  candidate.valid = LooksLikeSteamRoot(path);
  return candidate;
}

std::vector<SteamCandidate> SteamPathResolver::DetectCandidates() {
  std::vector<SteamCandidate> candidates;

  candidates.push_back(MakeCandidate(
      SteamInstallType::Flatpak,
      ExpandUserHome("~/.var/app/com.valvesoftware.Steam/.local/share/Steam"),
      "Flatpak"));

  candidates.push_back(MakeCandidate(
      SteamInstallType::Debian,
      ExpandUserHome("~/.steam/debian_installation"),
      "Debian / derivative"));

  candidates.push_back(MakeCandidate(
      SteamInstallType::Native,
      ExpandUserHome("~/.local/share/Steam"),
      "Native"));

  // `~/.steam/steam` is a very common symlink target on distros that
  // don't use the `debian_installation` naming; surface it too if it
  // resolves to something different from the candidates above.
  fs::path legacy_symlink = ExpandUserHome("~/.steam/steam");
  std::error_code ec;
  if (fs::exists(legacy_symlink, ec)) {
    fs::path resolved = fs::weakly_canonical(legacy_symlink, ec);
    bool duplicate = false;
    for (const auto& c : candidates) {
      std::error_code ec2;
      if (fs::weakly_canonical(c.path, ec2) == resolved) {
        duplicate = true;
        break;
      }
    }
    if (!duplicate) {
      candidates.push_back(MakeCandidate(SteamInstallType::Unknown, legacy_symlink,
                                          "~/.steam/steam (symlink)"));
    }
  }

  return candidates;
}

SteamCandidate SteamPathResolver::ValidateCustomPath(const fs::path& path) {
  return MakeCandidate(SteamInstallType::Custom, path, "Custom");
}

fs::path SteamPathResolver::CompatibilityToolsDir(const fs::path& steam_path) {
  fs::path dir = steam_path / "compatibilitytools.d";
  std::error_code ec;
  fs::create_directories(dir, ec);
  return dir;
}

fs::path SteamPathResolver::CompatDataDir(const fs::path& steam_path) {
  return steam_path / "steamapps" / "compatdata";
}

}  // namespace protonctl
