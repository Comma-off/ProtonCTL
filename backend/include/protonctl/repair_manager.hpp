#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "protonctl/common.hpp"

namespace protonctl {

enum class RepairIssueType {
  BrokenSymlink,
  MissingRuntime,
  CorruptManifest,
  EmptyProtonDir,
  OrphanedPrefix,
};

NLOHMANN_JSON_SERIALIZE_ENUM(RepairIssueType, {
  {RepairIssueType::BrokenSymlink, "broken_symlink"},
  {RepairIssueType::MissingRuntime, "missing_runtime"},
  {RepairIssueType::CorruptManifest, "corrupt_manifest"},
  {RepairIssueType::EmptyProtonDir, "empty_proton_dir"},
  {RepairIssueType::OrphanedPrefix, "orphaned_prefix"},
})

struct RepairIssue {
  RepairIssueType type;
  fs::path path;
  std::string description;
  bool fixable = false;

  nlohmann::json ToJson() const;
};

// Scans `compatibilitytools.d` and `steamapps/compatdata` for common
// breakage: dangling symlinks (e.g. a Proton build that references a
// runtime that was since removed), directories missing their `proton`
// launcher script, unreadable/corrupt `version`/manifest files, and wine
// prefixes whose owning Proton install no longer exists.
class RepairManager {
 public:
  static std::vector<RepairIssue> Scan(const fs::path& compat_tools_dir,
                                        const fs::path& compat_data_dir);

  // Applies the fix appropriate to `issue.type`:
  //   - BrokenSymlink: removes the dangling link.
  //   - EmptyProtonDir: removes the empty directory.
  //   - OrphanedPrefix: leaves data intact but moves the prefix under
  //     `<compat_data_dir>/.orphaned/` for the user to review before delete.
  // Returns false (and leaves the issue untouched) for types that require
  // a reinstall rather than an automatic fix (MissingRuntime, CorruptManifest).
  static bool Fix(const RepairIssue& issue);

 private:
  static bool HasBrokenSymlinks(const fs::path& dir, std::vector<RepairIssue>& out);
  static bool HasProtonLauncher(const fs::path& proton_dir);
};

}  // namespace protonctl
