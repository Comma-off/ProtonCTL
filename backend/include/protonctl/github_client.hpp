#pragma once

#include <cstdint>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "protonctl/common.hpp"

namespace protonctl {

struct GitHubAsset {
  std::string name;
  std::string browser_download_url;
  std::uint64_t size = 0;
  std::string content_type;

  nlohmann::json ToJson() const;
  static GitHubAsset FromJson(const nlohmann::json& j);
};

struct GitHubRelease {
  std::string tag_name;
  std::string name;
  std::string body;
  std::string published_at;
  bool prerelease = false;
  std::vector<GitHubAsset> assets;

  nlohmann::json ToJson() const;
  static GitHubRelease FromJson(const nlohmann::json& j);
};

// Progress callback: (bytes_downloaded, bytes_total).
using DownloadProgressCallback = std::function<void(std::uint64_t, std::uint64_t)>;

// Thin wrapper around the GitHub REST API (releases + repo metadata) built
// on libcurl. A personal access token may be supplied to raise the
// unauthenticated rate limit (60 req/hr -> 5000 req/hr).
class GitHubClient {
 public:
  explicit GitHubClient(std::string token = "");

  // GET /repos/{owner}/{repo}/releases
  std::vector<GitHubRelease> FetchReleases(const std::string& owner,
                                            const std::string& repo,
                                            int per_page = 30,
                                            int page = 1) const;

  // GET /repos/{owner}/{repo} - used to validate that `owner/repo` exists
  // and is reachable before it's added as a custom repository.
  nlohmann::json FetchRepoMetadata(const std::string& owner,
                                    const std::string& repo) const;

  // Streams `url` to `destination`, invoking `on_progress` periodically.
  void DownloadAsset(const std::string& url, const fs::path& destination,
                      const DownloadProgressCallback& on_progress = nullptr) const;

 private:
  std::string HttpGet(const std::string& url) const;
  std::string token_;
};

}  // namespace protonctl
