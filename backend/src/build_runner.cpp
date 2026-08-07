#include "protonctl/build_runner.hpp"

#include <chrono>
#include <random>
#include <sstream>

#include "protonctl/process_util.hpp"

namespace protonctl {

namespace {

std::string GenerateJobId() {
  static std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());
  std::uniform_int_distribution<uint64_t> dist;
  std::ostringstream oss;
  oss << "job-" << std::hex << dist(rng);
  return oss.str();
}

bool FileExists(const fs::path& p) {
  std::error_code ec;
  return fs::exists(p, ec);
}

}  // namespace

nlohmann::json BuildJob::ToJson() const {
  return {
      {"id", id},
      {"repo", repo.ToJson()},
      {"ref", ref},
      {"stage", stage},
      {"progress_percent", progress_percent},
      {"log_lines", log_lines},
      {"error", error},
      {"result_path", result_path.string()},
  };
}

BuildRunner::BuildRunner() {
  worker_ = std::thread([this] { WorkerLoop(); });
}

BuildRunner::~BuildRunner() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    shutting_down_ = true;
  }
  cv_.notify_all();
  if (worker_.joinable()) worker_.join();
}

std::string BuildRunner::EnqueueBuild(const ProtonRepository& repo, const std::string& ref,
                                       const fs::path& compat_tools_dir) {
  std::string id = GenerateJobId();

  BuildJob job;
  job.id = id;
  job.repo = repo;
  job.ref = ref;
  job.stage = BuildStage::Queued;

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

std::optional<BuildJob> BuildRunner::GetStatus(const std::string& job_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = jobs_.find(job_id);
  if (it == jobs_.end()) return std::nullopt;
  return it->second;
}

std::vector<BuildJob> BuildRunner::ListJobs() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<BuildJob> result;
  result.reserve(jobs_.size());
  for (const auto& [id, job] : jobs_) result.push_back(job);
  return result;
}

bool BuildRunner::Cancel(const std::string& job_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = cancel_flags_.find(job_id);
  if (it == cancel_flags_.end()) return false;
  it->second.store(true);
  return true;
}

void BuildRunner::AppendLog(BuildJob& job, const std::string& line) {
  job.log_lines.push_back(line);
  if (job.log_lines.size() > 2000) job.log_lines.erase(job.log_lines.begin());

  std::lock_guard<std::mutex> lock(mutex_);
  jobs_[job.id] = job;
}

void BuildRunner::WorkerLoop() {
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

    BuildJob job;
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

void BuildRunner::RunJob(BuildJob& job, const fs::path& compat_tools_dir) {
  auto is_cancelled = [&] {
    std::lock_guard<std::mutex> lock(mutex_);
    return cancel_flags_[job.id].load();
  };

  fs::path scratch = fs::temp_directory_path() / ("protonctl-build-" + job.id);
  std::error_code ec;
  fs::create_directories(scratch, ec);
  fs::path src_dir = scratch / "src";

  auto on_line = [&](const std::string& line) { AppendLog(job, line); };

  try {
    // --- Clone ---------------------------------------------------------
    job.stage = BuildStage::Cloning;
    job.progress_percent = 5;
    AppendLog(job, "Cloning " + job.repo.FullName() + (job.ref.empty() ? "" : "@" + job.ref));

    std::string clone_url = "https://github.com/" + job.repo.FullName() + ".git";
    std::vector<std::string> clone_argv = {"git", "clone", "--recurse-submodules", "--depth", "1"};
    if (!job.ref.empty()) {
      clone_argv.push_back("--branch");
      clone_argv.push_back(job.ref);
    }
    clone_argv.push_back(clone_url);
    clone_argv.push_back(src_dir.string());

    ProcessResult clone_result = ProcessUtil::Run(clone_argv, scratch, on_line);
    if (clone_result.exit_code != 0) {
      throw ProtonCtlError("git clone failed with exit code " + std::to_string(clone_result.exit_code));
    }
    if (is_cancelled()) throw ProtonCtlError("Build cancelled during clone");

    // --- Configure / Build ----------------------------------------------
    job.stage = BuildStage::Configuring;
    job.progress_percent = 25;

    std::vector<std::vector<std::string>> build_steps;
    if (!job.repo.build_command_override.empty()) {
      build_steps.push_back({"/bin/sh", "-c", job.repo.build_command_override});
    } else if (FileExists(src_dir / "build.sh")) {
      build_steps.push_back({"/bin/sh", "build.sh"});
    } else if (FileExists(src_dir / "autogen.sh")) {
      build_steps.push_back({"/bin/sh", "autogen.sh"});
      build_steps.push_back({"/bin/sh", "configure"});
      build_steps.push_back({"make", "-j" + std::to_string(std::thread::hardware_concurrency())});
    } else if (FileExists(src_dir / "configure")) {
      build_steps.push_back({"/bin/sh", "configure"});
      build_steps.push_back({"make", "-j" + std::to_string(std::thread::hardware_concurrency())});
    } else if (FileExists(src_dir / "Makefile")) {
      build_steps.push_back({"make", "-j" + std::to_string(std::thread::hardware_concurrency())});
    } else {
      throw ProtonCtlError(
          "Unable to autodetect a build pipeline (no build.sh/autogen.sh/configure/Makefile found)");
    }

    job.stage = BuildStage::Building;
    int step_index = 0;
    for (auto& step : build_steps) {
      if (is_cancelled()) throw ProtonCtlError("Build cancelled");
      AppendLog(job, "$ " + [&] {
        std::string joined;
        for (auto& s : step) joined += s + " ";
        return joined;
      }());
      ProcessResult r = ProcessUtil::Run(step, src_dir, on_line);
      if (r.exit_code != 0) {
        throw ProtonCtlError("Build step failed with exit code " + std::to_string(r.exit_code));
      }
      job.progress_percent = 25 + static_cast<int>(50.0 * (++step_index) / build_steps.size());
    }

    // --- Package ---------------------------------------------------------
    job.stage = BuildStage::Packaging;
    job.progress_percent = 90;
    AppendLog(job, "Packaging build output into compatibilitytools.d");

    fs::path output_source = src_dir;
    for (const char* candidate : {"dist", "build", "out"}) {
      if (fs::is_directory(src_dir / candidate, ec)) {
        output_source = src_dir / candidate;
        break;
      }
    }

    std::string target_name = job.repo.repo + (job.ref.empty() ? "" : "-" + job.ref);
    fs::path target_dir = compat_tools_dir / target_name;
    fs::create_directories(compat_tools_dir, ec);
    fs::remove_all(target_dir, ec);

    fs::copy(output_source, target_dir, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
    if (ec) throw ProtonCtlError("Failed to package build output: " + ec.message());
    ProtonRepositoryManager::MarkInstalled(target_dir, job.repo.FullName());

    job.result_path = target_dir;
    job.stage = BuildStage::Done;
    job.progress_percent = 100;
    AppendLog(job, "Build complete: " + target_dir.string());
  } catch (const std::exception& e) {
    job.stage = is_cancelled() ? BuildStage::Cancelled : BuildStage::Failed;
    job.error = e.what();
    AppendLog(job, std::string("ERROR: ") + e.what());
  }

  std::error_code cleanup_ec;
  fs::remove_all(scratch, cleanup_ec);
}

}  // namespace protonctl
