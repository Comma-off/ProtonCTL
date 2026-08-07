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
#include "protonctl/proton_repository_manager.hpp"

namespace protonctl {

enum class BuildStage {
  Queued,
  Cloning,
  Configuring,
  Building,
  Packaging,
  Done,
  Failed,
  Cancelled,
};

NLOHMANN_JSON_SERIALIZE_ENUM(BuildStage, {
  {BuildStage::Queued, "queued"},
  {BuildStage::Cloning, "cloning"},
  {BuildStage::Configuring, "configuring"},
  {BuildStage::Building, "building"},
  {BuildStage::Packaging, "packaging"},
  {BuildStage::Done, "done"},
  {BuildStage::Failed, "failed"},
  {BuildStage::Cancelled, "cancelled"},
})

struct BuildJob {
  std::string id;
  ProtonRepository repo;
  std::string ref;  // branch, tag, or commit; empty = default branch
  BuildStage stage = BuildStage::Queued;
  int progress_percent = 0;
  std::vector<std::string> log_lines;
  std::string error;
  fs::path result_path;  // populated once Packaging completes

  nlohmann::json ToJson() const;
};

// Runs a single build job at a time on a background thread: clones the
// repository into a scratch directory, autodetects (or uses the repo's
// override) a build pipeline - `./autogen.sh && ./configure && make`,
// a top-level `build.sh`, or a bare `Makefile` - then tars up whatever the
// build produced under a `dist/`/`build/` output directory and drops it
// straight into `compatibilitytools.d`.
class BuildRunner {
 public:
  BuildRunner();
  ~BuildRunner();

  // Enqueues a build and returns its job id immediately; the job runs on
  // an internal worker thread.
  std::string EnqueueBuild(const ProtonRepository& repo, const std::string& ref,
                            const fs::path& compat_tools_dir);

  std::optional<BuildJob> GetStatus(const std::string& job_id) const;
  std::vector<BuildJob> ListJobs() const;
  bool Cancel(const std::string& job_id);

 private:
  void WorkerLoop();
  void RunJob(BuildJob& job, const fs::path& compat_tools_dir);
  void AppendLog(BuildJob& job, const std::string& line);

  mutable std::mutex mutex_;
  std::unordered_map<std::string, BuildJob> jobs_;
  std::unordered_map<std::string, fs::path> job_compat_dirs_;
  std::vector<std::string> queue_;
  std::unordered_map<std::string, std::atomic<bool>> cancel_flags_;

  std::thread worker_;
  std::atomic<bool> shutting_down_{false};
  std::condition_variable_any cv_;
};

}  // namespace protonctl
