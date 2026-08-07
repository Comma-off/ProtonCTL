// Thin development CLI for exercising protonctl_core without the Flutter
// shell. Not part of the shipped product; useful for CI smoke tests and
// local debugging of the native engine.
#include <iostream>

#include "protonctl/backup_manager.hpp"
#include "protonctl/build_runner.hpp"
#include "protonctl/config.hpp"
#include "protonctl/proton_repository_manager.hpp"
#include "protonctl/repair_manager.hpp"
#include "protonctl/steam_path_resolver.hpp"

using namespace protonctl;

namespace {

void PrintUsage(const char* argv0) {
  std::cerr << "Usage: " << argv0 << " <command> [args]\n"
            << "Commands:\n"
            << "  detect-steam                      Probe standard Steam install locations\n"
            << "  list-repos                        List registered Proton repositories\n"
            << "  add-repo <owner> <repo>           Register a custom GitHub repository\n"
            << "  releases <owner> <repo>            List releases for a repository\n"
            << "  list-installed <compat_tools_dir>  List installed Proton versions\n"
            << "  scan-repair <compat_dir> <data_dir> Scan for broken installs/prefixes\n";
}

int CmdDetectSteam() {
  for (const auto& candidate : SteamPathResolver::DetectCandidates()) {
    std::cout << (candidate.valid ? "[ok]   " : "[miss] ") << candidate.label << ": "
              << candidate.path.string() << "\n";
  }
  return 0;
}

int CmdListRepos() {
  ProtonRepositoryManager mgr;
  for (const auto& repo : mgr.ListRepositories()) {
    std::cout << repo.FullName() << (repo.is_builtin ? " (builtin)" : "") << "\n";
  }
  return 0;
}

int CmdAddRepo(const std::string& owner, const std::string& repo) {
  ProtonRepositoryManager mgr;
  auto added = mgr.AddCustomRepository(owner, repo);
  std::cout << "Added " << added.FullName() << "\n";
  return 0;
}

int CmdReleases(const std::string& owner, const std::string& repo) {
  ProtonRepositoryManager mgr;
  ProtonRepository r;
  r.owner = owner;
  r.repo = repo;
  for (const auto& rel : mgr.FetchAvailableReleases(r)) {
    std::cout << rel.tag_name << "  (" << rel.assets.size() << " assets)\n";
  }
  return 0;
}

int CmdListInstalled(const std::string& compat_dir) {
  ProtonRepositoryManager mgr;
  for (const auto& v : mgr.ListInstalledVersions(compat_dir)) {
    std::cout << v.name << "  " << v.path.string() << "\n";
  }
  return 0;
}

int CmdScanRepair(const std::string& compat_dir, const std::string& data_dir) {
  for (const auto& issue : RepairManager::Scan(compat_dir, data_dir)) {
    std::cout << (issue.fixable ? "[fixable]  " : "[manual]   ") << issue.path.string() << " - "
              << issue.description << "\n";
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    PrintUsage(argv[0]);
    return 1;
  }

  std::string command = argv[1];
  try {
    if (command == "detect-steam") return CmdDetectSteam();
    if (command == "list-repos") return CmdListRepos();
    if (command == "add-repo" && argc == 4) return CmdAddRepo(argv[2], argv[3]);
    if (command == "releases" && argc == 4) return CmdReleases(argv[2], argv[3]);
    if (command == "list-installed" && argc == 3) return CmdListInstalled(argv[2]);
    if (command == "scan-repair" && argc == 4) return CmdScanRepair(argv[2], argv[3]);

    PrintUsage(argv[0]);
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
