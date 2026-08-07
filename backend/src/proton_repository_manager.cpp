#include "protonctl/proton_repository_manager.hpp"

#include <archive.h>
#include <archive_entry.h>

#include "protonctl/config.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace protonctl {

namespace {

constexpr const char* kInstalledAtMarkerName = ".protonctl-installed-at";

bool HasArchiveExtension(const std::string& name) {
  static const std::vector<std::string> kSuffixes = {
      ".tar.gz", ".tar.xz", ".tar.zst", ".tgz", ".tar.bz2",
  };
  return std::any_of(kSuffixes.begin(), kSuffixes.end(), [&](const std::string& suffix) {
    return name.size() >= suffix.size() &&
           name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
  });
}

// Extracts a (possibly compressed) tar archive into `dest_dir`, returning
// the name of the single top-level directory it produced, if any -- Proton
// release tarballs are conventionally a single directory such as
// `GE-Proton9-20/`.
std::string ExtractTarball(const fs::path& archive_path, const fs::path& dest_dir) {
  struct archive* a = archive_read_new();
  archive_read_support_format_tar(a);
  archive_read_support_filter_all(a);

  if (archive_read_open_filename(a, archive_path.c_str(), 1 << 20) != ARCHIVE_OK) {
    std::string err = archive_error_string(a);
    archive_read_free(a);
    throw ProtonCtlError("Failed to open archive: " + err);
  }

  struct archive* ext = archive_write_disk_new();
  archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM |
                                           ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS |
                                           ARCHIVE_EXTRACT_SECURE_NODOTDOT);

  std::string top_level_dir;
  struct archive_entry* entry;
  int r;
  while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
    std::string entry_path = archive_entry_pathname(entry);
    if (top_level_dir.empty()) {
      size_t slash = entry_path.find('/');
      top_level_dir = slash == std::string::npos ? entry_path : entry_path.substr(0, slash);
    }

    fs::path full_dest = dest_dir / entry_path;
    archive_entry_set_pathname(entry, full_dest.c_str());

    if (archive_write_header(ext, entry) != ARCHIVE_OK) {
      throw ProtonCtlError(std::string("Archive write header failed: ") + archive_error_string(ext));
    }

    const void* buff;
    size_t size;
    la_int64_t offset;
    while (true) {
      int rr = archive_read_data_block(a, &buff, &size, &offset);
      if (rr == ARCHIVE_EOF) break;
      if (rr != ARCHIVE_OK) {
        throw ProtonCtlError(std::string("Archive read failed: ") + archive_error_string(a));
      }
      if (archive_write_data_block(ext, buff, size, offset) != ARCHIVE_OK) {
        throw ProtonCtlError(std::string("Archive write failed: ") + archive_error_string(ext));
      }
    }
  }
  if (r != ARCHIVE_EOF) {
    std::string err = archive_error_string(a);
    archive_read_free(a);
    archive_write_free(ext);
    throw ProtonCtlError("Archive extraction error: " + err);
  }

  archive_read_free(a);
  archive_write_free(ext);
  return top_level_dir;
}

}  // namespace

nlohmann::json ProtonRepository::ToJson() const {
  return {{"owner", owner},
          {"repo", repo},
          {"display_name", display_name},
          {"is_builtin", is_builtin},
          {"build_command_override", build_command_override}};
}

ProtonRepository ProtonRepository::FromJson(const nlohmann::json& j) {
  ProtonRepository r;
  r.owner = j.value("owner", "");
  r.repo = j.value("repo", "");
  r.display_name = j.value("display_name", r.owner + "/" + r.repo);
  r.is_builtin = j.value("is_builtin", false);
  r.build_command_override = j.value("build_command_override", "");
  return r;
}

nlohmann::json InstalledProtonVersion::ToJson() const {
  return {{"name", name},
          {"path", path.string()},
          {"source_repo", source_repo},
          {"version_tag", version_tag},
          {"installed_at", installed_at}};
}

ProtonRepositoryManager::ProtonRepositoryManager(std::string github_token)
    : github_(std::move(github_token)) {}

fs::path ProtonRepositoryManager::RepositoryRegistryPath() {
  fs::path dir = ConfigManager::ConfigFilePath().parent_path();
  return dir / "repositories.json";
}

std::vector<ProtonRepository> ProtonRepositoryManager::ListRepositories() const {
  fs::path path = RepositoryRegistryPath();
  std::ifstream in(path);

  std::vector<ProtonRepository> repos;
  if (in.is_open()) {
    try {
      nlohmann::json j;
      in >> j;
      for (const auto& item : j) repos.push_back(ProtonRepository::FromJson(item));
      return repos;
    } catch (const std::exception&) {
      // Fall through to seed defaults if the registry is corrupt.
    }
  }

  // Seed with the canonical GE-Proton repository on first run.
  ProtonRepository ge_proton;
  ge_proton.owner = "GloriousEggroll";
  ge_proton.repo = "proton-ge-custom";
  ge_proton.display_name = "GE-Proton";
  ge_proton.is_builtin = true;
  repos.push_back(ge_proton);
  SaveRepositories(repos);
  return repos;
}

void ProtonRepositoryManager::SaveRepositories(const std::vector<ProtonRepository>& repos) const {
  nlohmann::json j = nlohmann::json::array();
  for (const auto& r : repos) j.push_back(r.ToJson());

  fs::path path = RepositoryRegistryPath();
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::trunc);
  if (!out.is_open()) throw ProtonCtlError("Unable to write repository registry");
  out << j.dump(2);
}

ProtonRepository ProtonRepositoryManager::AddCustomRepository(const std::string& owner,
                                                               const std::string& repo) {
  // Validate the repo exists and is reachable before persisting it.
  nlohmann::json meta = github_.FetchRepoMetadata(owner, repo);

  auto repos = ListRepositories();
  auto existing = std::find_if(repos.begin(), repos.end(), [&](const ProtonRepository& r) {
    return r.owner == owner && r.repo == repo;
  });
  if (existing != repos.end()) return *existing;

  ProtonRepository new_repo;
  new_repo.owner = owner;
  new_repo.repo = repo;
  new_repo.display_name = meta.value("full_name", owner + "/" + repo);
  new_repo.is_builtin = false;

  repos.push_back(new_repo);
  SaveRepositories(repos);
  return new_repo;
}

void ProtonRepositoryManager::RemoveRepository(const std::string& owner, const std::string& repo) {
  auto repos = ListRepositories();
  repos.erase(std::remove_if(repos.begin(), repos.end(),
                              [&](const ProtonRepository& r) {
                                return r.owner == owner && r.repo == repo && !r.is_builtin;
                              }),
              repos.end());
  SaveRepositories(repos);
}

std::vector<GitHubRelease> ProtonRepositoryManager::FetchAvailableReleases(
    const ProtonRepository& repo) const {
  return github_.FetchReleases(repo.owner, repo.repo);
}

std::optional<GitHubRelease> ProtonRepositoryManager::CheckForUpdate(
    const ProtonRepository& repo, const std::string& installed_version_tag) const {
  std::vector<GitHubRelease> releases = github_.FetchReleases(repo.owner, repo.repo);
  if (releases.empty()) return std::nullopt;

  // GitHub's /releases endpoint returns newest-first by default.
  const GitHubRelease& latest = releases.front();
  if (!installed_version_tag.empty() && latest.tag_name == installed_version_tag) return std::nullopt;
  return latest;
}

const GitHubAsset* ProtonRepositoryManager::PickBestAsset(const GitHubRelease& release) const {
  for (const auto& asset : release.assets) {
    if (HasArchiveExtension(asset.name)) return &asset;
  }
  return nullptr;
}

void ProtonRepositoryManager::MarkInstalled(const fs::path& install_dir,
                                             const std::string& source_repo,
                                             const std::string& version_tag) {
  nlohmann::json j = {
      {"installed_at", NowIso8601()}, {"source_repo", source_repo}, {"version_tag", version_tag}};
  std::ofstream marker(install_dir / kInstalledAtMarkerName, std::ios::trunc);
  if (marker.is_open()) marker << j.dump();
}

InstalledMetadata ProtonRepositoryManager::ReadInstalledMetadata(const fs::path& install_dir) {
  std::ifstream marker(install_dir / kInstalledAtMarkerName);
  if (!marker.is_open()) return {};

  std::string content((std::istreambuf_iterator<char>(marker)), std::istreambuf_iterator<char>());
  try {
    nlohmann::json j = nlohmann::json::parse(content);
    return InstalledMetadata{j.value("installed_at", ""), j.value("source_repo", ""),
                              j.value("version_tag", "")};
  } catch (const nlohmann::json::exception&) {
    // Legacy marker format: a bare ISO-8601 timestamp with no repo info.
    return InstalledMetadata{content, "", ""};
  }
}

InstalledProtonVersion ProtonRepositoryManager::InstallRelease(
    const ProtonRepository& repo, const GitHubRelease& release, const fs::path& compat_tools_dir,
    const DownloadProgressCallback& on_progress) const {
  const GitHubAsset* asset = PickBestAsset(release);
  if (!asset) {
    throw ProtonCtlError("Release " + release.tag_name + " has no recognizable tar archive asset");
  }

  std::error_code ec;
  fs::create_directories(compat_tools_dir, ec);

  fs::path scratch_dir = fs::temp_directory_path() / ("protonctl-dl-" + release.tag_name);
  fs::create_directories(scratch_dir, ec);
  fs::path archive_path = scratch_dir / asset->name;

  github_.DownloadAsset(asset->browser_download_url, archive_path, on_progress);

  std::string top_level = ExtractTarball(archive_path, compat_tools_dir);

  std::error_code rm_ec;
  fs::remove_all(scratch_dir, rm_ec);

  InstalledProtonVersion installed;
  installed.name = top_level.empty() ? release.tag_name : top_level;
  installed.path = compat_tools_dir / installed.name;
  installed.source_repo = repo.FullName();
  installed.version_tag = release.tag_name;
  MarkInstalled(installed.path, installed.source_repo, installed.version_tag);
  installed.installed_at = ReadInstalledMetadata(installed.path).installed_at;
  return installed;
}

std::vector<InstalledProtonVersion> ProtonRepositoryManager::ListInstalledVersions(
    const fs::path& compat_tools_dir) const {
  std::vector<InstalledProtonVersion> result;
  std::error_code ec;
  if (!fs::is_directory(compat_tools_dir, ec)) return result;

  for (const auto& entry : fs::directory_iterator(compat_tools_dir, ec)) {
    if (!entry.is_directory()) continue;

    InstalledProtonVersion v;
    v.name = entry.path().filename().string();
    v.path = entry.path();

    InstalledMetadata metadata = ReadInstalledMetadata(entry.path());
    v.installed_at = metadata.installed_at;
    v.source_repo = metadata.source_repo;

    if (!metadata.version_tag.empty()) {
      // The exact tag/ref recorded at install time - reliable for comparing
      // against GitHub release tags (see CheckForUpdate).
      v.version_tag = metadata.version_tag;
    } else {
      // Legacy/manually-placed installs with no marker: best-effort guess
      // from Proton's own `version` file, which typically holds
      // "<build-id> <tag>" - not necessarily equal to a GitHub tag, so this
      // is only ever a fallback for display, never used for update checks.
      std::ifstream vf(entry.path() / "version");
      if (vf.is_open()) std::getline(vf, v.version_tag);
    }
    result.push_back(v);
  }
  return result;
}

void ProtonRepositoryManager::RemoveInstalledVersion(const fs::path& compat_tools_dir,
                                                      const std::string& name) const {
  fs::path target = compat_tools_dir / name;
  std::error_code ec;
  // Guard against `name` escaping the compat tools directory via `..`.
  fs::path canonical_target = fs::weakly_canonical(target, ec);
  fs::path canonical_root = fs::weakly_canonical(compat_tools_dir, ec);
  if (canonical_target.string().rfind(canonical_root.string(), 0) != 0) {
    throw ProtonCtlError("Refusing to remove path outside compatibilitytools.d");
  }
  fs::remove_all(target, ec);
  if (ec) throw ProtonCtlError("Failed to remove " + target.string() + ": " + ec.message());
}

}  // namespace protonctl
