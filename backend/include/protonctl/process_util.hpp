#pragma once

#include <functional>
#include <string>
#include <vector>

#include "protonctl/common.hpp"

namespace protonctl {

// Line-by-line stdout+stderr callback used to stream build/clone logs to
// the caller (and, ultimately, to the Flutter UI).
using LineCallback = std::function<void(const std::string&)>;

struct ProcessResult {
  int exit_code = -1;
  std::string combined_output;
};

// Minimal synchronous subprocess runner built on POSIX pipe/fork/execvp.
// Streams combined stdout/stderr to `on_line` as it becomes available,
// which is what lets BuildRunner report live progress for long-running
// `make` invocations.
class ProcessUtil {
 public:
  static ProcessResult Run(const std::vector<std::string>& argv,
                            const fs::path& working_dir,
                            const LineCallback& on_line = nullptr);
};

}  // namespace protonctl
