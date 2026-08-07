#pragma once

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

namespace protonctl {

namespace fs = std::filesystem;

// Thrown by any core operation that fails in a way the caller must surface
// to the UI (missing paths, network failures, archive corruption, etc).
class ProtonCtlError : public std::runtime_error {
public:
  explicit ProtonCtlError(const std::string& what) : std::runtime_error(what) {}
};

inline fs::path ExpandUserHome(const std::string& raw) {
  if (raw.empty() || raw[0] != '~') return fs::path(raw);
  const char* home = std::getenv("HOME");
  if (!home) throw ProtonCtlError("HOME environment variable is not set");
  return fs::path(home) / raw.substr(raw[1] == '/' ? 2 : 1);
}

inline std::string NowIso8601() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

}  // namespace protonctl
