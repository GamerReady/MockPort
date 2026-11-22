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

#ifndef MOCKTAIL_PS4_PS4_HTTP_TRANSPORT_H_
#define MOCKTAIL_PS4_PS4_HTTP_TRANSPORT_H_

#include <cstddef>
#include <string>
#include <vector>

namespace mocktail::ps4 {

// Low-level HTTP/1.1 transport used by the PS4 port. The jailbroken PS4 has
// no libcurl and no OpenSSL, so this module implements plain sockets plus
// mbedTLS-backed TLS, redirect following, and optional HTTP proxy support
// (including CONNECT tunnelling for https through a tunneling proxy).
//
// All functions are safe to call from any thread; each performs a complete,
// self-contained request.

struct HttpTransportRequest {
  std::string method = "GET";
  std::string url;
  std::vector<std::string> headers;
  std::string body;

  long timeout_ms = 15000;
  std::size_t maximum_body_bytes = 16 * 1024 * 1024;
  bool follow_redirects = true;
  int maximum_redirects = 8;

  // PEM CA bundle used for https certificate verification. Empty selects
  // the bundled default (MOCKTAIL_PS4_CA_BUNDLE, then exe/cacert.pem).
  std::string ca_bundle;

  // Optional HTTP proxy ("http://host:port"). Empty means direct.
  std::string proxy;
};

struct HttpTransportResponse {
  long status_code = 0;
  std::vector<std::string> headers;
  std::string body;
  std::string final_url;
  std::string error;

  explicit operator bool() const { return error.empty() && status_code > 0; }
};

// Receives response body chunks. Return false to abort the transfer.
class HttpBodySink {
 public:
  virtual ~HttpBodySink() = default;
  virtual bool OnBodyChunk(const char* data, std::size_t size) = 0;
};

// Performs one HTTP request, following redirects, and delivers the response
// body through the sink (or into `response.body` when sink is null).
// Transport-level failures (DNS, connect, TLS, protocol) are reported in
// `response.error`; HTTP error status codes are reported in
// `response.status_code` with an empty error.
HttpTransportResponse PerformHttpRequest(const HttpTransportRequest& request,
                                         HttpBodySink* sink);

// Parses "scheme://host[:port]/path" and fills the components. Returns false
// for malformed URLs or unsupported schemes.
bool ParseHttpUrl(const std::string& url, std::string* scheme,
                  std::string* host, std::string* port, std::string* path);

// Extracts the lowercase host from a URL, or empty on parse failure.
std::string HttpUrlHost(const std::string& url);

}  // namespace mocktail::ps4

#endif  // MOCKTAIL_PS4_PS4_HTTP_TRANSPORT_H_
