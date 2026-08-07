#include "protonctl/backup_manager.hpp"

#include <archive.h>
#include <archive_entry.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <vector>

#include "protonctl/proton_repository_manager.hpp"

namespace protonctl {

nlohmann::json PrtBakManifest::ToJson() const {
  return {
      {"format_version", format_version},
      {"proton_version", proton_version},
      {"source_repo", source_repo},
      {"proton_dir_name", proton_dir_name.string()},
      {"wine_prefix_relative", wine_prefix_relative.string()},
      {"dependencies", dependencies},
      {"created_at", created_at},
      {"sha256_proton_dir", sha256_proton_dir},
  };
}

PrtBakManifest PrtBakManifest::FromJson(const nlohmann::json& j) {
  PrtBakManifest m;
  m.format_version = j.value("format_version", std::string("1"));
  m.proton_version = j.value("proton_version", "");
  m.source_repo = j.value("source_repo", "");
  m.proton_dir_name = fs::path(j.value("proton_dir_name", ""));
  m.wine_prefix_relative = fs::path(j.value("wine_prefix_relative", ""));
  if (j.contains("dependencies")) m.dependencies = j.at("dependencies").get<std::vector<std::string>>();
  m.created_at = j.value("created_at", "");
  m.sha256_proton_dir = j.value("sha256_proton_dir", "");
  return m;
}

nlohmann::json CompatToolsEntry::ToJson() const {
  return {{"name", name}, {"source_repo", source_repo}, {"version_tag", version_tag}};
}

CompatToolsEntry CompatToolsEntry::FromJson(const nlohmann::json& j) {
  CompatToolsEntry e;
  e.name = j.value("name", "");
  e.source_repo = j.value("source_repo", "");
  e.version_tag = j.value("version_tag", "");
  return e;
}

nlohmann::json CompatToolsBackupManifest::ToJson() const {
  nlohmann::json tools_json = nlohmann::json::array();
  for (const auto& tool : tools) tools_json.push_back(tool.ToJson());
  return {{"format_version", format_version}, {"created_at", created_at}, {"tools", tools_json}};
}

CompatToolsBackupManifest CompatToolsBackupManifest::FromJson(const nlohmann::json& j) {
  CompatToolsBackupManifest m;
  m.format_version = j.value("format_version", std::string("1"));
  m.created_at = j.value("created_at", "");
  if (j.contains("tools")) {
    for (const auto& item : j.at("tools")) m.tools.push_back(CompatToolsEntry::FromJson(item));
  }
  return m;
}

namespace {

void CheckArchiveWriteHeader(struct archive* a, struct archive_entry* entry) {
  if (archive_write_header(a, entry) != ARCHIVE_OK) {
    std::string err = archive_error_string(a);
    archive_entry_free(entry);
    throw ProtonCtlError("Failed to write archive entry: " + err);
  }
}

// archive_write_data() may accept fewer bytes than requested in one call
// without that being an error - the caller has to loop and check the total
// against what it meant to write, otherwise a short write silently produces
// an entry whose declared header size doesn't match its actual data, which
// only surfaces later as "truncated archive" on restore.
void WriteAllData(struct archive* a, const char* data, size_t size) {
  size_t written = 0;
  while (written < size) {
    la_ssize_t n = archive_write_data(a, data + written, size - written);
    if (n < 0) throw ProtonCtlError(std::string("Failed to write archive data: ") + archive_error_string(a));
    if (n == 0) throw ProtonCtlError("Failed to write archive data: short write (disk full?)");
    written += static_cast<size_t>(n);
  }
}

void WriteEntryFromBuffer(struct archive* a, const std::string& archive_path,
                           const std::string& content) {
  struct archive_entry* entry = archive_entry_new();
  archive_entry_set_pathname(entry, archive_path.c_str());
  archive_entry_set_size(entry, static_cast<la_int64_t>(content.size()));
  archive_entry_set_filetype(entry, AE_IFREG);
  archive_entry_set_perm(entry, 0644);
  CheckArchiveWriteHeader(a, entry);
  WriteAllData(a, content.data(), content.size());
  archive_entry_free(entry);
}

void WriteEntryFromFile(struct archive* a, const fs::path& file_path,
                         const std::string& archive_path) {
  std::error_code ec;
  bool is_symlink = fs::is_symlink(file_path, ec);

  struct archive_entry* entry = archive_entry_new();
  archive_entry_set_pathname(entry, archive_path.c_str());

  if (is_symlink) {
    fs::path target = fs::read_symlink(file_path, ec);
    archive_entry_set_filetype(entry, AE_IFLNK);
    archive_entry_set_symlink(entry, target.c_str());
    archive_entry_set_perm(entry, 0777);
    CheckArchiveWriteHeader(a, entry);
    archive_entry_free(entry);
    return;
  }

  std::ifstream in(file_path, std::ios::binary);
  if (!in.is_open()) {
    archive_entry_free(entry);
    return;  // unreadable file (permissions, broken link target); skip rather than abort backup
  }
  in.seekg(0, std::ios::end);
  auto size = in.tellg();
  in.seekg(0, std::ios::beg);

  archive_entry_set_filetype(entry, AE_IFREG);
  archive_entry_set_perm(entry, 0644);
  archive_entry_set_size(entry, static_cast<la_int64_t>(size));
  CheckArchiveWriteHeader(a, entry);

  // The header above already committed to writing exactly `size` bytes.
  // If the file is concurrently modified/truncated and ends up shorter than
  // that (e.g. it shrank between the stat above and this read), pad with
  // zeroes rather than leaving the entry short - a short entry desyncs the
  // tar block framing for everything written after it, which is exactly
  // what produces "truncated archive" errors on unrelated later entries.
  std::vector<char> buffer(1 << 16);
  la_int64_t remaining = static_cast<la_int64_t>(size);
  while (remaining > 0) {
    std::streamsize to_read = static_cast<std::streamsize>(std::min<la_int64_t>(remaining, static_cast<la_int64_t>(buffer.size())));
    in.read(buffer.data(), to_read);
    std::streamsize n = in.gcount();
    if (n <= 0) {
      // Underlying file ended early - zero-fill the rest so the entry still
      // matches the size declared in its header.
      std::fill(buffer.begin(), buffer.end(), 0);
      n = to_read;
    }
    WriteAllData(a, buffer.data(), static_cast<size_t>(n));
    remaining -= n;
  }
  archive_entry_free(entry);
}

// Deleting a file only requires write+execute permission on its *parent*
// directory, not on the file itself - but upstream Proton/Wine release
// tarballs sometimes preserve read-only (no write bit) directories from the
// original archive. A read-only directory anywhere in a tree blocks
// removing anything under it, which silently leaves stale, conflicting
// content behind even after an attempted cleanup - since we own these
// files, granting ourselves owner rwx back is always allowed regardless of
// the current permission bits, so this guarantees the tree is removable.
void MakeTreeRemovable(const fs::path& root) {
  std::error_code ec;
  if (!fs::is_directory(root, ec)) return;
  fs::permissions(root, fs::perms::owner_all, fs::perm_options::add, ec);
  for (auto it = fs::recursive_directory_iterator(
           root, fs::directory_options::skip_permission_denied, ec);
       it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (ec) continue;
    if (it->is_directory(ec) && !it->is_symlink(ec)) {
      fs::permissions(it->path(), fs::perms::owner_all, fs::perm_options::add, ec);
    }
  }
}

}  // namespace

void BackupManager::AddDirectoryToArchive(struct archive* archive, const fs::path& root,
                                           const std::string& archive_root_name,
                                           const BackupProgressCallback& on_progress) {
  std::error_code ec;
  if (!fs::exists(root, ec)) return;

  // Directory entry for the root itself.
  {
    struct archive_entry* dir_entry = archive_entry_new();
    archive_entry_set_pathname(dir_entry, (archive_root_name + "/").c_str());
    archive_entry_set_filetype(dir_entry, AE_IFDIR);
    archive_entry_set_perm(dir_entry, 0755);
    CheckArchiveWriteHeader(archive, dir_entry);
    archive_entry_free(dir_entry);
  }

  for (auto it = fs::recursive_directory_iterator(
           root, fs::directory_options::skip_permission_denied, ec);
       it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (ec) continue;
    const fs::path& entry_path = it->path();
    fs::path relative = fs::relative(entry_path, root, ec);
    std::string archive_path = archive_root_name + "/" + relative.generic_string();

    if (on_progress) on_progress(entry_path.string());

    if (it->is_directory(ec) && !it->is_symlink(ec)) {
      struct archive_entry* dir_entry = archive_entry_new();
      archive_entry_set_pathname(dir_entry, (archive_path + "/").c_str());
      archive_entry_set_filetype(dir_entry, AE_IFDIR);
      archive_entry_set_perm(dir_entry, 0755);
      CheckArchiveWriteHeader(archive, dir_entry);
      archive_entry_free(dir_entry);
    } else {
      WriteEntryFromFile(archive, entry_path, archive_path);
    }
  }
}

void BackupManager::VerifyArchiveIntegrity(const fs::path& file) {
  struct archive* a = archive_read_new();
  archive_read_support_format_all(a);  // used for both the inner .zip and the outer tar+gzip
  archive_read_support_filter_all(a);

  if (archive_read_open_filename(a, file.c_str(), 1 << 20) != ARCHIVE_OK) {
    std::string err = archive_error_string(a);
    archive_read_free(a);
    throw ProtonCtlError("Backup verification failed: could not reopen " + file.string() + ": " + err);
  }

  struct archive_entry* entry;
  int r;
  while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
    // Deliberately using the same archive_read_data_block loop RestoreBackup
    // uses (rather than archive_read_data_skip) so verification exercises
    // byte-for-byte the identical read path a real restore takes - no risk
    // of skip taking some other internal shortcut that a block read wouldn't.
    const void* buff;
    size_t size;
    la_int64_t offset;
    while (true) {
      int rr = archive_read_data_block(a, &buff, &size, &offset);
      if (rr == ARCHIVE_EOF) break;
      if (rr != ARCHIVE_OK) {
        std::string err = archive_error_string(a);
        archive_read_free(a);
        throw ProtonCtlError("Backup verification failed: " + err);
      }
    }
  }
  if (r != ARCHIVE_EOF) {
    std::string err = archive_error_string(a);
    archive_read_free(a);
    throw ProtonCtlError("Backup verification failed: " + err);
  }

  archive_read_free(a);
}

void BackupManager::WriteZipPayload(const fs::path& zip_out,
                                     const std::vector<std::pair<std::string, fs::path>>& roots,
                                     int compression_level, const BackupProgressCallback& on_progress) {
  struct archive* a = archive_write_new();
  archive_write_set_format_zip(a);
  // Zip's own per-entry deflate compression, not an external filter - level
  // is clamped since deflate (unlike zstd) only understands 0-9.
  int level = std::clamp(compression_level, 1, 9);
  archive_write_set_options(a, ("compression-level=" + std::to_string(level)).c_str());

  std::error_code ec;
  fs::create_directories(zip_out.parent_path(), ec);

  if (archive_write_open_filename(a, zip_out.c_str()) != ARCHIVE_OK) {
    std::string err = archive_error_string(a);
    archive_write_free(a);
    throw ProtonCtlError("Failed to open " + zip_out.string() + " for writing: " + err);
  }

  try {
    for (const auto& [archive_root_name, source_dir] : roots) {
      AddDirectoryToArchive(a, source_dir, archive_root_name, on_progress);
    }
  } catch (...) {
    archive_write_free(a);
    std::error_code rm_ec;
    fs::remove(zip_out, rm_ec);
    throw;
  }

  if (archive_write_close(a) != ARCHIVE_OK) {
    std::string err = archive_error_string(a);
    archive_write_free(a);
    std::error_code rm_ec;
    fs::remove(zip_out, rm_ec);
    throw ProtonCtlError("Failed to finalize " + zip_out.string() + ": " + err);
  }
  archive_write_free(a);
}

void BackupManager::WrapPayloadInTarGz(const fs::path& payload_zip, const std::string& manifest_json,
                                        const fs::path& out_file, int compression_level) {
  struct archive* a = archive_write_new();
  archive_write_set_format_pax_restricted(a);
  archive_write_add_filter_gzip(a);
  int level = std::clamp(compression_level, 1, 9);
  archive_write_set_options(a, ("compression-level=" + std::to_string(level)).c_str());

  std::error_code ec;
  fs::create_directories(out_file.parent_path(), ec);

  if (archive_write_open_filename(a, out_file.c_str()) != ARCHIVE_OK) {
    std::string err = archive_error_string(a);
    archive_write_free(a);
    throw ProtonCtlError("Failed to open " + out_file.string() + " for writing: " + err);
  }

  try {
    WriteEntryFromBuffer(a, "manifest.json", manifest_json);
    // payload.zip is a real, documented entry name in the .prtbak format,
    // not an implementation detail - the whole backed-up tree lives here.
    WriteEntryFromFile(a, payload_zip, "payload.zip");
  } catch (...) {
    archive_write_free(a);
    std::error_code rm_ec;
    fs::remove(out_file, rm_ec);
    throw;
  }

  if (archive_write_close(a) != ARCHIVE_OK) {
    std::string err = archive_error_string(a);
    archive_write_free(a);
    std::error_code rm_ec;
    fs::remove(out_file, rm_ec);
    throw ProtonCtlError("Failed to finalize " + out_file.string() + ": " + err);
  }
  archive_write_free(a);
}

void BackupManager::CreateBackup(const fs::path& proton_dir, const fs::path& prefix_dir,
                                  PrtBakManifest manifest, const fs::path& out_file,
                                  int compression_level, const BackupProgressCallback& on_progress) {
  if (manifest.proton_dir_name.empty()) manifest.proton_dir_name = proton_dir.filename();
  if (!prefix_dir.empty() && manifest.wine_prefix_relative.empty()) {
    manifest.wine_prefix_relative = fs::path("prefix");
  }
  if (manifest.created_at.empty()) manifest.created_at = NowIso8601();

  std::vector<std::pair<std::string, fs::path>> roots = {
      {manifest.proton_dir_name.string(), proton_dir},
  };
  if (!prefix_dir.empty()) roots.push_back({manifest.wine_prefix_relative.string(), prefix_dir});

  fs::path temp_zip = fs::temp_directory_path() /
                       (out_file.filename().string() + ".payload-" + NowIso8601() + ".zip");
  std::error_code ec;

  try {
    WriteZipPayload(temp_zip, roots, compression_level, on_progress);
    VerifyArchiveIntegrity(temp_zip);
    WrapPayloadInTarGz(temp_zip, manifest.ToJson().dump(2), out_file, compression_level);
    VerifyArchiveIntegrity(out_file);
  } catch (...) {
    fs::remove(temp_zip, ec);
    fs::remove(out_file, ec);
    throw;
  }
  fs::remove(temp_zip, ec);
}

PrtBakManifest BackupManager::ReadManifest(const fs::path& prtbak_file) {
  struct archive* a = archive_read_new();
  archive_read_support_format_tar(a);
  archive_read_support_filter_all(a);

  if (archive_read_open_filename(a, prtbak_file.c_str(), 1 << 20) != ARCHIVE_OK) {
    std::string err = archive_error_string(a);
    archive_read_free(a);
    throw ProtonCtlError("Failed to open .prtbak: " + err);
  }

  struct archive_entry* entry;
  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    std::string path = archive_entry_pathname(entry);
    if (path == "manifest.json") {
      std::ostringstream oss;
      char buf[8192];
      la_ssize_t n;
      while ((n = archive_read_data(a, buf, sizeof(buf))) > 0) oss.write(buf, n);
      archive_read_free(a);
      nlohmann::json j = nlohmann::json::parse(oss.str());
      if (j.contains("tools")) {
        throw ProtonCtlError(prtbak_file.string() +
                              " is a whole-library backup - use the library restore instead");
      }
      return PrtBakManifest::FromJson(j);
    }
    archive_read_data_skip(a);
  }

  archive_read_free(a);
  throw ProtonCtlError("Archive does not contain a manifest.json entry: " + prtbak_file.string());
}

void BackupManager::ExtractArchiveEntries(struct archive* a, const fs::path& restore_root,
                                           const BackupProgressCallback& on_progress) {
  struct archive* ext = archive_write_disk_new();
  archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM |
                                           ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS |
                                           ARCHIVE_EXTRACT_SECURE_NODOTDOT);

  std::error_code ec;
  fs::create_directories(restore_root, ec);

  // Restoring is documented as *replacing* whatever's already at each
  // top-level destination (a whole tool directory for a library restore, or
  // proton_dir_name/wine_prefix_relative for a single-version restore) -
  // but extracting entry-by-entry over pre-existing content relies on every
  // entry's type already matching what's on disk. It doesn't: restoring a
  // different version of the same-named tool, or content left behind by an
  // earlier interrupted restore, can easily put a file/symlink where the
  // old tree has a directory (or vice versa), which archive_write_disk
  // refuses to resolve on its own ("Can't replace existing directory with
  // non-directory"). So each top-level destination is wiped once, the first
  // time an entry under it is seen, before anything is extracted into it.
  std::unordered_set<std::string> cleared_roots;

  struct archive_entry* entry;
  int r;
  try {
    while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
      std::string entry_path = archive_entry_pathname(entry);

      std::string top_level = entry_path.substr(0, entry_path.find('/'));
      if (cleared_roots.insert(top_level).second) {
        fs::path target = restore_root / top_level;
        MakeTreeRemovable(target);
        fs::remove_all(target, ec);
      }

      if (on_progress) on_progress(entry_path);

      fs::path full_dest = restore_root / entry_path;
      archive_entry_set_pathname(entry, full_dest.c_str());

      if (archive_write_header(ext, entry) != ARCHIVE_OK) {
        throw ProtonCtlError(std::string("Restore write header failed: ") + archive_error_string(ext));
      }

      const void* buff;
      size_t size;
      la_int64_t offset;
      while (true) {
        int rr = archive_read_data_block(a, &buff, &size, &offset);
        if (rr == ARCHIVE_EOF) break;
        if (rr != ARCHIVE_OK) throw ProtonCtlError(std::string("Restore read failed: ") + archive_error_string(a));
        archive_write_data_block(ext, buff, size, offset);
      }
    }
    if (r != ARCHIVE_EOF) {
      throw ProtonCtlError("Restore extraction error: " + std::string(archive_error_string(a)));
    }
  } catch (...) {
    archive_read_free(a);
    archive_write_free(ext);
    throw;
  }

  archive_read_free(a);
  archive_write_free(ext);
}

void BackupManager::RestoreBackup(const fs::path& prtbak_file, const fs::path& restore_root,
                                   const BackupProgressCallback& on_progress) {
  // Step one: pull payload.zip (the actual backed-up tree) back out of the
  // outer tar+gzip container into a temp file - it has to be a real seekable
  // file for the zip reader below, not a stream.
  struct archive* outer = archive_read_new();
  archive_read_support_format_tar(outer);
  archive_read_support_filter_all(outer);

  if (archive_read_open_filename(outer, prtbak_file.c_str(), 1 << 20) != ARCHIVE_OK) {
    std::string err = archive_error_string(outer);
    archive_read_free(outer);
    throw ProtonCtlError("Failed to open .prtbak: " + err);
  }

  fs::path temp_zip = fs::temp_directory_path() / (prtbak_file.filename().string() + ".payload.zip");
  std::error_code ec;
  bool found_payload = false;

  struct archive_entry* entry;
  int r;
  try {
    while ((r = archive_read_next_header(outer, &entry)) == ARCHIVE_OK) {
      std::string path = archive_entry_pathname(entry);
      if (path != "payload.zip") {
        archive_read_data_skip(outer);
        continue;
      }

      std::ofstream out(temp_zip, std::ios::binary | std::ios::trunc);
      if (!out.is_open()) throw ProtonCtlError("Failed to open " + temp_zip.string() + " for writing");

      const void* buff;
      size_t size;
      la_int64_t offset;
      while (true) {
        int rr = archive_read_data_block(outer, &buff, &size, &offset);
        if (rr == ARCHIVE_EOF) break;
        if (rr != ARCHIVE_OK) throw ProtonCtlError(std::string("Restore read failed: ") + archive_error_string(outer));
        out.write(reinterpret_cast<const char*>(buff), static_cast<std::streamsize>(size));
      }
      found_payload = true;
    }
    if (r != ARCHIVE_EOF) {
      throw ProtonCtlError("Restore extraction error: " + std::string(archive_error_string(outer)));
    }
  } catch (...) {
    archive_read_free(outer);
    fs::remove(temp_zip, ec);
    throw;
  }
  archive_read_free(outer);

  if (!found_payload) {
    fs::remove(temp_zip, ec);
    throw ProtonCtlError(prtbak_file.string() +
                          " has no payload.zip entry - this looks like a backup made by an older, "
                          "incompatible version of ProtonCTL. Please create a new backup.");
  }

  // Step two: extract the recovered zip like any other archive.
  struct archive* inner = archive_read_new();
  archive_read_support_format_zip(inner);
  if (archive_read_open_filename(inner, temp_zip.c_str(), 1 << 20) != ARCHIVE_OK) {
    std::string err = archive_error_string(inner);
    archive_read_free(inner);
    fs::remove(temp_zip, ec);
    throw ProtonCtlError("Failed to open recovered payload: " + err);
  }

  try {
    ExtractArchiveEntries(inner, restore_root, on_progress);  // frees `inner`
  } catch (...) {
    fs::remove(temp_zip, ec);
    throw;
  }
  fs::remove(temp_zip, ec);
}

void BackupManager::CreateLibraryBackup(const fs::path& compat_tools_dir, const fs::path& out_file,
                                         int compression_level, const BackupProgressCallback& on_progress) {
  std::error_code ec;
  if (!fs::is_directory(compat_tools_dir, ec)) {
    throw ProtonCtlError("No such directory: " + compat_tools_dir.string());
  }

  CompatToolsBackupManifest manifest;
  manifest.created_at = NowIso8601();

  for (const auto& entry : fs::directory_iterator(compat_tools_dir, ec)) {
    if (!entry.is_directory()) continue;

    CompatToolsEntry tool;
    tool.name = entry.path().filename().string();

    std::ifstream vf(entry.path() / "version");
    if (vf.is_open()) std::getline(vf, tool.version_tag);

    tool.source_repo = ProtonRepositoryManager::ReadInstalledMetadata(entry.path()).source_repo;

    manifest.tools.push_back(tool);
  }

  if (manifest.tools.empty()) {
    throw ProtonCtlError("No installed Proton versions found under " + compat_tools_dir.string());
  }

  std::vector<std::pair<std::string, fs::path>> roots;
  for (const auto& tool : manifest.tools) roots.push_back({tool.name, compat_tools_dir / tool.name});

  fs::path temp_zip = fs::temp_directory_path() /
                       (out_file.filename().string() + ".payload-" + NowIso8601() + ".zip");

  try {
    WriteZipPayload(temp_zip, roots, compression_level, on_progress);
    VerifyArchiveIntegrity(temp_zip);
    WrapPayloadInTarGz(temp_zip, manifest.ToJson().dump(2), out_file, compression_level);
    VerifyArchiveIntegrity(out_file);
  } catch (...) {
    fs::remove(temp_zip, ec);
    fs::remove(out_file, ec);
    throw;
  }
  fs::remove(temp_zip, ec);
}

CompatToolsBackupManifest BackupManager::ReadLibraryManifest(const fs::path& prtbak_file) {
  struct archive* a = archive_read_new();
  archive_read_support_format_tar(a);
  archive_read_support_filter_all(a);

  if (archive_read_open_filename(a, prtbak_file.c_str(), 1 << 20) != ARCHIVE_OK) {
    std::string err = archive_error_string(a);
    archive_read_free(a);
    throw ProtonCtlError("Failed to open .prtbak: " + err);
  }

  struct archive_entry* entry;
  while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
    std::string path = archive_entry_pathname(entry);
    if (path == "manifest.json") {
      std::ostringstream oss;
      char buf[8192];
      la_ssize_t n;
      while ((n = archive_read_data(a, buf, sizeof(buf))) > 0) oss.write(buf, n);
      archive_read_free(a);
      nlohmann::json j = nlohmann::json::parse(oss.str());
      if (!j.contains("tools")) {
        throw ProtonCtlError(prtbak_file.string() +
                              " is a single-version backup - use the regular restore instead");
      }
      return CompatToolsBackupManifest::FromJson(j);
    }
    archive_read_data_skip(a);
  }

  archive_read_free(a);
  throw ProtonCtlError("Archive does not contain a manifest.json entry: " + prtbak_file.string());
}

void BackupManager::RestoreLibraryBackup(const fs::path& prtbak_file, const fs::path& compat_tools_dir,
                                          const BackupProgressCallback& on_progress) {
  ReadLibraryManifest(prtbak_file);  // throws if this isn't actually a library backup
  RestoreBackup(prtbak_file, compat_tools_dir, on_progress);
}

}  // namespace protonctl
