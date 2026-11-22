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

#ifndef MOCKTAIL_SERVICES_SOCKET_HTTP_CLIENT_H_
#define MOCKTAIL_SERVICES_SOCKET_HTTP_CLIENT_H_

#include <string>

#include "services/http_client.h"

namespace mocktail::services {

// HttpClient implementation built on the PS4 socket/TLS transport
// (src/ps4/ps4_http_transport). Used by the PS4 port, where libcurl is not
// available; also usable on any BSD-socket platform with mbedTLS.
//
// The client honours the MOCKTAIL_HTTP_PROXY_* environment variables when a
// proxy is configured, mirroring the libcurl-backed client's behaviour.
class SocketHttpClient final : public HttpClient {
 public:
  HttpResponse Get(const HttpRequest& request) override;
  HttpResponse Post(const HttpRequest& request,
                    const std::string& body) override;
};

}  // namespace mocktail::services

#endif  // MOCKTAIL_SERVICES_SOCKET_HTTP_CLIENT_H_
