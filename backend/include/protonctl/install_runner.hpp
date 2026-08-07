#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "protonctl/common.hpp"
#include "protonctl/github_client.hpp"
#include "protonctl/proton_repository_manager.hpp"

namespace protonctl {

enum class InstallStage {
  Queued,
  Downloading,
  Extracting,
  Done,
  Failed,
  Cancelled,
};

NLOHMANN_JSON_SERIALIZE_ENUM(InstallStage, {
  {InstallStage::Queued, "queued"},
  {InstallStage::Downloading, "downloading"},
  {InstallStage::Extracting, "extracting"},
  {InstallStage::Done, "done"},
  {InstallStage::Failed, "failed"},
  {InstallStage::Cancelled, "cancelled"},
})

struct InstallJob {
  std::string id;
  ProtonRepository repo;
  GitHubRelease release;
  InstallStage stage = InstallStage::Queued;
  int progress_percent = 0;
  std::uint64_t bytes_downloaded = 0;
  std::uint64_t bytes_total = 0;
  std::string error;
  InstalledProtonVersion result;  // populated once Done

  nlohmann::json ToJson() const;
};

// Runs Proton release installs (download + extract into
// compatibilitytools.d) on a background thread so the caller gets an
// immediate job id back instead of blocking for however long the download
// takes, and can poll real byte-level progress instead of staring at an
// indeterminate spinner. Mirrors `BuildRunner`'s queue/poll shape.
class InstallRunner {
 public:
  explicit InstallRunner(std::string github_token = "");
  ~InstallRunner();

  std::string EnqueueInstall(const ProtonRepository& repo, const GitHubRelease& release,
                              const fs::path& compat_tools_dir);

  std::optional<InstallJob> GetStatus(const std::string& job_id) const;
  std::vector<InstallJob> ListJobs() const;
  bool Cancel(const std::string& job_id);

 private:
  void WorkerLoop();
  void RunJob(InstallJob& job, const fs::path& compat_tools_dir);

  mutable std::mutex mutex_;
  std::unordered_map<std::string, InstallJob> jobs_;
  std::unordered_map<std::string, fs::path> job_compat_dirs_;
  std::vector<std::string> queue_;
  std::unordered_map<std::string, std::atomic<bool>> cancel_flags_;

  ProtonRepositoryManager repo_manager_;

  std::thread worker_;
  std::atomic<bool> shutting_down_{false};
  std::condition_variable_any cv_;
};

}  // namespace protonctl
