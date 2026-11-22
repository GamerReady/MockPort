// Copyright 2026 MockPort Project Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// PS4 build of the curl-based update downloader. Implements the same
// interface with the PS4 socket/TLS transport: streaming downloads with
// resume support, progress reporting, retries, and SHA-256 digests.

#include "update/http_download.h"

#include <mbedtls/sha256.h>

#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ps4/ps4_http_transport.h"

namespace mocktail::update {
namespace {

constexpr std::int64_t kMebibyte = 1024 * 1024;

std::string Lower(std::string value) {
  for (char& character : value) {
    if (character >= 'A' && character <= 'Z') {
      character = static_cast<char>(character - 'A' + 'a');
    }
  }
  return value;
}

bool HostMatches(std::string_view host, std::string_view allowed) {
  if (host == allowed) return true;
  return host.size() > allowed.size() &&
         host[host.size() - allowed.size() - 1] == '.' &&
         host.substr(host.size() - allowed.size()) == allowed;
}

void PublishMessage(int descriptor, const std::string& message) {
  if (descriptor < 0) return;
  const std::string packet = "P" + message;
  (void)::send(descriptor, packet.data(), packet.size(), MSG_NOSIGNAL);
}

bool FileSize(const std::filesystem::path& path, std::int64_t* size) {
  std::error_code error;
  const auto status = std::filesystem::status(path, error);
  if (error || !std::filesystem::is_regular_file(status)) {
    return false;
  }
  *size = static_cast<std::int64_t>(std::filesystem::file_size(path, error));
  return !error;
}

std::string FileSha256(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return {};
  }
  mbedtls_sha256_context context;
  mbedtls_sha256_starts(&context, 0);
  std::vector<char> buffer(64 * 1024);
  while (input) {
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize read = input.gcount();
    if (read > 0) {
      mbedtls_sha256_update(&context,
                            reinterpret_cast<const unsigned char*>(buffer.data()),
                            static_cast<size_t>(read));
    }
  }
  unsigned char digest[32] = {};
  mbedtls_sha256_finish(&context, digest);
  static const char kHex[] = "0123456789abcdef";
  std::string output(64, '0');
  for (std::size_t index = 0; index < 32; ++index) {
    output[index * 2] = kHex[digest[index] >> 4];
    output[index * 2 + 1] = kHex[digest[index] & 0x0f];
  }
  return output;
}

struct FileBodySink final : public ps4::HttpBodySink {
  std::ofstream* output = nullptr;
  std::int64_t written = 0;
  std::int64_t expected = 0;
  int progress_fd = -1;
  std::chrono::steady_clock::time_point last_progress{};
  std::int64_t last_reported = 0;
  bool started = false;

  bool OnBodyChunk(const char* data, std::size_t size) override {
    if (output == nullptr || !output->write(data, static_cast<std::streamsize>(size))) {
      return false;
    }
    written += static_cast<std::int64_t>(size);
    const auto moment = std::chrono::steady_clock::now();
    if (progress_fd >= 0) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          moment - last_progress);
      if (!started || elapsed >= std::chrono::seconds(1)) {
        std::string message = "Downloading Roblox... " +
                              std::to_string(written / kMebibyte) + " MiB";
        if (expected > 0) {
          message += " / " + std::to_string(expected / kMebibyte) + " MiB (" +
                     std::to_string(written * 100 / expected) + "%)";
        }
        PublishMessage(progress_fd, message);
        last_progress = moment;
        started = true;
        last_reported = written;
      }
    }
    (void)last_reported;
    return true;
  }
};

// One download attempt with resume support. `temporary` must already exist
// with any previously transferred bytes when resuming.
HttpDownloadResult DownloadFileAttempt(const HttpTransferRequest& request,
                                       const std::filesystem::path& temporary,
                                       int progress_fd, bool* retryable) {
  HttpDownloadResult result;
  *retryable = false;

  std::int64_t existing = 0;
  bool has_existing = FileSize(temporary, &existing);
  if (has_existing && existing == 0) {
    has_existing = false;
  }

  ps4::HttpTransportRequest transport_request;
  transport_request.method = "GET";
  transport_request.url = request.url;
  transport_request.headers = request.headers;
  transport_request.timeout_ms = request.connect_timeout_ms;
  transport_request.maximum_body_bytes =
      request.maximum_bytes == 0
          ? std::numeric_limits<std::size_t>::max()
          : request.maximum_bytes;
  transport_request.follow_redirects = false;
  if (const char* ca_bundle = std::getenv("MOCKTAIL_CA_BUNDLE");
      ca_bundle != nullptr && ca_bundle[0] != '\0') {
    transport_request.ca_bundle = ca_bundle;
  }

  std::vector<std::string> headers = request.headers;
  if (has_existing) {
    headers.push_back("Range: bytes=" + std::to_string(existing) + "-");
  }
  transport_request.headers = headers;

  FileBodySink sink;
  std::ofstream output(temporary,
                       std::ios::binary |
                           (has_existing ? std::ios::app : std::ios::trunc));
  if (!output) {
    result.error = "cannot open temporary download file";
    return result;
  }
  sink.output = &output;
  sink.progress_fd = progress_fd;

  ps4::HttpTransportResponse response;
  response = ps4::PerformHttpRequest(transport_request, &sink);
  output.flush();
  if (!response) {
    result.error = response.error.empty() ? "download failed" : response.error;
    *retryable = true;
    return result;
  }

  if (response.status_code == 206) {
    // Resumed transfer.
    if (!has_existing) {
      result.error = "server resumed a download that had no local bytes";
      *retryable = true;
      return result;
    }
  } else if (response.status_code != 200) {
    result.error = "download request failed with HTTP " +
                   std::to_string(response.status_code);
    *retryable = response.status_code == 429 || response.status_code >= 500;
    return result;
  } else if (has_existing) {
    // Server ignored the Range request; restart from scratch.
    std::error_code error;
    std::filesystem::remove(temporary, error);
    std::ofstream restart(temporary, std::ios::binary | std::ios::trunc);
    if (!restart) {
      result.error = "cannot restart the download";
      *retryable = true;
      return result;
    }
    sink.output = &restart;
    sink.written = 0;
    response = ps4::PerformHttpRequest(transport_request, &sink);
    restart.flush();
    if (!response) {
      result.error = response.error.empty() ? "download failed" : response.error;
      *retryable = true;
      return result;
    }
    if (response.status_code != 200) {
      result.error = "download request failed with HTTP " +
                     std::to_string(response.status_code);
      *retryable = response.status_code >= 500;
      return result;
    }
  }

  if (response.status_code < 200 || response.status_code >= 300) {
    result.error = "download request failed with HTTP " +
                   std::to_string(response.status_code);
    *retryable = response.status_code >= 500;
    return result;
  }

  result.bytes_written = sink.written;
  if (response.status_code == 206 && has_existing) {
    result.bytes_written += existing;
  }
  result.final_url = response.final_url;
  result.sha256 = FileSha256(temporary);
  return result;
}

}  // namespace

bool IsTrustedHttpsUrl(std::string_view url,
                       const std::vector<std::string>& allowed_hosts,
                       std::string* error) {
  std::string scheme;
  std::string host;
  std::string port;
  std::string path;
  if (!ps4::ParseHttpUrl(std::string(url), &scheme, &host, &port, &path)) {
    if (error != nullptr) *error = "URL is malformed";
    return false;
  }
  if (scheme != "https") {
    if (error != nullptr) *error = "URL must be credential-free HTTPS on port 443";
    return false;
  }
  if (!port.empty() && port != "443") {
    if (error != nullptr) *error = "URL must be credential-free HTTPS on port 443";
    return false;
  }
  if (url.find('@') != std::string_view::npos) {
    if (error != nullptr) *error = "URL must be credential-free HTTPS on port 443";
    return false;
  }
  std::string normalized_host = Lower(host);
  while (!normalized_host.empty() && normalized_host.back() == '.') {
    normalized_host.pop_back();
  }
  for (const std::string& allowed : allowed_hosts) {
    if (HostMatches(normalized_host, Lower(allowed))) return true;
  }
  if (error != nullptr) *error = "URL host is not trusted";
  return false;
}

HttpBytesResult DownloadBytes(const HttpTransferRequest& request) {
  HttpBytesResult result;
  if (request.maximum_bytes == 0 ||
      !IsTrustedHttpsUrl(request.url, request.allowed_hosts, &result.error)) {
    if (result.error.empty()) result.error = "HTTP byte limit is zero";
    return result;
  }

  ps4::HttpTransportRequest transport_request;
  transport_request.method = "GET";
  transport_request.url = request.url;
  transport_request.headers = request.headers;
  transport_request.timeout_ms = request.connect_timeout_ms;
  transport_request.maximum_body_bytes = request.maximum_bytes;
  transport_request.follow_redirects = true;
  transport_request.maximum_redirects =
      std::max(1, request.maximum_redirects);
  if (const char* ca_bundle = std::getenv("MOCKTAIL_CA_BUNDLE");
      ca_bundle != nullptr && ca_bundle[0] != '\0') {
    transport_request.ca_bundle = ca_bundle;
  }

  class BufferSink final : public ps4::HttpBodySink {
   public:
    explicit BufferSink(HttpBytesResult* result) : result_(result) {}
    bool OnBodyChunk(const char* data, std::size_t size) override {
      result_->bytes.append(data, size);
      return true;
    }

   private:
    HttpBytesResult* result_;
  };

  BufferSink sink(&result);
  transport_request.maximum_body_bytes =
      std::min(transport_request.maximum_body_bytes, request.maximum_bytes);
  const ps4::HttpTransportResponse response =
      ps4::PerformHttpRequest(transport_request, &sink);
  if (!response) {
    result.error = response.error.empty() ? "download failed" : response.error;
    return result;
  }
  if (response.status_code < 200 || response.status_code >= 300) {
    result.error = "download failed with HTTP " +
                   std::to_string(response.status_code);
    return result;
  }
  result.status_code = response.status_code;
  result.final_url = response.final_url;
  return result;
}

HttpDownloadResult DownloadFile(const HttpTransferRequest& request,
                                const std::filesystem::path& destination,
                                int progress_fd) {
  HttpDownloadResult result;
  if (!IsTrustedHttpsUrl(request.url, request.allowed_hosts, &result.error)) {
    return result;
  }

  std::error_code error;
  std::filesystem::create_directories(destination.parent_path(), error);
  if (error && !std::filesystem::is_directory(destination.parent_path())) {
    result.error = "cannot create the download directory";
    return result;
  }

  const std::filesystem::path temporary =
      destination.string() + ".mocktail-download";
  int attempts = std::max(1, request.maximum_attempts);
  for (int attempt = 1; attempt <= attempts; ++attempt) {
    bool retryable = false;
    result = DownloadFileAttempt(request, temporary, progress_fd, &retryable);
    if (result) {
      std::error_code move_error;
      std::filesystem::rename(temporary, destination, move_error);
      if (move_error) {
        result.error = "cannot finalize the downloaded file";
        return result;
      }
      if (progress_fd >= 0) {
        PublishMessage(progress_fd, "Download complete");
      }
      return result;
    }
    if (!retryable || attempt == attempts) {
      return result;
    }
    if (request.retry_delay_ms > 0) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(request.retry_delay_ms));
    }
  }
  return result;
}

}  // namespace mocktail::update
