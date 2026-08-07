#include "protonctl/install_runner.hpp"

#include <chrono>
#include <random>
#include <sstream>

namespace protonctl {

namespace {

std::string GenerateJobId() {
  static std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());
  std::uniform_int_distribution<uint64_t> dist;
  std::ostringstream oss;
  oss << "install-" << std::hex << dist(rng);
  return oss.str();
}

}  // namespace

nlohmann::json InstallJob::ToJson() const {
  return {
      {"id", id},
      {"repo", repo.ToJson()},
      {"release", release.ToJson()},
      {"stage", stage},
      {"progress_percent", progress_percent},
      {"bytes_downloaded", bytes_downloaded},
      {"bytes_total", bytes_total},
      {"error", error},
      {"result", result.ToJson()},
  };
}

InstallRunner::InstallRunner(std::string github_token) : repo_manager_(std::move(github_token)) {
  worker_ = std::thread([this] { WorkerLoop(); });
}

InstallRunner::~InstallRunner() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    shutting_down_ = true;
  }
  cv_.notify_all();
  if (worker_.joinable()) worker_.join();
}

std::string InstallRunner::EnqueueInstall(const ProtonRepository& repo, const GitHubRelease& release,
                                           const fs::path& compat_tools_dir) {
  std::string id = GenerateJobId();

  InstallJob job;
  job.id = id;
  job.repo = repo;
  job.release = release;
  job.stage = InstallStage::Queued;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    jobs_[id] = job;
    job_compat_dirs_[id] = compat_tools_dir;
    cancel_flags_[id].store(false);
    queue_.push_back(id);
  }
  cv_.notify_one();
  return id;
}

std::optional<InstallJob> InstallRunner::GetStatus(const std::string& job_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = jobs_.find(job_id);
  if (it == jobs_.end()) return std::nullopt;
  return it->second;
}

std::vector<InstallJob> InstallRunner::ListJobs() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<InstallJob> result;
  result.reserve(jobs_.size());
  for (const auto& [id, job] : jobs_) result.push_back(job);
  return result;
}

bool InstallRunner::Cancel(const std::string& job_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = cancel_flags_.find(job_id);
  if (it == cancel_flags_.end()) return false;
  it->second.store(true);
  return true;
}

void InstallRunner::WorkerLoop() {
  while (true) {
    std::string job_id;
    fs::path compat_dir;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return shutting_down_ || !queue_.empty(); });
      if (shutting_down_ && queue_.empty()) return;
      job_id = queue_.front();
      queue_.erase(queue_.begin());
      compat_dir = job_compat_dirs_[job_id];
    }

    InstallJob job;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      job = jobs_[job_id];
    }

    RunJob(job, compat_dir);

    {
      std::lock_guard<std::mutex> lock(mutex_);
      jobs_[job_id] = job;
    }
  }
}

void InstallRunner::RunJob(InstallJob& job, const fs::path& compat_tools_dir) {
  auto is_cancelled = [&] {
    std::lock_guard<std::mutex> lock(mutex_);
    return cancel_flags_[job.id].load();
  };

  if (is_cancelled()) {
    job.stage = InstallStage::Cancelled;
    return;
  }

  job.stage = InstallStage::Downloading;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    jobs_[job.id] = job;
  }

  // Mutates the local `job` (the source of truth RunJob/WorkerLoop share)
  // and syncs it into the map under lock, the same pattern BuildRunner's
  // AppendLog uses - keeps the two from diverging the way they would if
  // this only wrote through to the map directly.
  auto on_progress = [&](std::uint64_t downloaded, std::uint64_t total) {
    job.bytes_downloaded = downloaded;
    job.bytes_total = total;
    if (total > 0) {
      // Downloading is the first ~90% of the job; extraction (which isn't
      // itself instrumented) accounts for the rest. curl calls this once
      // more at downloaded == total right as the transfer finishes, which
      // is as accurate a signal as we have for "extraction is starting".
      job.progress_percent =
          static_cast<int>(90.0 * static_cast<double>(downloaded) / static_cast<double>(total));
      if (downloaded >= total) {
        job.stage = InstallStage::Extracting;
        job.progress_percent = 90;
      }
    }
    std::lock_guard<std::mutex> lock(mutex_);
    jobs_[job.id] = job;
  };

  try {
    InstalledProtonVersion installed =
        repo_manager_.InstallRelease(job.repo, job.release, compat_tools_dir, on_progress);
    job.result = installed;
    job.stage = InstallStage::Done;
    job.progress_percent = 100;
  } catch (const std::exception& e) {
    job.stage = is_cancelled() ? InstallStage::Cancelled : InstallStage::Failed;
    job.error = e.what();
  }
}

}  // namespace protonctl
