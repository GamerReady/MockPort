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

// PS4 port redirect header: <openssl/rsa.h> resolves to the Mbed TLS
// backed shim when this compatibility directory is on the include path.

#ifndef MOCKTAIL_COMPAT_OPENSSL_RSA_H_
#define MOCKTAIL_COMPAT_OPENSSL_RSA_H_

#include "ps4/ps4_openssl_compat.h"

#endif  // MOCKTAIL_COMPAT_OPENSSL_RSA_H_
