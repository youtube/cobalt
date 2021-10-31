// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/subtlecrypto/crypto_impl.h"

#include <openssl/cipher.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

#include <algorithm>
#include <map>

#include "base/logging.h"

namespace cobalt {
namespace subtlecrypto {

namespace {

using EVP_Func = const EVP_MD *(*)(void);

template <typename TCtx, size_t HashLen, EVP_Func evp_func,
          int (*fInit)(TCtx *), int (*fUpdate)(TCtx *, const void *, size_t),
          int (*fFinal)(unsigned char *, TCtx *)>
class HashImpl : public Hash {
  TCtx ctx;

 public:
  HashImpl() { fInit(&ctx); }
  void Update(const ByteVector &data) override {
    if (!data.empty()) {
      fUpdate(&ctx, static_cast<const unsigned char *>(data.data()),
              data.size());
    }
  }
  ByteVector Finish() override {
    ByteVector out(HashLen);
    fFinal(out.data(), &ctx);
    return out;
  }
  ByteVector CalculateHMAC(const ByteVector &data,
                           const ByteVector &key) override {
    ByteVector ret(HashLen);
    unsigned int ret_len = static_cast<unsigned int>(ret.size());
    auto result = HMAC(evp_func(), key.data(), key.size(), data.data(),
                       data.size(), ret.data(), &ret_len);
    if (ret_len == HashLen && result) {
      return ret;
    }
    return {};
  }
};

using Sha1Hash = HashImpl<SHA_CTX, SHA_DIGEST_LENGTH, EVP_sha1, SHA1_Init,
                          SHA1_Update, SHA1_Final>;
using Sha256Hash = HashImpl<SHA256_CTX, SHA256_DIGEST_LENGTH, EVP_sha256,
                            SHA256_Init, SHA256_Update, SHA256_Final>;
using Sha384Hash = HashImpl<SHA512_CTX, SHA384_DIGEST_LENGTH, EVP_sha384,
                            SHA384_Init, SHA384_Update, SHA384_Final>;
using Sha512Hash = HashImpl<SHA512_CTX, SHA512_DIGEST_LENGTH, EVP_sha512,
                            SHA512_Init, SHA512_Update, SHA512_Final>;
using HashPtr = std::unique_ptr<Hash>;

}  // namespace

std::unique_ptr<Hash> Hash::CreateByName(const std::string &name) {
  std::string tmp_name(name);
  transform(tmp_name.begin(), tmp_name.end(), tmp_name.begin(), ::toupper);
  if (tmp_name == "SHA-1") return std::make_unique<Sha1Hash>();
  if (tmp_name == "SHA-256") return std::make_unique<Sha256Hash>();
  if (tmp_name == "SHA-384") return std::make_unique<Sha384Hash>();
  if (tmp_name == "SHA-512") return std::make_unique<Sha512Hash>();
  return nullptr;
}

ByteVector CalculateAES_CTR(const ByteVector &data, const ByteVector &key,
                            const ByteVector &iv) {
  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  DCHECK(ctx);
  auto error = [&ctx](const char *msg) -> ByteVector {
    DLOG(ERROR) << msg;
    EVP_CIPHER_CTX_free(ctx);
    return {};
  };

  if (iv.size() != 16) {
    return error("Invalid initialization vector size, AES requires 128-bit IV");
  }
  const char *algo = nullptr;
  switch (key.size()) {
    case 16:
      algo = "aes-128-ctr";
      break;
    case 24:
      algo = "aes-192-ctr";
      break;
    case 32:
      algo = "aes-256-ctr";
      break;
  }
  if (!algo) {
    return error("Invalid key size, 128, 192 or 256 bits required");
  }

  if (!EVP_EncryptInit_ex(ctx, EVP_get_cipherbyname(algo), NULL, key.data(),
                          iv.data())) {
    return error("EVP_EncryptInit_ex failed");
  }
  int buf_len = 0, out_len = 0;
  ByteVector out(data.size() + 1);
  if (!EVP_EncryptUpdate(ctx, out.data(), &buf_len, data.data(),
                         static_cast<int>(data.size()))) {
    return error("EncryptUpdate failed");
  }
  out_len += buf_len;
  if (!EVP_EncryptFinal_ex(ctx, &out[buf_len], &buf_len)) {
    return error("EVP_EncryptFinal_ex failed");
  }
  out_len += buf_len;
  out.resize(out_len);
  EVP_CIPHER_CTX_free(ctx);
  return out;
}

}  // namespace subtlecrypto
}  // namespace cobalt
