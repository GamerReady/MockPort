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

#include "services/socket_http_client.h"

#include <cstdlib>
#include <string>
#include <utility>

#include "ps4/ps4_http_transport.h"
#include "runtime/environment.h"
#include "runtime/runtime_config.h"

namespace mocktail::services {
namespace {

class BufferBodySink final : public ps4::HttpBodySink {
 public:
  explicit BufferBodySink(ps4::HttpTransportResponse* response)
      : response_(response) {}

  bool OnBodyChunk(const char* data, std::size_t size) override {
    response_->body.append(data, size);
    return true;
  }

 private:
  ps4::HttpTransportResponse* response_;
};

ps4::HttpTransportRequest ToTransportRequest(const HttpRequest& request,
                                            const std::string& method,
                                            const std::string& body) {
  ps4::HttpTransportRequest transport_request;
  transport_request.method = method;
  transport_request.url = request.url;
  transport_request.headers = request.headers;
  transport_request.body = body;
  transport_request.timeout_ms = request.timeout_ms;
  transport_request.maximum_body_bytes = request.maximum_body_bytes;
  transport_request.follow_redirects = request.follow_redirects;

  if (const char* ca_bundle = std::getenv("MOCKTAIL_CA_BUNDLE");
      ca_bundle != nullptr && ca_bundle[0] != '\0') {
    transport_request.ca_bundle = ca_bundle;
  }

  const auto proxy = runtime::ReadNetworkProxy(runtime::ProcessEnvironment());
  if (proxy.has_value()) {
    if (proxy->scheme != "http") {
      // The PS4 transport implements HTTP CONNECT tunnelling only.
      transport_request.proxy.clear();
    } else {
      transport_request.proxy = runtime::BuildNetworkProxyUrl(*proxy);
    }
  }
  return transport_request;
}

HttpResponse ToHttpResponse(const ps4::HttpTransportResponse& transport) {
  HttpResponse response(transport.error.empty(), transport.status_code,
                        std::string{}, transport.error,
                        transport.headers);
  response.body = transport.body;
  return response;
}

}  // namespace

HttpResponse SocketHttpClient::Get(const HttpRequest& request) {
  const ps4::HttpTransportRequest transport_request =
      ToTransportRequest(request, "GET", std::string{});
  ps4::HttpTransportResponse transport;
  BufferBodySink sink(&transport);
  transport = ps4::PerformHttpRequest(transport_request, &sink);
  return ToHttpResponse(transport);
}

HttpResponse SocketHttpClient::Post(const HttpRequest& request,
                                    const std::string& body) {
  const ps4::HttpTransportRequest transport_request =
      ToTransportRequest(request, "POST", body);
  ps4::HttpTransportResponse transport;
  BufferBodySink sink(&transport);
  transport = ps4::PerformHttpRequest(transport_request, &sink);
  return ToHttpResponse(transport);
}

}  // namespace mocktail::services
