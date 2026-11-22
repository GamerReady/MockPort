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

#include "ps4/ps4_http_transport.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <mbedtls/error.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

namespace mocktail::ps4 {
namespace {

class TlsSession;

constexpr int kDefaultHttpPort = 80;
constexpr int kDefaultHttpsPort = 443;
constexpr std::size_t kMaxHeaderLine = 64 * 1024;
constexpr std::size_t kMaxHeaderCount = 256;
constexpr std::size_t kBodyChunkSize = 64 * 1024;

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::string Trim(std::string_view value) {
  std::size_t begin = 0;
  std::size_t end = value.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) {
    ++begin;
  }
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return std::string(value.substr(begin, end - begin));
}

// Resolves the default CA bundle: explicit request value, then the
// MOCKTAIL_PS4_CA_BUNDLE environment variable, then cacert.pem next to the
// executable (the PS4 package ships it there).
std::string ResolveCaBundle(const std::string& requested) {
  if (!requested.empty()) {
    return requested;
  }
  if (const char* env = std::getenv("MOCKTAIL_PS4_CA_BUNDLE");
      env != nullptr && env[0] != '\0') {
    return env;
  }
  std::string executable_dir;
  char buffer[4096] = {};
  const ssize_t length = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
  if (length > 0) {
    buffer[length] = '\0';
    const std::string path(buffer);
    const std::size_t separator = path.find_last_of('/');
    executable_dir = separator == std::string::npos ? std::string{}
                                                    : path.substr(0, separator);
  }
  if (!executable_dir.empty()) {
    return executable_dir + "/cacert.pem";
  }
  return "cacert.pem";
}

bool ConnectWithTimeout(const sockaddr* address, socklen_t address_length,
                        long timeout_ms, int* out_fd) {
  const int fd = ::socket(address->sa_family, SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0) {
    return false;
  }
  const int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    ::close(fd);
    return false;
  }
  int result = ::connect(fd, address, address_length);
  if (result != 0 && errno != EINPROGRESS) {
    ::close(fd);
    return false;
  }
  if (result != 0) {
    struct pollfd poll_fd{};
    poll_fd.fd = fd;
    poll_fd.events = POLLOUT;
    const int ready = ::poll(&poll_fd, 1, static_cast<int>(timeout_ms));
    if (ready <= 0) {
      ::close(fd);
      return false;
    }
    int error = 0;
    socklen_t error_length = sizeof(error);
    if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &error_length) != 0 ||
        error != 0) {
      ::close(fd);
      return false;
    }
  }
  const int one = 1;
  ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  struct timeval timeout{};
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = (timeout_ms % 1000) * 1000;
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  *out_fd = fd;
  return true;
}

bool ResolveAddress(const std::string& host, const std::string& port,
                    sockaddr_storage* address, socklen_t* address_length) {
  struct addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo* results = nullptr;
  const std::string port_string = port.empty() ? std::string{} : port;
  const int status =
      ::getaddrinfo(host.c_str(), port_string.empty() ? nullptr
                                                      : port_string.c_str(),
                    &hints, &results);
  if (status != 0 || results == nullptr) {
    return false;
  }
  for (struct addrinfo* entry = results; entry != nullptr;
       entry = entry->ai_next) {
    if (entry->ai_addrlen > sizeof(sockaddr_storage)) {
      continue;
    }
    std::memcpy(address, entry->ai_addr, entry->ai_addrlen);
    *address_length = static_cast<socklen_t>(entry->ai_addrlen);
    ::freeaddrinfo(results);
    return true;
  }
  ::freeaddrinfo(results);
  return false;
}

// Splits "Name: value" header lines into name/value pairs.
bool ParseHeaderLines(const std::string& raw,
                      std::vector<std::pair<std::string, std::string>>* headers);

// Reads one CRLF-terminated line from the transport. Returns false on
// transport failure; an empty line at EOF returns true with `line` empty.
bool ReadLine(TlsSession* tls, int raw_fd, bool use_tls, long timeout_ms,
              std::string* line, std::size_t max_length);

// Streams a response body (Content-Length, chunked, or until-close) into the
// sink. Returns false on protocol error, size overflow, or sink abort.
bool StreamBody(TlsSession* tls, int raw_fd, bool use_tls, long timeout_ms,
                bool chunked, std::int64_t content_length,
                bool connection_close, HttpBodySink* sink,
                std::size_t maximum_body_bytes, std::string* error);

// Reads up to `want` body bytes into the sink. Returns the byte count, or
// -1 on failure, -2 on EOF.
long ReadBody(TlsSession* tls, int raw_fd, bool use_tls, long timeout_ms,
              std::size_t want, HttpBodySink* sink, std::string* error);

// Owns one mbedTLS client session bound to a connected socket.
class TlsSession {
 public:
  TlsSession() {
    mbedtls_ssl_config_init(&config_);
    mbedtls_ssl_init(&ssl_);
  }

  ~TlsSession() {
    mbedtls_ssl_free(&ssl_);
    mbedtls_ssl_config_free(&config_);
    if (ca_chain_ != nullptr) {
      mbedtls_x509_crt_free(ca_chain_);
    }
  }

  bool Start(int fd, const std::string& host, const std::string& ca_bundle,
             std::string* error) {
    mbedtls_ssl_config_defaults(&config_, MBEDTLS_SSL_IS_CLIENT,
                                MBEDTLS_SSL_TRANSPORT_STREAM,
                                MBEDTLS_SSL_PRESET_DEFAULT);
    mbedtls_ssl_conf_authmode(&config_, MBEDTLS_SSL_VERIFY_REQUIRED);

    std::vector<unsigned char> bundle;
    if (!ReadFileBytes(ca_bundle, &bundle)) {
      *error = "cannot read CA bundle " + ca_bundle;
      return false;
    }
    ca_chain_ = new mbedtls_x509_crt[1];
    mbedtls_x509_crt_init(ca_chain_);
    const int parsed =
        mbedtls_x509_crt_parse(ca_chain_, bundle.data(), bundle.size());
    if (parsed <= 0) {
      *error = "cannot parse CA bundle " + ca_bundle;
      return false;
    }
    mbedtls_ssl_conf_ca_chain(&config_, ca_chain_, nullptr);

    if (mbedtls_ssl_setup(&ssl_, &config_) != 0) {
      *error = "cannot create mbedTLS context";
      return false;
    }
    fd_ = fd;
    mbedtls_ssl_set_bio(&ssl_, this, TlsSend, TlsRecv, nullptr);
    if (mbedtls_ssl_set_hostname(&ssl_, host.c_str()) != 0) {
      *error = "cannot set TLS hostname";
      return false;
    }
    const int handshake_error = mbedtls_ssl_handshake(&ssl_);
    if (handshake_error != 0) {
      *error = "TLS handshake failed: " + TlsError(handshake_error);
      return false;
    }
    const uint32_t verify_result = mbedtls_ssl_get_verify_result(&ssl_);
    if (verify_result != 0) {
      *error = "TLS certificate verification failed: " +
               TlsError(static_cast<int>(verify_result));
      return false;
    }
    return true;
  }

  bool Write(const char* data, std::size_t size, std::string* error) {
    std::size_t written = 0;
    while (written < size) {
      const int count =
          mbedtls_ssl_write(&ssl_,
                            reinterpret_cast<const unsigned char*>(
                                data + written),
                            static_cast<size_t>(size - written));
      if (count > 0) {
        written += static_cast<std::size_t>(count);
        continue;
      }
      if (count == MBEDTLS_ERR_SSL_WANT_READ ||
          count == MBEDTLS_ERR_SSL_WANT_WRITE) {
        continue;
      }
      *error = "TLS write failed: " + TlsError(count);
      return false;
    }
    return true;
  }

  // Reads up to `size` bytes. Returns the number of bytes read (>= 0), or
  // -1 on failure, -2 on EOF / close_notify.
  long Read(char* data, std::size_t size) {
    const int count =
        mbedtls_ssl_read(&ssl_, reinterpret_cast<unsigned char*>(data),
                         static_cast<size_t>(size));
    if (count > 0) {
      return count;
    }
    return -1;
  }

 private:
  static int TlsSend(void* context, const unsigned char* data, size_t size) {
    auto* self = static_cast<TlsSession*>(context);
    std::size_t total = 0;
    while (total < size) {
      const ssize_t count = ::send(self->fd_, data + total, size - total, 0);
      if (count > 0) {
        total += static_cast<std::size_t>(count);
        continue;
      }
      if (count < 0 && errno == EINTR) {
        continue;
      }
      return static_cast<int>(total);
    }
    return static_cast<int>(total);
  }

  static int TlsRecv(void* context, unsigned char* data, size_t size) {
    auto* self = static_cast<TlsSession*>(context);
    for (;;) {
      const ssize_t count = ::recv(self->fd_, data, size, 0);
      if (count > 0) {
        return static_cast<int>(count);
      }
      if (count == 0) {
        return 0;
      }
      if (count < 0 && errno == EINTR) {
        continue;
      }
      return -1;
    }
  }

  static bool ReadFileBytes(const std::string& path,
                            std::vector<unsigned char>* output) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
      return false;
    }
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size <= 0) {
      std::fclose(file);
      return false;
    }
    output->resize(static_cast<std::size_t>(size));
    const std::size_t read = std::fread(output->data(), 1, output->size(), file);
    std::fclose(file);
    return read == output->size();
  }

  static std::string TlsError(int code) {
    char buffer[256] = {};
    mbedtls_strerror(code, buffer, sizeof(buffer));
    return std::string(buffer);
  }

  int fd_ = -1;
  mbedtls_ssl_config config_{};
  mbedtls_ssl_context ssl_{};
  mbedtls_x509_crt* ca_chain_ = nullptr;
};

bool ReadLine(TlsSession* tls, int raw_fd, bool use_tls, long timeout_ms,
              std::string* line, std::size_t max_length) {
  line->clear();
  char buffer[1024];
  while (line->size() < max_length) {
    long got = 0;
    if (use_tls) {
      got = tls->Read(buffer, sizeof(buffer));
    } else {
      struct pollfd poll_fd{};
      poll_fd.fd = raw_fd;
      poll_fd.events = POLLIN;
      const int ready = ::poll(&poll_fd, 1, static_cast<int>(timeout_ms));
      if (ready <= 0) {
        return false;
      }
      const ssize_t count = ::recv(raw_fd, buffer, sizeof(buffer), 0);
      if (count > 0) {
        got = static_cast<long>(count);
      } else if (count == 0) {
        got = -2;
      } else if (errno == EINTR) {
        continue;
      } else {
        got = -1;
      }
    }
    if (got == -2) {
      return line->empty();
    }
    if (got <= 0) {
      return false;
    }
    line->append(buffer, static_cast<std::size_t>(got));
    const std::size_t newline = line->find('\n');
    if (newline != std::string::npos) {
      *line = line->substr(0, newline);
      if (!line->empty() && line->back() == '\r') {
        line->pop_back();
      }
      return true;
    }
  }
  return false;
}

long ReadBody(TlsSession* tls, int raw_fd, bool use_tls, long timeout_ms,
              std::size_t want, HttpBodySink* sink, std::string* error) {
  if (want == 0) {
    return 0;
  }
  std::vector<char> buffer(std::min(want, kBodyChunkSize));
  std::size_t total = 0;
  while (total < buffer.size()) {
    long got = 0;
    if (use_tls) {
      got = tls->Read(buffer.data() + total, buffer.size() - total);
    } else {
      struct pollfd poll_fd{};
      poll_fd.fd = raw_fd;
      poll_fd.events = POLLIN;
      const int ready = ::poll(&poll_fd, 1, static_cast<int>(timeout_ms));
      if (ready <= 0) {
        *error = "timed out reading the response body";
        return -1;
      }
      const ssize_t count =
          ::recv(raw_fd, buffer.data() + total, buffer.size() - total, 0);
      if (count > 0) {
        got = static_cast<long>(count);
      } else if (count == 0) {
        got = -2;
      } else if (errno == EINTR) {
        continue;
      } else {
        got = -1;
      }
    }
    if (got == -2) {
      return total == 0 ? -2 : static_cast<long>(total);
    }
    if (got <= 0) {
      *error = "connection closed while reading the response body";
      return -1;
    }
    total += static_cast<std::size_t>(got);
    if (sink != nullptr && !sink->OnBodyChunk(buffer.data(), total)) {
      *error = "body sink aborted the transfer";
      return -1;
    }
  }
  return static_cast<long>(total);
}

bool StreamBody(TlsSession* tls, int raw_fd, bool use_tls, long timeout_ms,
                bool chunked, std::int64_t content_length,
                bool connection_close, HttpBodySink* sink,
                std::size_t maximum_body_bytes, std::string* error) {
  std::int64_t remaining = content_length;
  std::int64_t total = 0;

  if (chunked) {
    std::int64_t chunk_remaining = 0;
    bool in_chunk = false;
    while (true) {
      if (!in_chunk) {
        std::string line;
        if (!ReadLine(tls, raw_fd, use_tls, timeout_ms, &line, 8192)) {
          *error = "truncated chunked body";
          return false;
        }
        const std::size_t semicolon = line.find(';');
        const std::string size_text = Trim(line.substr(0, semicolon));
        char* end = nullptr;
        const long chunk_size = std::strtol(size_text.c_str(), &end, 16);
        if (end == size_text.c_str() || chunk_size < 0) {
          *error = "invalid chunk size";
          return false;
        }
        if (chunk_size == 0) {
          break;
        }
        chunk_remaining = chunk_size;
        in_chunk = true;
      }
      const std::size_t want =
          std::min<std::int64_t>(chunk_remaining, kBodyChunkSize);
      const long got =
          ReadBody(tls, raw_fd, use_tls, timeout_ms, want, sink, error);
      if (got < 0) {
        return false;
      }
      chunk_remaining -= got;
      total += got;
      if (static_cast<std::size_t>(total) > maximum_body_bytes) {
        *error = "response body exceeds the maximum size";
        return false;
      }
      if (chunk_remaining == 0) {
        in_chunk = false;
        std::string line;
        if (!ReadLine(tls, raw_fd, use_tls, timeout_ms, &line, 8192)) {
          *error = "truncated chunk terminator";
          return false;
        }
      }
    }
    return true;
  }

  if (remaining > static_cast<std::int64_t>(maximum_body_bytes)) {
    *error = "response body exceeds the maximum size";
    return false;
  }
  while (remaining > 0 || connection_close) {
    const std::size_t want =
        remaining == 0
            ? kBodyChunkSize
            : static_cast<std::size_t>(std::min<std::int64_t>(
                  remaining, kBodyChunkSize));
    const long got = ReadBody(tls, raw_fd, use_tls, timeout_ms, want, sink,
                              error);
    if (got < 0) {
      return false;
    }
    if (got == 0) {
      break;
    }
    if (remaining > 0) {
      remaining -= got;
    }
    total += got;
    if (static_cast<std::size_t>(total) > maximum_body_bytes) {
      *error = "response body exceeds the maximum size";
      return false;
    }
  }
  return true;
}

bool SendPlain(int raw_fd, const std::string& data, std::string* error) {
  std::size_t sent = 0;
  while (sent < data.size()) {
    const ssize_t count =
        ::send(raw_fd, data.data() + sent, data.size() - sent, 0);
    if (count > 0) {
      sent += static_cast<std::size_t>(count);
      continue;
    }
    if (count < 0 && errno == EINTR) {
      continue;
    }
    *error = "cannot send HTTP request";
    return false;
  }
  return true;
}

// Sends one request over an established transport and parses the response.
bool PerformOnce(TlsSession* tls, int raw_fd, bool use_tls, long timeout_ms,
                 const std::string& method, const std::string& body,
                 const HttpTransportRequest& request, const std::string& host,
                 const std::string& path_and_query, bool via_proxy,
                 HttpTransportResponse* response, HttpBodySink* sink) {
  std::string request_line = method + " ";
  request_line += via_proxy ? request.url : path_and_query;
  request_line += " HTTP/1.1\r\n";
  request_line += "Host: " + host + "\r\n";
  request_line += "User-Agent: MockPort/1.0 (PS4)\r\n";
  request_line += "Accept: */*\r\n";
  request_line += "Connection: close\r\n";
  for (const std::string& header : request.headers) {
    request_line += header;
    if (request_line.empty() || request_line.back() != '\n') {
      request_line += "\r\n";
    }
  }
  if (!body.empty()) {
    request_line += "Content-Length: " + std::to_string(body.size()) +
                    "\r\n";
  }
  request_line += "\r\n";
  request_line += body;

  if (use_tls) {
    std::string error;
    if (!tls->Write(request_line.data(), request_line.size(), &error)) {
      response->error = error;
      return false;
    }
  } else if (!SendPlain(raw_fd, request_line, &response->error)) {
    return false;
  }

  std::string status_line;
  if (!ReadLine(tls, raw_fd, use_tls, timeout_ms, &status_line, 8192)) {
    response->error = "no HTTP response received";
    return false;
  }
  const std::size_t first_space = status_line.find(' ');
  if (first_space == std::string::npos) {
    response->error = "malformed HTTP status line";
    return false;
  }
  response->status_code = std::strtol(status_line.c_str() + first_space + 1,
                                      nullptr, 10);

  std::string header_block;
  while (true) {
    std::string line;
    if (!ReadLine(tls, raw_fd, use_tls, timeout_ms, &line, kMaxHeaderLine)) {
      response->error = "truncated HTTP headers";
      return false;
    }
    if (line.empty()) {
      break;
    }
    header_block += line;
    header_block += '\n';
  }
  std::vector<std::pair<std::string, std::string>> headers;
  if (!ParseHeaderLines(header_block, &headers)) {
    response->error = "malformed HTTP headers";
    return false;
  }
  for (const auto& header : headers) {
    response->headers.push_back(header.first + ": " + header.second);
  }

  const std::string* transfer_encoding = nullptr;
  const std::string* content_length = nullptr;
  const std::string* connection = nullptr;
  for (const auto& header : headers) {
    if (header.first == "transfer-encoding") {
      transfer_encoding = &header.second;
    } else if (header.first == "content-length") {
      content_length = &header.second;
    } else if (header.first == "connection") {
      connection = &header.second;
    }
  }
  const bool chunked =
      transfer_encoding != nullptr &&
      ToLower(*transfer_encoding).find("chunked") != std::string::npos;
  std::int64_t content_length_value = 0;
  bool has_content_length = false;
  if (content_length != nullptr) {
    char* end = nullptr;
    const long parsed = std::strtol(content_length->c_str(), &end, 10);
    if (end != content_length->c_str() && parsed >= 0) {
      content_length_value = parsed;
      has_content_length = true;
    }
  }
  const bool connection_close =
      connection != nullptr &&
      ToLower(*connection).find("close") != std::string::npos;

  std::string body_error;
  if (!StreamBody(tls, raw_fd, use_tls, timeout_ms, chunked,
                  has_content_length ? content_length_value : -1,
                  connection_close, sink, request.maximum_body_bytes,
                  &body_error)) {
    response->error = body_error;
    return false;
  }
  return true;
}

bool ParseHeaderLines(const std::string& raw,
                      std::vector<std::pair<std::string, std::string>>* headers) {
  std::size_t position = 0;
  while (position < raw.size()) {
    const std::size_t newline = raw.find('\n', position);
    std::string line = raw.substr(position,
                                  newline == std::string::npos
                                      ? std::string::npos
                                      : newline - position);
    position = newline == std::string::npos ? raw.size() : newline + 1;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      continue;
    }
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos) {
      return false;
    }
    headers->emplace_back(ToLower(Trim(line.substr(0, colon))),
                          Trim(line.substr(colon + 1)));
    if (headers->size() > kMaxHeaderCount) {
      return false;
    }
  }
  return true;
}

// Establishes the transport for one attempt: direct or proxied, plain or TLS.
struct Attempt {
  int raw_fd = -1;
  std::unique_ptr<TlsSession> tls;
  bool use_tls = false;
  bool via_proxy = false;

  ~Attempt() {
    if (raw_fd >= 0) {
      ::close(raw_fd);
    }
  }
};

bool OpenAttempt(const HttpTransportRequest& request, const std::string& host,
                 const std::string& port, bool want_tls, Attempt* attempt,
                 std::string* error) {
  sockaddr_storage address{};
  socklen_t address_length = 0;
  std::string connect_host = host;
  std::string connect_port = port;
  attempt->via_proxy = false;

  const std::string& proxy = request.proxy;
  if (!proxy.empty()) {
    std::string proxy_scheme;
    std::string proxy_host;
    std::string proxy_port;
    std::string proxy_path;
    if (!ParseHttpUrl(proxy, &proxy_scheme, &proxy_host, &proxy_port,
                      &proxy_path)) {
      *error = "invalid proxy URL";
      return false;
    }
    connect_host = proxy_host;
    connect_port = proxy_port.empty()
                       ? (proxy_scheme == "https" ? "443" : "80")
                       : proxy_port;
    attempt->via_proxy = true;
  }

  if (!ResolveAddress(connect_host, connect_port, &address, &address_length)) {
    *error = "cannot resolve " + connect_host;
    return false;
  }
  if (!ConnectWithTimeout(reinterpret_cast<const sockaddr*>(&address),
                          address_length, request.timeout_ms,
                          &attempt->raw_fd)) {
    *error = "cannot connect to " + connect_host + ":" + connect_port;
    return false;
  }

  if (attempt->via_proxy && want_tls) {
    // Tunnel TLS through the proxy with CONNECT.
    const std::string connect_request =
        "CONNECT " + host + ":" + port + " HTTP/1.1\r\n"
        "Host: " + host + ":" + port + "\r\n"
        "Proxy-Connection: Keep-Alive\r\n\r\n";
    std::string error_string;
    if (!SendPlain(attempt->raw_fd, connect_request, &error_string)) {
      *error = "cannot send CONNECT to proxy";
      return false;
    }
    std::string status_line;
    if (!ReadLine(nullptr, attempt->raw_fd, false, request.timeout_ms,
                  &status_line, 8192)) {
      *error = "proxy did not respond to CONNECT";
      return false;
    }
    const std::size_t space = status_line.find(' ');
    if (space == std::string::npos ||
        std::strtol(status_line.c_str() + space + 1, nullptr, 10) != 200) {
      *error = "proxy refused CONNECT tunnel";
      return false;
    }
    while (true) {
      std::string line;
      if (!ReadLine(nullptr, attempt->raw_fd, false, request.timeout_ms,
                    &line, 8192) ||
          line.empty()) {
        break;
      }
    }
  }

  attempt->use_tls = want_tls;
  if (want_tls) {
    attempt->tls = std::make_unique<TlsSession>();
    if (!attempt->tls->Start(attempt->raw_fd, host,
                             ResolveCaBundle(request.ca_bundle), error)) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool ParseHttpUrl(const std::string& url, std::string* scheme,
                  std::string* host, std::string* port, std::string* path) {
  const std::size_t scheme_end = url.find("://");
  if (scheme_end == std::string::npos) {
    return false;
  }
  *scheme = ToLower(url.substr(0, scheme_end));
  if (*scheme != "http" && *scheme != "https") {
    return false;
  }
  const std::size_t authority_start = scheme_end + 3;
  const std::size_t path_start = url.find('/', authority_start);
  const std::string authority =
      path_start == std::string::npos
          ? url.substr(authority_start)
          : url.substr(authority_start, path_start - authority_start);
  *path = path_start == std::string::npos ? "/" : url.substr(path_start);

  if (!authority.empty() && authority.front() == '[') {
    // IPv6 literal: [::1]:8080
    const std::size_t close = authority.find(']');
    if (close == std::string::npos) {
      return false;
    }
    *host = authority.substr(1, close - 1);
    if (close + 1 < authority.size() && authority[close + 1] == ':') {
      *port = authority.substr(close + 2);
    } else {
      *port = (*scheme == "https") ? "443" : "80";
    }
    return !host->empty();
  }
  const std::size_t colon = authority.rfind(':');
  if (colon != std::string::npos) {
    *host = authority.substr(0, colon);
    *port = authority.substr(colon + 1);
  } else {
    *host = authority;
    *port = (*scheme == "https") ? "443" : "80";
  }
  return !host->empty();
}

std::string HttpUrlHost(const std::string& url) {
  std::string scheme;
  std::string host;
  std::string port;
  std::string path;
  if (!ParseHttpUrl(url, &scheme, &host, &port, &path)) {
    return {};
  }
  return host;
}

HttpTransportResponse PerformHttpRequest(const HttpTransportRequest& request,
                                         HttpBodySink* sink) {
  HttpTransportResponse response;
  response.final_url = request.url;

  std::string scheme;
  std::string host;
  std::string port;
  std::string path;
  if (!ParseHttpUrl(request.url, &scheme, &host, &port, &path)) {
    response.error = "unsupported or malformed URL: " + request.url;
    return response;
  }
  const std::string path_and_query = path.empty() ? "/" : path;

  std::string method = request.method;
  std::string body = request.body;
  int redirects = 0;

  while (true) {
    Attempt attempt;
    std::string error;
    if (!OpenAttempt(request, host, port, scheme == "https", &attempt,
                     &error)) {
      response.error = error;
      return response;
    }
    if (!PerformOnce(attempt.tls.get(), attempt.raw_fd, attempt.use_tls,
                     request.timeout_ms, method, body, request, host,
                     path_and_query, attempt.via_proxy, &response, sink)) {
      if (response.error.empty()) {
        response.error = "HTTP request failed";
      }
      return response;
    }

    if (!request.follow_redirects ||
        (response.status_code != 301 && response.status_code != 302 &&
         response.status_code != 303 && response.status_code != 307 &&
         response.status_code != 308)) {
      response.final_url = request.url;
      return response;
    }

    const std::string* location = nullptr;
    for (const std::string& header : response.headers) {
      const std::size_t colon = header.find(':');
      if (colon == std::string::npos) {
        continue;
      }
      if (ToLower(Trim(header.substr(0, colon))) == "location") {
        location = &header;
        break;
      }
    }
    if (location == nullptr) {
      response.error = "redirect without a Location header";
      return response;
    }
    const std::string target =
        Trim(location->substr(location->find(':') + 1));
    std::string next_url;
    if (!target.empty() && target.find("://") != std::string::npos) {
      next_url = target;
    } else {
      const std::size_t scheme_end = request.url.find("://");
      const std::size_t authority_end =
          request.url.find('/', scheme_end + 3);
      const std::string base = authority_end == std::string::npos
                                   ? request.url
                                   : request.url.substr(0, authority_end);
      next_url = base + (target.empty() ? "/" : target);
    }
    if (++redirects > request.maximum_redirects) {
      response.error = "too many HTTP redirects";
      return response;
    }
    if (response.status_code == 303 ||
        (response.status_code != 307 && response.status_code != 308)) {
      method = "GET";
      body.clear();
    }
    std::string next_scheme;
    std::string next_host;
    std::string next_port;
    std::string next_path;
    if (!ParseHttpUrl(next_url, &next_scheme, &next_host, &next_port,
                      &next_path)) {
      response.error = "invalid redirect target: " + next_url;
      return response;
    }
    scheme = next_scheme;
    host = next_host;
    port = next_port;
    path = next_path;
  }
}

}  // namespace mocktail::ps4
