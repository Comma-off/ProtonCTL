#include "protonctl/config.hpp"

#include <cstdlib>
#include <fstream>

namespace protonctl {

nlohmann::json SteamCandidate::ToJson() const {
  return {
      {"type", type},
      {"path", path.string()},
      {"valid", valid},
      {"label", label},
  };
}

nlohmann::json AppConfig::ToJson() const {
  return {
      {"install_type", install_type},
      {"steam_path", steam_path.string()},
      {"compatibility_tools_dir", compatibility_tools_dir.string()},
      {"first_start_completed", first_start_completed},
      {"theme_spec_version", theme_spec_version},
      {"theme_variant", theme_variant},
      {"seed_color_index", seed_color_index},
  };
}

AppConfig AppConfig::FromJson(const nlohmann::json& j) {
  AppConfig cfg;
  cfg.install_type = j.value("install_type", SteamInstallType::Unknown);
  cfg.steam_path = fs::path(j.value("steam_path", ""));
  cfg.compatibility_tools_dir = fs::path(j.value("compatibility_tools_dir", ""));
  cfg.first_start_completed = j.value("first_start_completed", false);
  cfg.theme_spec_version = j.value("theme_spec_version", std::string("spec2025"));
  cfg.theme_variant = j.value("theme_variant", std::string("expressive"));
  cfg.seed_color_index = j.value("seed_color_index", 0);
  return cfg;
}

fs::path ConfigManager::ConfigFilePath() {
  const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
  fs::path base = xdg_config && *xdg_config
                      ? fs::path(xdg_config)
                      : ExpandUserHome("~/.config");
  fs::path dir = base / "protonctl";
  std::error_code ec;
  fs::create_directories(dir, ec);
  return dir / "config.json";
}

std::optional<AppConfig> ConfigManager::Load() {
  fs::path path = ConfigFilePath();
  std::ifstream in(path);
  if (!in.is_open()) return std::nullopt;

  try {
    nlohmann::json j;
    in >> j;
    return AppConfig::FromJson(j);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

void ConfigManager::Save(const AppConfig& config) {
  fs::path path = ConfigFilePath();
  std::ofstream out(path, std::ios::trunc);
  if (!out.is_open()) {
    throw ProtonCtlError("Unable to write config file at " + path.string());
  }
  out << config.ToJson().dump(2);
}

}  // namespace protonctl
