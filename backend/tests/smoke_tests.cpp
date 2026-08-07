// Minimal, dependency-free smoke tests (no gtest) so `PROTONCTL_BUILD_TESTS`
// can be enabled without vendoring a test framework. Each check aborts the
// process with a non-zero exit code on failure, which is all `ctest` needs.
#include <cassert>
#include <fstream>
#include <iostream>

#include "protonctl/backup_manager.hpp"
#include "protonctl/config.hpp"
#include "protonctl/proton_repository_manager.hpp"
#include "protonctl/steam_path_resolver.hpp"

using namespace protonctl;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
  if (!condition) {
    std::cerr << "FAILED: " << what << "\n";
    ++g_failures;
  } else {
    std::cout << "ok: " << what << "\n";
  }
}

void TestAppConfigRoundTrip() {
  AppConfig cfg;
  cfg.steam_path = "/tmp/steam";
  cfg.install_type = SteamInstallType::Flatpak;
  cfg.theme_variant = "fidelity";
  cfg.seed_color_index = 3;

  AppConfig roundtripped = AppConfig::FromJson(cfg.ToJson());
  Check(roundtripped.steam_path == cfg.steam_path, "AppConfig steam_path survives JSON round trip");
  Check(roundtripped.install_type == cfg.install_type, "AppConfig install_type survives JSON round trip");
  Check(roundtripped.theme_variant == cfg.theme_variant, "AppConfig theme_variant survives JSON round trip");
  Check(roundtripped.seed_color_index == cfg.seed_color_index,
        "AppConfig seed_color_index survives JSON round trip");
}

void TestProtonRepositoryRoundTrip() {
  ProtonRepository repo;
  repo.owner = "GloriousEggroll";
  repo.repo = "proton-ge-custom";
  repo.display_name = "GE-Proton";

  ProtonRepository roundtripped = ProtonRepository::FromJson(repo.ToJson());
  Check(roundtripped.FullName() == "GloriousEggroll/proton-ge-custom",
        "ProtonRepository::FullName survives JSON round trip");
}

void TestSteamPathValidation() {
  SteamCandidate invalid = SteamPathResolver::ValidateCustomPath("/nonexistent/path/for/sure");
  Check(!invalid.valid, "Nonexistent custom Steam path is reported invalid");
}

void TestPrtBakManifestRoundTrip() {
  PrtBakManifest manifest;
  manifest.proton_version = "GE-Proton9-20";
  manifest.dependencies = {"dxvk-2.3", "vkd3d-proton-2.12"};

  PrtBakManifest roundtripped = PrtBakManifest::FromJson(manifest.ToJson());
  Check(roundtripped.proton_version == manifest.proton_version,
        "PrtBakManifest proton_version survives JSON round trip");
  Check(roundtripped.dependencies.size() == 2, "PrtBakManifest dependencies survive JSON round trip");
}

void WriteFile(const fs::path& path, const std::string& content) {
  fs::create_directories(path.parent_path());
  std::ofstream(path) << content;
}

std::string ReadFile(const fs::path& path) {
  std::ifstream in(path);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// Exercises the whole-library backup end to end: fabricates a fake
// compatibilitytools.d with two "installed" Proton builds, backs the
// whole thing up, restores it into a fresh directory, and checks the
// files and manifest actually round-trip - not just that the types do.
void TestLibraryBackupRoundTrip() {
  fs::path root = fs::temp_directory_path() / "protonctl_test_library_backup";
  fs::path compat_dir = root / "compatibilitytools.d";
  fs::path restore_dir = root / "restored";
  fs::path archive = root / "library.prtbak";
  std::error_code ec;
  fs::remove_all(root, ec);

  WriteFile(compat_dir / "GE-Proton9-20" / "version", "9-20 GE-Proton9-20");
  WriteFile(compat_dir / "GE-Proton9-20" / "files" / "bin" / "wine", "fake wine binary");
  ProtonRepositoryManager::MarkInstalled(compat_dir / "GE-Proton9-20", "GloriousEggroll/proton-ge-custom");

  // Real Proton/Wine trees are full of versioned .so symlinks, and their
  // targets routinely exceed ustar's hard 100-byte linkname field (this is
  // exactly what "Failed to write archive entry: Link contents too long"
  // was - CreateLibraryBackup must use a format without that limit).
  fs::create_directories(compat_dir / "GE-Proton9-20" / "files" / "lib", ec);
  std::string long_symlink_target(150, 'x');
  fs::create_symlink(long_symlink_target, compat_dir / "GE-Proton9-20" / "files" / "lib" / "libfoo.so", ec);

  WriteFile(compat_dir / "my-custom-build" / "version", "1 my-custom-build");
  WriteFile(compat_dir / "my-custom-build" / "files" / "bin" / "wine", "another fake wine binary");
  ProtonRepositoryManager::MarkInstalled(compat_dir / "my-custom-build", "someuser/proton-fork");

  BackupManager::CreateLibraryBackup(compat_dir, archive);
  Check(fs::exists(archive), "CreateLibraryBackup produces an archive file");

  CompatToolsBackupManifest manifest = BackupManager::ReadLibraryManifest(archive);
  Check(manifest.tools.size() == 2, "Library manifest lists both installed builds");
  bool found_ge = false, found_custom = false;
  for (const auto& tool : manifest.tools) {
    if (tool.name == "GE-Proton9-20") {
      found_ge = tool.source_repo == "GloriousEggroll/proton-ge-custom";
    }
    if (tool.name == "my-custom-build") {
      found_custom = tool.source_repo == "someuser/proton-fork";
    }
  }
  Check(found_ge, "Library manifest records GE-Proton9-20's source repo");
  Check(found_custom, "Library manifest records my-custom-build's source repo");

  try {
    BackupManager::ReadManifest(archive);
    Check(false, "Single-backup ReadManifest rejects a whole-library archive");
  } catch (const ProtonCtlError&) {
    Check(true, "Single-backup ReadManifest rejects a whole-library archive");
  }

  // Simulate stale/conflicting content already sitting at the destination -
  // e.g. left behind by a different-shaped previous install/restore of the
  // same tool - to prove restore actually replaces it instead of relying on
  // libarchive to resolve the type mismatch itself ("Can't replace existing
  // directory with non-directory"). Non-empty, since libarchive will happily
  // rmdir() an *empty* conflicting directory on its own - only a non-empty
  // one hits the failure this is guarding against.
  fs::create_directories(restore_dir / "GE-Proton9-20" / "files" / "bin" / "wine" / "unexpected_child", ec);

  // Real Proton/GE-Proton release tarballs sometimes preserve read-only
  // (no write bit) directories from upstream. If removing the conflicting
  // tree above can't actually unlink through a read-only intermediate
  // directory, cleanup silently leaves the conflict in place - and the
  // restore fails with the exact same "Can't replace existing directory
  // with non-directory" error a second time.
  fs::permissions(restore_dir / "GE-Proton9-20" / "files" / "bin", fs::perms::owner_read | fs::perms::owner_exec,
                   fs::perm_options::replace, ec);

  BackupManager::RestoreLibraryBackup(archive, restore_dir);
  Check(fs::is_directory(restore_dir / "GE-Proton9-20"), "Restored library contains GE-Proton9-20");
  Check(fs::is_directory(restore_dir / "my-custom-build"), "Restored library contains my-custom-build");
  Check(ReadFile(restore_dir / "GE-Proton9-20" / "files" / "bin" / "wine") == "fake wine binary",
        "Restored file contents match what was backed up");

  fs::path restored_symlink = restore_dir / "GE-Proton9-20" / "files" / "lib" / "libfoo.so";
  Check(fs::is_symlink(restored_symlink, ec), "Restored library preserves a >100-byte symlink target");
  Check(fs::read_symlink(restored_symlink, ec).string() == long_symlink_target,
        "Restored symlink target matches the original >100-byte target");

  // Import the same library backup a second time, into a destination that
  // now already has the full, correctly-restored content from the first
  // pass - the realistic "re-import" / "restore into a compatibilitytools.d
  // that already has these builds" case, not just a synthetic conflict.
  BackupManager::RestoreLibraryBackup(archive, restore_dir);
  Check(fs::is_directory(restore_dir / "GE-Proton9-20"), "Re-importing over already-restored content succeeds");
  Check(ReadFile(restore_dir / "GE-Proton9-20" / "files" / "bin" / "wine") == "fake wine binary",
        "Re-imported file contents still match after a second import");

  fs::remove_all(root, ec);
}

}  // namespace

int main() {
  TestAppConfigRoundTrip();
  TestProtonRepositoryRoundTrip();
  TestSteamPathValidation();
  TestPrtBakManifestRoundTrip();
  TestLibraryBackupRoundTrip();

  if (g_failures > 0) {
    std::cerr << g_failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "All smoke tests passed\n";
  return 0;
}
