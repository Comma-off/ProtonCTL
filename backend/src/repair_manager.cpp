#include "protonctl/repair_manager.hpp"

#include <unistd.h>

namespace protonctl {

nlohmann::json RepairIssue::ToJson() const {
  return {
      {"type", type},
      {"path", path.string()},
      {"description", description},
      {"fixable", fixable},
  };
}

bool RepairManager::HasProtonLauncher(const fs::path& proton_dir) {
  std::error_code ec;
  return fs::exists(proton_dir / "proton", ec) || fs::exists(proton_dir / "files" / "bin" / "wine", ec);
}

bool RepairManager::HasBrokenSymlinks(const fs::path& dir, std::vector<RepairIssue>& out) {
  std::error_code ec;
  bool found_any = false;
  for (auto it = fs::recursive_directory_iterator(
           dir, fs::directory_options::skip_permission_denied, ec);
       it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (ec) continue;
    if (!it->is_symlink(ec)) continue;

    std::error_code target_ec;
    fs::path resolved = fs::read_symlink(it->path(), target_ec);
    fs::path absolute_target =
        resolved.is_absolute() ? resolved : (it->path().parent_path() / resolved);

    if (!fs::exists(absolute_target, target_ec)) {
      RepairIssue issue;
      issue.type = RepairIssueType::BrokenSymlink;
      issue.path = it->path();
      issue.description = "Dangling symlink -> " + resolved.string();
      issue.fixable = true;
      out.push_back(issue);
      found_any = true;
    }
  }
  return found_any;
}

std::vector<RepairIssue> RepairManager::Scan(const fs::path& compat_tools_dir,
                                              const fs::path& compat_data_dir) {
  std::vector<RepairIssue> issues;
  std::error_code ec;

  if (fs::is_directory(compat_tools_dir, ec)) {
    HasBrokenSymlinks(compat_tools_dir, issues);

    for (const auto& entry : fs::directory_iterator(compat_tools_dir, ec)) {
      if (!entry.is_directory()) continue;

      bool is_empty = fs::is_empty(entry.path(), ec);
      if (is_empty) {
        RepairIssue issue;
        issue.type = RepairIssueType::EmptyProtonDir;
        issue.path = entry.path();
        issue.description = "Proton directory is empty (likely an interrupted install)";
        issue.fixable = true;
        issues.push_back(issue);
        continue;
      }

      if (!HasProtonLauncher(entry.path())) {
        RepairIssue issue;
        issue.type = RepairIssueType::MissingRuntime;
        issue.path = entry.path();
        issue.description = "No `proton` launcher or wine binary found; the build may be incomplete";
        issue.fixable = false;
        issues.push_back(issue);
      }

      fs::path version_file = entry.path() / "version";
      if (fs::exists(version_file, ec) && fs::is_empty(version_file, ec)) {
        RepairIssue issue;
        issue.type = RepairIssueType::CorruptManifest;
        issue.path = version_file;
        issue.description = "version file exists but is empty";
        issue.fixable = false;
        issues.push_back(issue);
      }
    }
  }

  if (fs::is_directory(compat_data_dir, ec)) {
    for (const auto& entry : fs::directory_iterator(compat_data_dir, ec)) {
      if (!entry.is_directory()) continue;
      fs::path pfx = entry.path() / "pfx";
      fs::path config_info = entry.path() / "config_info";
      if (fs::is_directory(pfx, ec) && !fs::exists(config_info, ec)) {
        // A prefix with no config_info metadata usually means the owning
        // Steam app entry (and therefore the Proton version it points to)
        // no longer exists.
        RepairIssue issue;
        issue.type = RepairIssueType::OrphanedPrefix;
        issue.path = entry.path();
        issue.description = "Wine prefix has no associated Steam app metadata";
        issue.fixable = true;
        issues.push_back(issue);
      }
    }
  }

  return issues;
}

bool RepairManager::Fix(const RepairIssue& issue) {
  std::error_code ec;
  switch (issue.type) {
    case RepairIssueType::BrokenSymlink:
      fs::remove(issue.path, ec);
      return !ec;

    case RepairIssueType::EmptyProtonDir:
      fs::remove_all(issue.path, ec);
      return !ec;

    case RepairIssueType::OrphanedPrefix: {
      fs::path orphaned_dir = issue.path.parent_path() / ".orphaned";
      fs::create_directories(orphaned_dir, ec);
      fs::path dest = orphaned_dir / issue.path.filename();
      fs::rename(issue.path, dest, ec);
      return !ec;
    }

    case RepairIssueType::MissingRuntime:
    case RepairIssueType::CorruptManifest:
    default:
      // These require a reinstall/rebuild rather than an in-place fix.
      return false;
  }
}

}  // namespace protonctl
