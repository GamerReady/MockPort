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

// Implementation of the PS4 OpenSSL EVP compatibility shim on top of the
// vendored Mbed TLS. See include/ps4/ps4_openssl_compat.h.
//
// All symbols live in the global namespace so that upstream sources which
// #include <openssl/evp.h> (redirected to the shim headers) compile
// unchanged.

// Allow direct access to Mbed TLS context members (pk_info, raw).
#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include "ps4/ps4_openssl_compat.h"

#include <mbedtls/ecdsa.h>
#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha512.h>
#include <mbedtls/x509_crt.h>

#include <cstring>
#include <vector>

namespace {

enum DigestType {
  kSha256 = 1,
  kSha512 = 2,
};

mbedtls_md_type_t MbedType(const EVP_MD* digest) {
  return digest != nullptr && digest->type == kSha512 ? MBEDTLS_MD_SHA512
                                                      : MBEDTLS_MD_SHA256;
}

std::size_t DigestSize(DigestType type) { return type == kSha512 ? 64 : 32; }

}  // namespace

struct EVP_PKEY {
  mbedtls_pk_context key;
};

struct EVP_MD_CTX {
  DigestType digest = kSha256;
  bool digest_ready = false;
  bool verify_ready = false;
  mbedtls_sha256_context sha256{};
  mbedtls_sha512_context sha512{};
  mbedtls_pk_context key{};
  EVP_PKEY_CTX key_ctx{};
};

struct X509 {
  mbedtls_x509_crt crt;
};

const EVP_MD* EVP_sha256() {
  static const EVP_MD digest{kSha256};
  return &digest;
}

const EVP_MD* EVP_sha512() {
  static const EVP_MD digest{kSha512};
  return &digest;
}

std::size_t EVP_MD_get_size(const EVP_MD* digest) {
  if (digest != nullptr && digest->type == kSha512) {
    return 64;
  }
  return 32;
}

EVP_MD_CTX* EVP_MD_CTX_new() {
  auto* ctx = new EVP_MD_CTX();
  mbedtls_sha256_starts(&ctx->sha256, 0);
  mbedtls_sha512_starts(&ctx->sha512, 0);
  mbedtls_pk_init(&ctx->key);
  return ctx;
}

void EVP_MD_CTX_free(EVP_MD_CTX* ctx) {
  if (ctx == nullptr) {
    return;
  }
  if (ctx->verify_ready) {
    mbedtls_pk_free(&ctx->key);
  }
  delete ctx;
}

int EVP_DigestInit_ex(EVP_MD_CTX* ctx, const EVP_MD* type, void* engine) {
  (void)engine;
  if (ctx == nullptr || type == nullptr) {
    return 0;
  }
  ctx->digest = type->type == kSha512 ? kSha512 : kSha256;
  ctx->digest_ready = true;
  ctx->verify_ready = false;
  if (ctx->digest == kSha256) {
    mbedtls_sha256_starts(&ctx->sha256, 0);
  } else {
    mbedtls_sha512_starts(&ctx->sha512, 0);
  }
  return 1;
}

int EVP_DigestUpdate(EVP_MD_CTX* ctx, const void* data, std::size_t size) {
  if (ctx == nullptr || data == nullptr || !ctx->digest_ready) {
    return 0;
  }
  if (ctx->digest == kSha256) {
    return mbedtls_sha256_update(&ctx->sha256,
                                 static_cast<const unsigned char*>(data),
                                 size) == 0 ? 1 : 0;
  }
  return mbedtls_sha512_update(&ctx->sha512,
                               static_cast<const unsigned char*>(data),
                               size) == 0 ? 1 : 0;
}

int EVP_DigestFinal_ex(EVP_MD_CTX* ctx, unsigned char* out,
                       unsigned int* out_size) {
  if (ctx == nullptr || out == nullptr || out_size == nullptr ||
      !ctx->digest_ready) {
    return 0;
  }
  unsigned char digest[EVP_MAX_MD_SIZE] = {};
  if (ctx->digest == kSha256) {
    if (mbedtls_sha256_finish(&ctx->sha256, digest) != 0) {
      return 0;
    }
  } else {
    if (mbedtls_sha512_finish(&ctx->sha512, digest) != 0) {
      return 0;
    }
  }
  const std::size_t size = DigestSize(ctx->digest);
  std::memcpy(out, digest, size);
  *out_size = static_cast<unsigned int>(size);
  return 1;
}

int EVP_Digest(const unsigned char* data, std::size_t size,
               unsigned char* out, unsigned int* out_size,
               const EVP_MD* type, void* engine) {
  (void)engine;
  if (data == nullptr || out == nullptr || out_size == nullptr ||
      type == nullptr) {
    return 0;
  }
  if (type->type == kSha512) {
    if (mbedtls_sha512(data, size, out, 0) != 0) {
      return 0;
    }
    *out_size = 64;
    return 1;
  }
  if (mbedtls_sha256(data, size, out, 0) != 0) {
    return 0;
  }
  *out_size = 32;
  return 1;
}

EVP_PKEY* d2i_PUBKEY(EVP_PKEY** apk, const unsigned char** pp, long length) {
  if (pp == nullptr || *pp == nullptr || length <= 0) {
    return nullptr;
  }
  auto* key = new EVP_PKEY();
  mbedtls_pk_init(&key->key);
  unsigned char* cursor = const_cast<unsigned char*>(*pp);
  const unsigned char* end = cursor + length;
  if (mbedtls_pk_parse_subpubkey(&cursor, end, &key->key) != 0 ||
      cursor != end) {
    mbedtls_pk_free(&key->key);
    delete key;
    return nullptr;
  }
  *pp = cursor;
  if (apk != nullptr) {
    *apk = key;
  }
  return key;
}

void EVP_PKEY_free(EVP_PKEY* key) {
  if (key == nullptr) {
    return;
  }
  mbedtls_pk_free(&key->key);
  delete key;
}

int EVP_PKEY_eq(const EVP_PKEY* a, const EVP_PKEY* b) {
  if (a == nullptr || b == nullptr) {
    return 0;
  }
  if (mbedtls_pk_get_bitlen(&a->key) != mbedtls_pk_get_bitlen(&b->key)) {
    return 0;
  }
  // mbedtls_pk_dup/mbedtls_pk_cmp do not exist in Mbed TLS 3.6; compare the
  // DER SubjectPublicKeyInfo encodings instead.
  const std::size_t capacity =
      mbedtls_pk_get_bitlen(&a->key) / 8U * 2U + 64U;
  std::vector<unsigned char> left(capacity);
  std::vector<unsigned char> right(capacity);
  const int left_size = mbedtls_pk_write_pubkey_der(
      &a->key, left.data(), static_cast<size_t>(left.size()));
  const int right_size = mbedtls_pk_write_pubkey_der(
      &b->key, right.data(), static_cast<size_t>(right.size()));
  if (left_size <= 0 || right_size <= 0 || left_size != right_size) {
    return 0;
  }
  return CRYPTO_memcmp(left.data(), right.data(),
                       static_cast<std::size_t>(left_size)) == 0 ? 1 : 0;
}

int EVP_PKEY_CTX_set_rsa_padding(EVP_PKEY_CTX* ctx, int padding) {
  if (ctx == nullptr) {
    return 0;
  }
  ctx->rsa_padding = padding;
  return 1;
}

int EVP_PKEY_CTX_set_rsa_pss_saltlen(EVP_PKEY_CTX* ctx, int saltlen) {
  if (ctx == nullptr) {
    return 0;
  }
  ctx->rsa_pss_saltlen = saltlen;
  return 1;
}

int EVP_PKEY_CTX_set_rsa_mgf1_md(EVP_PKEY_CTX* ctx, const EVP_MD* md) {
  if (ctx == nullptr || md == nullptr) {
    return 0;
  }
  ctx->rsa_mgf1_md = md;
  return 1;
}

int EVP_DigestVerifyInit(EVP_MD_CTX* ctx, EVP_PKEY_CTX** pctx,
                         const EVP_MD* type, void* engine, EVP_PKEY* key) {
  (void)engine;
  if (ctx == nullptr || type == nullptr || key == nullptr ||
      key->key.pk_info == nullptr) {
    return 0;
  }
  ctx->digest = type->type == kSha512 ? kSha512 : kSha256;
  ctx->digest_ready = true;
  if (ctx->verify_ready) {
    mbedtls_pk_free(&ctx->key);
    ctx->verify_ready = false;
  }
  if (mbedtls_pk_setup(&ctx->key, key->key.pk_info) != 0) {
    return 0;
  }
  ctx->verify_ready = true;
  if (ctx->digest == kSha256) {
    mbedtls_sha256_starts(&ctx->sha256, 0);
  } else {
    mbedtls_sha512_starts(&ctx->sha512, 0);
  }
  ctx->key_ctx.rsa_padding = 0;
  ctx->key_ctx.rsa_pss_saltlen = -1;
  ctx->key_ctx.rsa_mgf1_md = nullptr;
  if (pctx != nullptr) {
    *pctx = &ctx->key_ctx;
  }
  return 1;
}

int EVP_DigestVerify(EVP_MD_CTX* ctx, const unsigned char* signature,
                     std::size_t signature_size, const unsigned char* data,
                     std::size_t data_size) {
  if (ctx == nullptr || !ctx->digest_ready || !ctx->verify_ready ||
      signature == nullptr || data == nullptr) {
    return 0;
  }

  unsigned char digest[EVP_MAX_MD_SIZE] = {};
  if (ctx->digest == kSha256) {
    mbedtls_sha256_context hash_ctx = ctx->sha256;
    if (mbedtls_sha256_update(&hash_ctx, data, data_size) != 0 ||
        mbedtls_sha256_finish(&hash_ctx, digest) != 0) {
      return 0;
    }
  } else {
    mbedtls_sha512_context hash_ctx = ctx->sha512;
    if (mbedtls_sha512_update(&hash_ctx, data, data_size) != 0 ||
        mbedtls_sha512_finish(&hash_ctx, digest) != 0) {
      return 0;
    }
  }

  const std::size_t digest_size = DigestSize(ctx->digest);
  const mbedtls_md_type_t md_type =
      ctx->digest == kSha512 ? MBEDTLS_MD_SHA512 : MBEDTLS_MD_SHA256;
  const mbedtls_md_type_t mgf1_type =
      ctx->key_ctx.rsa_mgf1_md != nullptr
          ? MbedType(ctx->key_ctx.rsa_mgf1_md)
          : md_type;

  switch (mbedtls_pk_get_type(&ctx->key)) {
    case MBEDTLS_PK_RSA:
    case MBEDTLS_PK_RSA_ALT:
      if (ctx->key_ctx.rsa_padding == RSA_PKCS1_PSS_PADDING) {
        const int salt_len =
            ctx->key_ctx.rsa_pss_saltlen >= 0
                ? ctx->key_ctx.rsa_pss_saltlen
                : static_cast<int>(digest_size);
        return mbedtls_rsa_rsassa_pss_verify_ext(
                   mbedtls_pk_rsa(ctx->key), md_type,
                   static_cast<unsigned int>(digest_size), digest, mgf1_type,
                   salt_len, signature) == 0
                   ? 1
                   : 0;
      }
      return mbedtls_pk_verify(&ctx->key, md_type, digest, digest_size,
                               signature, signature_size) == 0 ? 1 : 0;
    case MBEDTLS_PK_ECKEY:
      return mbedtls_ecdsa_read_signature(
                 mbedtls_pk_ec(ctx->key), digest, digest_size, signature,
                 signature_size) == 0
                 ? 1
                 : 0;
    default:
      return 0;
  }
}

X509* d2i_X509(X509** apk, const unsigned char** pp, long length) {
  if (pp == nullptr || *pp == nullptr || length <= 0) {
    return nullptr;
  }
  auto* certificate = new X509();
  mbedtls_x509_crt_init(&certificate->crt);
  const unsigned char* cursor = *pp;
  if (mbedtls_x509_crt_parse_der(&certificate->crt, cursor,
                                 static_cast<std::size_t>(length)) != 0) {
    mbedtls_x509_crt_free(&certificate->crt);
    delete certificate;
    return nullptr;
  }
  cursor += certificate->crt.raw.len;
  *pp = cursor;
  if (apk != nullptr) {
    *apk = certificate;
  }
  return certificate;
}

void X509_free(X509* certificate) {
  if (certificate == nullptr) {
    return;
  }
  mbedtls_x509_crt_free(&certificate->crt);
  delete certificate;
}

EVP_PKEY* X509_get_pubkey(X509* certificate) {
  if (certificate == nullptr) {
    return nullptr;
  }
  // mbedtls_pk_dup does not exist in Mbed TLS 3.6; export the certificate's
  // public key to DER and parse an independent copy.
  const std::size_t bitlen = mbedtls_pk_get_bitlen(&certificate->crt.pk);
  std::vector<unsigned char> encoded(bitlen / 8U + 128U);
  const int encoded_size = mbedtls_pk_write_pubkey_der(
      &certificate->crt.pk, encoded.data(),
      static_cast<size_t>(encoded.size()));
  if (encoded_size <= 0) {
    return nullptr;
  }
  auto* key = new EVP_PKEY();
  mbedtls_pk_init(&key->key);
  unsigned char* cursor = encoded.data();
  if (mbedtls_pk_parse_subpubkey(&cursor, encoded.data() + encoded_size,
                                 &key->key) != 0) {
    mbedtls_pk_free(&key->key);
    delete key;
    return nullptr;
  }
  return key;
}

int CRYPTO_memcmp(const void* a, const void* b, std::size_t size) {
  const auto* left = static_cast<const unsigned char*>(a);
  const auto* right = static_cast<const unsigned char*>(b);
  unsigned char difference = 0;
  for (std::size_t index = 0; index < size; ++index) {
    difference = static_cast<unsigned char>(
        difference | (left[index] ^ right[index]));
  }
  return difference != 0 ? 1 : 0;
}
