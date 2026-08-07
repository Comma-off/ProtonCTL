#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

#include "protonctl/common.hpp"

namespace protonctl {

enum class SteamInstallType {
  Flatpak,
  Debian,
  Native,
  Custom,
  Unknown,
};

NLOHMANN_JSON_SERIALIZE_ENUM(SteamInstallType, {
  {SteamInstallType::Unknown, "unknown"},
  {SteamInstallType::Flatpak, "flatpak"},
  {SteamInstallType::Debian, "debian"},
  {SteamInstallType::Native, "native"},
  {SteamInstallType::Custom, "custom"},
})

// A Steam installation candidate discovered on disk (or supplied by hand),
// with a validity flag so the wizard can gray out installs that don't
// actually contain a `steamapps` folder.
struct SteamCandidate {
  SteamInstallType type = SteamInstallType::Unknown;
  fs::path path;
  bool valid = false;
  std::string label;

  nlohmann::json ToJson() const;
};

// Persisted application configuration, written to
// `$XDG_CONFIG_HOME/protonctl/config.json` (or `~/.config/protonctl/config.json`).
struct AppConfig {
  SteamInstallType install_type = SteamInstallType::Unknown;
  fs::path steam_path;
  fs::path compatibility_tools_dir;  // <steam_path>/compatibilitytools.d
  bool first_start_completed = false;

  // Theme preferences persisted so the Flutter shell can restore them
  // without round-tripping to shared_preferences on its own; kept here too
  // so the CLI / repair tooling can print a consistent config dump.
  std::string theme_spec_version = "spec2025";  // "spec2021" | "spec2025"
  std::string theme_variant = "expressive";      // tonalSpot|fidelity|content|expressive
  int seed_color_index = 0;

  nlohmann::json ToJson() const;
  static AppConfig FromJson(const nlohmann::json& j);
};

class ConfigManager {
 public:
  // Returns the canonical path to the config file, creating parent
  // directories as needed.
  static fs::path ConfigFilePath();

  static std::optional<AppConfig> Load();
  static void Save(const AppConfig& config);
};

}  // namespace protonctl
