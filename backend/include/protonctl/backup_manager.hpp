#pragma once

#include <archive.h>

#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "protonctl/common.hpp"

namespace protonctl {

// Metadata manifest embedded as `manifest.json` (the first entry) inside
// every `.prtbak` archive.
struct PrtBakManifest {
  std::string format_version = "1";
  std::string proton_version;         // e.g. "GE-Proton9-20"
  std::string source_repo;            // "owner/repo", if known
  fs::path proton_dir_name;           // directory name under compatibilitytools.d
  fs::path wine_prefix_relative;      // relative path of the prefix inside the archive
  std::vector<std::string> dependencies;  // e.g. dxvk, vkd3d-proton versions bundled
  std::string created_at;             // ISO-8601 UTC
  std::string sha256_proton_dir;      // best-effort content checksum

  nlohmann::json ToJson() const;
  static PrtBakManifest FromJson(const nlohmann::json& j);
};

// One entry in a `CompatToolsBackupManifest` - a single Proton version
// that was under compatibilitytools.d at backup time.
struct CompatToolsEntry {
  std::string name;         // directory name under compatibilitytools.d
  std::string source_repo;  // "owner/repo", if known
  std::string version_tag;  // release tag, if known

  nlohmann::json ToJson() const;
  static CompatToolsEntry FromJson(const nlohmann::json& j);
};

// Metadata manifest for a *whole-library* backup: every installed Proton
// version under compatibilitytools.d packed into one archive, so the
// entire collection can be restored on another machine/instance in one
// shot, rather than backing up one version (plus its wine prefix) at a
// time like `PrtBakManifest`/`CreateBackup` do.
struct CompatToolsBackupManifest {
  std::string format_version = "1";
  std::string created_at;  // ISO-8601 UTC
  std::vector<CompatToolsEntry> tools;

  nlohmann::json ToJson() const;
  static CompatToolsBackupManifest FromJson(const nlohmann::json& j);
};

using BackupProgressCallback = std::function<void(const std::string& current_path)>;

// Creates/reads/restores `.prtbak` archives. Built in two explicit steps:
// the Proton install (and prefix, or every installed tool for a library
// backup) is first packed into an intermediate .zip, then that .zip plus
// `manifest.json` are wrapped together into an outer tar+gzip archive -
// this is the on-disk format, not just an implementation detail, so
// `payload.zip` is a real entry name other tools reading a `.prtbak` can
// rely on.
class BackupManager {
 public:
  static void CreateBackup(const fs::path& proton_dir,
                            const fs::path& prefix_dir,  // may be empty to skip prefix
                            PrtBakManifest manifest,
                            const fs::path& out_file,
                            int compression_level = 6,
                            const BackupProgressCallback& on_progress = nullptr);

  // Reads only the manifest entry without extracting the rest of the
  // archive; used to populate backup list UIs cheaply. Throws if
  // `prtbak_file` is actually a library backup (see below).
  static PrtBakManifest ReadManifest(const fs::path& prtbak_file);

  // Extracts the full archive under `restore_root`, recreating
  // `<restore_root>/<proton_dir_name>` and, if present,
  // `<restore_root>/<wine_prefix_relative>`.
  static void RestoreBackup(const fs::path& prtbak_file, const fs::path& restore_root,
                             const BackupProgressCallback& on_progress = nullptr);

  // Backs up every installed Proton version under `compat_tools_dir` into
  // one archive - for moving a whole Proton collection to another
  // instance, as opposed to `CreateBackup`'s one-version-plus-prefix scope.
  static void CreateLibraryBackup(const fs::path& compat_tools_dir,
                                   const fs::path& out_file,
                                   int compression_level = 6,
                                   const BackupProgressCallback& on_progress = nullptr);

  // Throws if `prtbak_file` is actually a single-version backup.
  static CompatToolsBackupManifest ReadLibraryManifest(const fs::path& prtbak_file);

  // Extracts every tool back into `compat_tools_dir` (created if needed),
  // overwriting any existing directory of the same name.
  static void RestoreLibraryBackup(const fs::path& prtbak_file, const fs::path& compat_tools_dir,
                                    const BackupProgressCallback& on_progress = nullptr);

 private:
  static void AddDirectoryToArchive(struct archive* archive, const fs::path& root,
                                     const std::string& archive_root_name,
                                     const BackupProgressCallback& on_progress);

  // Step one: packs `roots` (archive-root-name -> source directory pairs)
  // into a fresh .zip at `zip_out`.
  static void WriteZipPayload(const fs::path& zip_out,
                               const std::vector<std::pair<std::string, fs::path>>& roots,
                               int compression_level, const BackupProgressCallback& on_progress);

  // Step two: wraps `payload_zip` plus a `manifest.json` entry into a
  // tar+gzip archive at `out_file`.
  static void WrapPayloadInTarGz(const fs::path& payload_zip, const std::string& manifest_json,
                                  const fs::path& out_file, int compression_level);

  // Re-opens a just-written archive and reads every entry's data through to
  // completion. libarchive's writers never surface a short write as an
  // error from archive_write_data (it just returns however many bytes it
  // accepted), so a header declaring a size that the following data doesn't
  // actually satisfy - a torn write, a file that shrank between being
  // stat'd and read, a disk that filled up mid-archive - produces a file
  // that "successfully" finishes writing but is truncated garbage on
  // restore. This catches that at backup time instead of at restore time.
  static void VerifyArchiveIntegrity(const fs::path& file);

  // Extracts every entry from an already-opened archive `a` into
  // `restore_root`, wiping and permission-fixing conflicting pre-existing
  // top-level destinations first (see RestoreBackup). Frees `a` before
  // returning or throwing, regardless of outcome.
  static void ExtractArchiveEntries(struct archive* a, const fs::path& restore_root,
                                     const BackupProgressCallback& on_progress);
};

}  // namespace protonctl
