#include "protonctl/github_client.hpp"

#include <curl/curl.h>

#include <fstream>

namespace protonctl {

namespace {

constexpr const char* kApiBase = "https://api.github.com";
constexpr const char* kUserAgent = "PROTONCTL/0.1 (+https://github.com)";

size_t WriteToString(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* out = static_cast<std::string*>(userdata);
  out->append(ptr, size * nmemb);
  return size * nmemb;
}

struct DownloadContext {
  std::ofstream file;
  const DownloadProgressCallback* on_progress = nullptr;
};

size_t WriteToFile(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* ctx = static_cast<DownloadContext*>(userdata);
  ctx->file.write(ptr, static_cast<std::streamsize>(size * nmemb));
  return size * nmemb;
}

int ProgressTrampoline(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
  auto* ctx = static_cast<DownloadContext*>(clientp);
  if (ctx->on_progress && *ctx->on_progress) {
    (*ctx->on_progress)(static_cast<std::uint64_t>(dlnow), static_cast<std::uint64_t>(dltotal));
  }
  return 0;
}

class CurlHandle {
 public:
  CurlHandle() : handle_(curl_easy_init()) {
    if (!handle_) throw ProtonCtlError("curl_easy_init failed");
  }
  ~CurlHandle() { curl_easy_cleanup(handle_); }
  CURL* get() const { return handle_; }

 private:
  CURL* handle_;
};

}  // namespace

nlohmann::json GitHubAsset::ToJson() const {
  return {{"name", name},
          {"browser_download_url", browser_download_url},
          {"size", size},
          {"content_type", content_type}};
}

GitHubAsset GitHubAsset::FromJson(const nlohmann::json& j) {
  GitHubAsset a;
  a.name = j.value("name", "");
  a.browser_download_url = j.value("browser_download_url", "");
  a.size = j.value("size", 0ULL);
  a.content_type = j.value("content_type", "");
  return a;
}

nlohmann::json GitHubRelease::ToJson() const {
  nlohmann::json assets_json = nlohmann::json::array();
  for (const auto& a : assets) assets_json.push_back(a.ToJson());
  return {{"tag_name", tag_name}, {"name", name},        {"body", body},
          {"published_at", published_at}, {"prerelease", prerelease}, {"assets", assets_json}};
}

GitHubRelease GitHubRelease::FromJson(const nlohmann::json& j) {
  GitHubRelease r;
  r.tag_name = j.value("tag_name", "");
  r.name = j.value("name", "");
  r.body = j.value("body", "");
  r.published_at = j.value("published_at", "");
  r.prerelease = j.value("prerelease", false);
  if (j.contains("assets")) {
    for (const auto& a : j.at("assets")) r.assets.push_back(GitHubAsset::FromJson(a));
  }
  return r;
}

GitHubClient::GitHubClient(std::string token) : token_(std::move(token)) {}

std::string GitHubClient::HttpGet(const std::string& url) const {
  CurlHandle curl;
  std::string response;

  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
  headers = curl_slist_append(headers, "X-GitHub-Api-Version: 2022-11-28");
  std::string auth_header;
  if (!token_.empty()) {
    auth_header = "Authorization: Bearer " + token_;
    headers = curl_slist_append(headers, auth_header.c_str());
  }

  curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, kUserAgent);
  curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteToString);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 30L);

  CURLcode res = curl_easy_perform(curl.get());
  curl_slist_free_all(headers);

  if (res != CURLE_OK) {
    throw ProtonCtlError(std::string("GitHub request failed: ") + curl_easy_strerror(res));
  }

  long status = 0;
  curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
  if (status == 403 || status == 429) {
    throw ProtonCtlError("GitHub API rate limit exceeded; add a personal access token to continue");
  }
  if (status == 404) {
    throw ProtonCtlError("Repository or resource not found on GitHub");
  }
  if (status >= 400) {
    throw ProtonCtlError("GitHub API returned HTTP " + std::to_string(status));
  }

  return response;
}

std::vector<GitHubRelease> GitHubClient::FetchReleases(const std::string& owner,
                                                        const std::string& repo, int per_page,
                                                        int page) const {
  std::string url = std::string(kApiBase) + "/repos/" + owner + "/" + repo +
                     "/releases?per_page=" + std::to_string(per_page) +
                     "&page=" + std::to_string(page);
  nlohmann::json j = nlohmann::json::parse(HttpGet(url));

  std::vector<GitHubRelease> releases;
  releases.reserve(j.size());
  for (const auto& item : j) releases.push_back(GitHubRelease::FromJson(item));
  return releases;
}

nlohmann::json GitHubClient::FetchRepoMetadata(const std::string& owner,
                                                const std::string& repo) const {
  std::string url = std::string(kApiBase) + "/repos/" + owner + "/" + repo;
  return nlohmann::json::parse(HttpGet(url));
}

void GitHubClient::DownloadAsset(const std::string& url, const fs::path& destination,
                                  const DownloadProgressCallback& on_progress) const {
  CurlHandle curl;
  DownloadContext ctx;
  ctx.file.open(destination, std::ios::binary | std::ios::trunc);
  if (!ctx.file.is_open()) {
    throw ProtonCtlError("Unable to open destination file: " + destination.string());
  }
  ctx.on_progress = &on_progress;

  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Accept: application/octet-stream");
  std::string auth_header;
  if (!token_.empty()) {
    auth_header = "Authorization: Bearer " + token_;
    headers = curl_slist_append(headers, auth_header.c_str());
  }

  curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, kUserAgent);
  curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteToFile);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &ctx);
  curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, ProgressTrampoline);
  curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, &ctx);
  curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 0L);
  curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 30L);
  // No overall time limit (release tarballs can legitimately take minutes on
  // a slow link), but abort if the transfer stalls below 1KB/s for 30s
  // straight - without this a dead connection hangs forever with zero
  // feedback, which is exactly what looked like an "infinite" install.
  curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_LIMIT, 1024L);
  curl_easy_setopt(curl.get(), CURLOPT_LOW_SPEED_TIME, 30L);

  CURLcode res = curl_easy_perform(curl.get());
  curl_slist_free_all(headers);
  ctx.file.close();

  if (res != CURLE_OK) {
    std::error_code ec;
    fs::remove(destination, ec);
    throw ProtonCtlError(std::string("Download failed: ") + curl_easy_strerror(res));
  }
}

}  // namespace protonctl
