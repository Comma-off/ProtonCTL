#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "protonctl/common.hpp"
#include "protonctl/github_client.hpp"

namespace protonctl {

// A GitHub repository registered as a Proton build/release source.
// `GE-Proton` (GloriousEggroll/proton-ge-custom) is seeded by default;
// users may add arbitrary `owner/repo` pairs.
struct ProtonRepository {
  std::string owner;
  std::string repo;
  std::string display_name;
  bool is_builtin = false;
  // Optional override for the build pipeline; if empty, BuildRunner falls
  // back to autodetection (./autogen.sh && ./configure && make, or a
  // top-level `build.sh`/`Makefile`).
  std::string build_command_override;

  std::string FullName() const { return owner + "/" + repo; }
  nlohmann::json ToJson() const;
  static ProtonRepository FromJson(const nlohmann::json& j);
};

// A Proton build that has been unpacked into compatibilitytools.d.
struct InstalledProtonVersion {
  std::string name;             // directory name under compatibilitytools.d
  fs::path path;
  std::string source_repo;      // "owner/repo" it came from, if known
  std::string version_tag;      // release tag, if known
  std::string installed_at;     // ISO-8601

  nlohmann::json ToJson() const;
};

// What the `.protonctl-installed-at` marker file (see `MarkInstalled`)
// records about an installed directory.
struct InstalledMetadata {
  std::string installed_at;  // ISO-8601
  std::string source_repo;   // "owner/repo", empty if unknown (e.g. legacy marker)
  std::string version_tag;   // exact release tag/ref installed, empty if unknown
};

class ProtonRepositoryManager {
 public:
  explicit ProtonRepositoryManager(std::string github_token = "");

  // Repository registry, persisted alongside AppConfig at
  // `~/.config/protonctl/repositories.json`.
  std::vector<ProtonRepository> ListRepositories() const;
  ProtonRepository AddCustomRepository(const std::string& owner, const std::string& repo);
  void RemoveRepository(const std::string& owner, const std::string& repo);

  std::vector<GitHubRelease> FetchAvailableReleases(const ProtonRepository& repo) const;

  // Returns the latest release for `repo` if its tag differs from
  // `installed_version_tag` (GitHub returns releases newest-first), or
  // nullopt if already up to date, the tag can't be determined, or the repo
  // has no releases at all.
  std::optional<GitHubRelease> CheckForUpdate(const ProtonRepository& repo,
                                               const std::string& installed_version_tag) const;

  // Downloads `release`'s first tar.gz/tar.xz/tar.zst asset and extracts it
  // into `compat_tools_dir`, returning the resulting installed version.
  InstalledProtonVersion InstallRelease(const ProtonRepository& repo,
                                        const GitHubRelease& release,
                                        const fs::path& compat_tools_dir,
                                        const DownloadProgressCallback& on_progress = nullptr) const;

  std::vector<InstalledProtonVersion> ListInstalledVersions(const fs::path& compat_tools_dir) const;
  void RemoveInstalledVersion(const fs::path& compat_tools_dir, const std::string& name) const;

  static fs::path RepositoryRegistryPath();

  // Records the current time and source repo as `install_dir`'s install
  // metadata via a small marker file - `ListInstalledVersions` re-scans the
  // directory fresh on every call and has no other way to recover this
  // after the fact. Used by both `InstallRelease` and `BuildRunner`'s
  // packaging step so both paths show up consistently in the
  // installed-tools list and in library backup manifests.
  static void MarkInstalled(const fs::path& install_dir, const std::string& source_repo,
                             const std::string& version_tag = "");
  static InstalledMetadata ReadInstalledMetadata(const fs::path& install_dir);

 private:
  void SaveRepositories(const std::vector<ProtonRepository>& repos) const;
  const GitHubAsset* PickBestAsset(const GitHubRelease& release) const;

  GitHubClient github_;
};

}  // namespace protonctl
