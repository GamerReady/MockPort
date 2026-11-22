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

// Minimal OpenSSL EVP compatibility shim for the PS4 port, implemented on
// top of the vendored Mbed TLS. It provides exactly the surface used by
// src/update/apk_signature.cc (digests, signature verification, SPKI and
// X.509 parsing) so that file compiles unchanged on Orbis OS, which has no
// OpenSSL.

#ifndef MOCKTAIL_PS4_PS4_OPENSSL_COMPAT_H_
#define MOCKTAIL_PS4_PS4_OPENSSL_COMPAT_H_

#include <cstddef>
#include <cstdint>

// Digest objects.
struct EVP_MD {
  std::uint32_t type = 0;
};

const EVP_MD* EVP_sha256();
const EVP_MD* EVP_sha512();
std::size_t EVP_MD_get_size(const EVP_MD* digest);

#define EVP_MAX_MD_SIZE 64

// Public key objects.
struct EVP_PKEY;

EVP_PKEY* d2i_PUBKEY(EVP_PKEY** apk, const unsigned char** pp, long length);
void EVP_PKEY_free(EVP_PKEY* key);
int EVP_PKEY_eq(const EVP_PKEY* a, const EVP_PKEY* b);

// RSA padding configuration context.
struct EVP_PKEY_CTX {
  int rsa_padding = 0;
  int rsa_pss_saltlen = -1;
  const EVP_MD* rsa_mgf1_md = nullptr;
};

int EVP_PKEY_CTX_set_rsa_padding(EVP_PKEY_CTX* ctx, int padding);
int EVP_PKEY_CTX_set_rsa_pss_saltlen(EVP_PKEY_CTX* ctx, int saltlen);
int EVP_PKEY_CTX_set_rsa_mgf1_md(EVP_PKEY_CTX* ctx, const EVP_MD* md);

#define RSA_PKCS1_PSS_PADDING 4

// Digest / signature contexts.
struct EVP_MD_CTX;

EVP_MD_CTX* EVP_MD_CTX_new();
void EVP_MD_CTX_free(EVP_MD_CTX* ctx);
int EVP_DigestInit_ex(EVP_MD_CTX* ctx, const EVP_MD* type, void* engine);
int EVP_DigestUpdate(EVP_MD_CTX* ctx, const void* data, std::size_t size);
int EVP_DigestFinal_ex(EVP_MD_CTX* ctx, unsigned char* out,
                       unsigned int* out_size);

// One-shot digest (EVP_Digest).
int EVP_Digest(const unsigned char* data, std::size_t size,
               unsigned char* out, unsigned int* out_size,
               const EVP_MD* type, void* engine);

// Signature verification.
int EVP_DigestVerifyInit(EVP_MD_CTX* ctx, EVP_PKEY_CTX** pctx,
                         const EVP_MD* type, void* engine, EVP_PKEY* key);
int EVP_DigestVerify(EVP_MD_CTX* ctx, const unsigned char* signature,
                     std::size_t signature_size, const unsigned char* data,
                     std::size_t data_size);

// X.509 certificate objects.
struct X509;

X509* d2i_X509(X509** apk, const unsigned char** pp, long length);
void X509_free(X509* certificate);
EVP_PKEY* X509_get_pubkey(X509* certificate);

// Constant-time memory comparison.
int CRYPTO_memcmp(const void* a, const void* b, std::size_t size);

#endif  // MOCKTAIL_PS4_PS4_OPENSSL_COMPAT_H_
