#include "protonctl/ffi_bridge.h"

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>

#include "protonctl/backup_manager.hpp"
#include "protonctl/build_runner.hpp"
#include "protonctl/config.hpp"
#include "protonctl/install_runner.hpp"
#include "protonctl/proton_repository_manager.hpp"
#include "protonctl/repair_manager.hpp"
#include "protonctl/steam_path_resolver.hpp"

namespace {

using namespace protonctl;

thread_local std::string g_last_error;

// Lazily constructed so a missing GITHUB_TOKEN env var doesn't matter until
// a network call is actually made.
ProtonRepositoryManager& RepoManager() {
  static ProtonRepositoryManager instance([] {
    const char* token = std::getenv("GITHUB_TOKEN");
    return token ? std::string(token) : std::string();
  }());
  return instance;
}

BuildRunner& GlobalBuildRunner() {
  static BuildRunner instance;
  return instance;
}

InstallRunner& GlobalInstallRunner() {
  static InstallRunner instance([] {
    const char* token = std::getenv("GITHUB_TOKEN");
    return token ? std::string(token) : std::string();
  }());
  return instance;
}

char* DupString(const std::string& s) {
  char* out = static_cast<char*>(std::malloc(s.size() + 1));
  if (!out) return nullptr;
  std::memcpy(out, s.c_str(), s.size() + 1);
  return out;
}

char* JsonToCString(const nlohmann::json& j) { return DupString(j.dump()); }

// Runs `fn`, capturing any ProtonCtlError/std::exception into g_last_error and
// returning nullptr/`error_value` on failure so no exception ever crosses
// the C ABI boundary into Dart.
template <typename Fn>
auto Guard(Fn&& fn, decltype(fn()) error_value) -> decltype(fn()) {
  try {
    g_last_error.clear();
    return fn();
  } catch (const std::exception& e) {
    g_last_error = e.what();
    return error_value;
  }
}

}  // namespace

extern "C" {

void protonctl_free_string(char* ptr) { std::free(ptr); }

const char* protonctl_last_error(void) { return g_last_error.c_str(); }

char* protonctl_detect_steam_candidates(void) {
  return Guard(
      [] {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& c : SteamPathResolver::DetectCandidates()) arr.push_back(c.ToJson());
        return JsonToCString(arr);
      },
      static_cast<char*>(nullptr));
}

char* protonctl_validate_steam_path(const char* path) {
  return Guard(
      [&] { return JsonToCString(SteamPathResolver::ValidateCustomPath(fs::path(path)).ToJson()); },
      static_cast<char*>(nullptr));
}

char* protonctl_load_config(void) {
  return Guard(
      [] {
        auto cfg = ConfigManager::Load();
        return JsonToCString(cfg ? cfg->ToJson() : AppConfig{}.ToJson());
      },
      static_cast<char*>(nullptr));
}

int protonctl_save_config(const char* config_json) {
  return Guard(
      [&] {
        nlohmann::json j = nlohmann::json::parse(config_json);
        ConfigManager::Save(AppConfig::FromJson(j));
        return 0;
      },
      1);
}

char* protonctl_list_repositories(void) {
  return Guard(
      [] {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& r : RepoManager().ListRepositories()) arr.push_back(r.ToJson());
        return JsonToCString(arr);
      },
      static_cast<char*>(nullptr));
}

char* protonctl_add_repository(const char* owner, const char* repo) {
  return Guard([&] { return JsonToCString(RepoManager().AddCustomRepository(owner, repo).ToJson()); },
               static_cast<char*>(nullptr));
}

int protonctl_remove_repository(const char* owner, const char* repo) {
  return Guard(
      [&] {
        RepoManager().RemoveRepository(owner, repo);
        return 0;
      },
      1);
}

char* protonctl_fetch_releases(const char* owner, const char* repo) {
  return Guard(
      [&] {
        ProtonRepository r;
        r.owner = owner;
        r.repo = repo;
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& rel : RepoManager().FetchAvailableReleases(r)) arr.push_back(rel.ToJson());
        return JsonToCString(arr);
      },
      static_cast<char*>(nullptr));
}

char* protonctl_check_for_update(const char* owner, const char* repo,
                                  const char* installed_version_tag) {
  return Guard(
      [&] {
        ProtonRepository r;
        r.owner = owner;
        r.repo = repo;
        auto update = RepoManager().CheckForUpdate(r, installed_version_tag);
        return JsonToCString(update ? update->ToJson() : nlohmann::json(nullptr));
      },
      static_cast<char*>(nullptr));
}

char* protonctl_install_release(const char* repo_json, const char* release_json,
                              const char* compat_tools_dir) {
  return Guard(
      [&] {
        ProtonRepository repo = ProtonRepository::FromJson(nlohmann::json::parse(repo_json));
        GitHubRelease release = GitHubRelease::FromJson(nlohmann::json::parse(release_json));
        InstalledProtonVersion installed =
            RepoManager().InstallRelease(repo, release, fs::path(compat_tools_dir));
        return JsonToCString(installed.ToJson());
      },
      static_cast<char*>(nullptr));
}

char* protonctl_list_installed(const char* compat_tools_dir) {
  return Guard(
      [&] {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& v : RepoManager().ListInstalledVersions(fs::path(compat_tools_dir))) {
          arr.push_back(v.ToJson());
        }
        return JsonToCString(arr);
      },
      static_cast<char*>(nullptr));
}

int protonctl_remove_installed(const char* compat_tools_dir, const char* name) {
  return Guard(
      [&] {
        RepoManager().RemoveInstalledVersion(fs::path(compat_tools_dir), name);
        return 0;
      },
      1);
}

char* protonctl_enqueue_install(const char* repo_json, const char* release_json,
                                 const char* compat_tools_dir) {
  return Guard(
      [&] {
        ProtonRepository repo = ProtonRepository::FromJson(nlohmann::json::parse(repo_json));
        GitHubRelease release = GitHubRelease::FromJson(nlohmann::json::parse(release_json));
        std::string id =
            GlobalInstallRunner().EnqueueInstall(repo, release, fs::path(compat_tools_dir));
        return DupString(id);
      },
      static_cast<char*>(nullptr));
}

char* protonctl_get_install_status(const char* job_id) {
  return Guard(
      [&] {
        auto job = GlobalInstallRunner().GetStatus(job_id);
        return job ? JsonToCString(job->ToJson()) : static_cast<char*>(nullptr);
      },
      static_cast<char*>(nullptr));
}

char* protonctl_list_install_jobs(void) {
  return Guard(
      [] {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& job : GlobalInstallRunner().ListJobs()) arr.push_back(job.ToJson());
        return JsonToCString(arr);
      },
      static_cast<char*>(nullptr));
}

int protonctl_cancel_install(const char* job_id) {
  return Guard([&] { return GlobalInstallRunner().Cancel(job_id) ? 0 : 1; }, 1);
}

char* protonctl_enqueue_build(const char* repo_json, const char* ref, const char* compat_tools_dir) {
  return Guard(
      [&] {
        ProtonRepository repo = ProtonRepository::FromJson(nlohmann::json::parse(repo_json));
        std::string id =
            GlobalBuildRunner().EnqueueBuild(repo, ref ? ref : "", fs::path(compat_tools_dir));
        return DupString(id);
      },
      static_cast<char*>(nullptr));
}

char* protonctl_get_build_status(const char* job_id) {
  return Guard(
      [&] {
        auto job = GlobalBuildRunner().GetStatus(job_id);
        return job ? JsonToCString(job->ToJson()) : static_cast<char*>(nullptr);
      },
      static_cast<char*>(nullptr));
}

char* protonctl_list_build_jobs(void) {
  return Guard(
      [] {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& job : GlobalBuildRunner().ListJobs()) arr.push_back(job.ToJson());
        return JsonToCString(arr);
      },
      static_cast<char*>(nullptr));
}

int protonctl_cancel_build(const char* job_id) {
  return Guard([&] { return GlobalBuildRunner().Cancel(job_id) ? 0 : 1; }, 1);
}

int protonctl_create_backup(const char* proton_dir, const char* prefix_dir, const char* manifest_json,
                          const char* out_file, int compression_level) {
  return Guard(
      [&] {
        PrtBakManifest manifest = PrtBakManifest::FromJson(nlohmann::json::parse(manifest_json));
        BackupManager::CreateBackup(fs::path(proton_dir),
                                     prefix_dir ? fs::path(prefix_dir) : fs::path{}, manifest,
                                     fs::path(out_file), compression_level > 0 ? compression_level : 6);
        return 0;
      },
      1);
}

char* protonctl_read_manifest(const char* prtbak_file) {
  return Guard([&] { return JsonToCString(BackupManager::ReadManifest(fs::path(prtbak_file)).ToJson()); },
               static_cast<char*>(nullptr));
}

int protonctl_restore_backup(const char* prtbak_file, const char* restore_root) {
  return Guard(
      [&] {
        BackupManager::RestoreBackup(fs::path(prtbak_file), fs::path(restore_root));
        return 0;
      },
      1);
}

int protonctl_create_library_backup(const char* compat_tools_dir, const char* out_file,
                                     int compression_level) {
  return Guard(
      [&] {
        BackupManager::CreateLibraryBackup(fs::path(compat_tools_dir), fs::path(out_file),
                                            compression_level > 0 ? compression_level : 6);
        return 0;
      },
      1);
}

char* protonctl_read_library_manifest(const char* prtbak_file) {
  return Guard(
      [&] { return JsonToCString(BackupManager::ReadLibraryManifest(fs::path(prtbak_file)).ToJson()); },
      static_cast<char*>(nullptr));
}

int protonctl_restore_library_backup(const char* prtbak_file, const char* compat_tools_dir) {
  return Guard(
      [&] {
        BackupManager::RestoreLibraryBackup(fs::path(prtbak_file), fs::path(compat_tools_dir));
        return 0;
      },
      1);
}

char* protonctl_scan_repair(const char* compat_tools_dir, const char* compat_data_dir) {
  return Guard(
      [&] {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& issue :
             RepairManager::Scan(fs::path(compat_tools_dir), fs::path(compat_data_dir))) {
          arr.push_back(issue.ToJson());
        }
        return JsonToCString(arr);
      },
      static_cast<char*>(nullptr));
}

int protonctl_fix_issue(const char* issue_json) {
  return Guard(
      [&] {
        nlohmann::json j = nlohmann::json::parse(issue_json);
        RepairIssue issue;
        issue.type = j.value("type", RepairIssueType::BrokenSymlink);
        issue.path = fs::path(j.value("path", ""));
        issue.description = j.value("description", "");
        issue.fixable = j.value("fixable", false);
        return RepairManager::Fix(issue) ? 0 : 1;
      },
      1);
}

}  // extern "C"
