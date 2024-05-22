// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This code implements SPAKE2, a variant of EKE:
//  http://www.di.ens.fr/~pointche/pub.php?reference=AbPo04

#include "crypto/p224_spake.h"

#include <string.h>

#include <algorithm>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "crypto/random.h"
#include "crypto/secure_util.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/obj.h"

namespace {

// The following two points (M and N in the protocol) are verifiable random
// points on the curve and can be generated with the following code:

// #include <stdint.h>
// #include <stdio.h>
// #include <string.h>
//
// #include <openssl/ec.h>
// #include <openssl/obj_mac.h>
// #include <openssl/sha.h>
//
// // Silence a presubmit.
// #define PRINTF printf
//
// static const char kSeed1[] = "P224 point generation seed (M)";
// static const char kSeed2[] = "P224 point generation seed (N)";
//
// void find_seed(const char* seed) {
//   SHA256_CTX sha256;
//   uint8_t digest[SHA256_DIGEST_LENGTH];
//
//   SHA256_Init(&sha256);
//   SHA256_Update(&sha256, seed, strlen(seed));
//   SHA256_Final(digest, &sha256);
//
//   BIGNUM x, y;
//   EC_GROUP* p224 = EC_GROUP_new_by_curve_name(NID_secp224r1);
//   EC_POINT* p = EC_POINT_new(p224);
//
//   for (unsigned i = 0;; i++) {
//     BN_init(&x);
//     BN_bin2bn(digest, 28, &x);
//
//     if (EC_POINT_set_compressed_coordinates_GFp(
//             p224, p, &x, digest[28] & 1, NULL)) {
//       BN_init(&y);
//       EC_POINT_get_affine_coordinates_GFp(p224, p, &x, &y, NULL);
//       char* x_str = BN_bn2hex(&x);
//       char* y_str = BN_bn2hex(&y);
//       PRINTF("Found after %u iterations:\n%s\n%s\n", i, x_str, y_str);
//       OPENSSL_free(x_str);
//       OPENSSL_free(y_str);
//       BN_free(&x);
//       BN_free(&y);
//       break;
//     }
//
//     SHA256_Init(&sha256);
//     SHA256_Update(&sha256, digest, sizeof(digest));
//     SHA256_Final(digest, &sha256);
//
//     BN_free(&x);
//   }
//
//   EC_POINT_free(p);
//   EC_GROUP_free(p224);
// }
//
// int main() {
//   find_seed(kSeed1);
//   find_seed(kSeed2);
//   return 0;
// }

const uint8_t kM_X962[1 + 28 + 28] = {
    0x04, 0x4d, 0x48, 0xc8, 0xea, 0x8d, 0x23, 0x39, 0x2e, 0x07, 0xe8, 0x51,
    0xfa, 0x6a, 0xa8, 0x20, 0x48, 0x09, 0x4e, 0x05, 0x13, 0x72, 0x49, 0x9c,
    0x6f, 0xba, 0x62, 0xa7, 0x4b, 0x6c, 0x18, 0x5c, 0xab, 0xd5, 0x2e, 0x2e,
    0x8a, 0x9e, 0x2d, 0x21, 0xb0, 0xec, 0x4e, 0xe1, 0x41, 0x21, 0x1f, 0xe2,
    0x9d, 0x64, 0xea, 0x4d, 0x04, 0x46, 0x3a, 0xe8, 0x33,
};

const uint8_t kN_X962[1 + 28 + 28] = {
    0x04, 0x0b, 0x1c, 0xfc, 0x6a, 0x40, 0x7c, 0xdc, 0xb1, 0x5d, 0xc1, 0x70,
    0x4c, 0xd1, 0x3e, 0xda, 0xab, 0x8f, 0xde, 0xff, 0x8c, 0xfb, 0xfb, 0x50,
    0xd2, 0xc8, 0x1d, 0xe2, 0xc2, 0x3e, 0x14, 0xf6, 0x29, 0x96, 0x08, 0x09,
    0x07, 0xb5, 0x6d, 0xd2, 0x82, 0x07, 0x1a, 0xa7, 0xa1, 0x21, 0xc3, 0x99,
    0x34, 0xbc, 0x30, 0xda, 0x5b, 0xcb, 0xc6, 0xa3, 0xcc,
};

// ToBignum returns |big_endian_bytes| interpreted as a big-endian number.
bssl::UniquePtr<BIGNUM> ToBignum(base::span<const uint8_t> big_endian_bytes) {
  bssl::UniquePtr<BIGNUM> bn(BN_new());
  CHECK(BN_bin2bn(big_endian_bytes.data(), big_endian_bytes.size(), bn.get()));
  return bn;
}

// GetPoint decodes and returns the given X.962-encoded point. It will crash if
// |x962| is not a valid P-224 point.
bssl::UniquePtr<EC_POINT> GetPoint(
    const EC_GROUP* p224,
    base::span<const uint8_t, 1 + 28 + 28> x962) {
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(p224));
  CHECK(EC_POINT_oct2point(p224, point.get(), x962.data(), x962.size(),
                           /*ctx=*/nullptr));
  return point;
}

// GetMask returns (M|N)**pw, where the choice of M or N is controlled by
// |use_m|.
bssl::UniquePtr<EC_POINT> GetMask(const EC_GROUP* p224,
                                  bool use_m,
                                  base::span<const uint8_t> pw) {
  bssl::UniquePtr<EC_POINT> MN(GetPoint(p224, use_m ? kM_X962 : kN_X962));
  bssl::UniquePtr<EC_POINT> MNpw(EC_POINT_new(p224));
  bssl::UniquePtr<BIGNUM> pw_bn(ToBignum(pw));
  CHECK(EC_POINT_mul(p224, MNpw.get(), nullptr, MN.get(), pw_bn.get(),
                     /*ctx=*/nullptr));
  return MNpw;
}

// ToMessage serialises |in| as a 56-byte string that contains the big-endian
// representations of x and y, or is all zeros if |in| is infinity.
std::string ToMessage(const EC_GROUP* p224, const EC_POINT* in) {
  if (EC_POINT_is_at_infinity(p224, in)) {
    return std::string(28 + 28, 0);
  }

  uint8_t x962[1 + 28 + 28];
  CHECK(EC_POINT_point2oct(p224, in, POINT_CONVERSION_UNCOMPRESSED, x962,
                           sizeof(x962), /*ctx=*/nullptr) == sizeof(x962));
  return std::string(reinterpret_cast<const char*>(&x962[1]), sizeof(x962) - 1);
}

// FromMessage converts a message, as generated by |ToMessage|, into a point. It
// returns |nullptr| if the input is invalid or not on the curve.
bssl::UniquePtr<EC_POINT> FromMessage(const EC_GROUP* p224,
                                      base::StringPiece in) {
  if (in.size() != 56) {
    return nullptr;
  }

  uint8_t x962[1 + 56];
  x962[0] = 4;
  memcpy(&x962[1], in.data(), sizeof(x962) - 1);

  bssl::UniquePtr<EC_POINT> ret(EC_POINT_new(p224));
  if (!EC_POINT_oct2point(p224, ret.get(), x962, sizeof(x962),
                          /*ctx=*/nullptr)) {
    return nullptr;
  }

  return ret;
}

}  // anonymous namespace

namespace crypto {

P224EncryptedKeyExchange::P224EncryptedKeyExchange(PeerType peer_type,
                                                   base::StringPiece password)
    : state_(kStateInitial), is_server_(peer_type == kPeerTypeServer) {
  memset(&x_, 0, sizeof(x_));
  memset(&expected_authenticator_, 0, sizeof(expected_authenticator_));

  // x_ is a random scalar.
  RandBytes(x_, sizeof(x_));

  // Calculate |password| hash to get SPAKE password value.
  SHA256HashString(std::string(password.data(), password.length()),
                   pw_, sizeof(pw_));

  Init();
}

void P224EncryptedKeyExchange::Init() {
  // X = g**x_
  bssl::UniquePtr<EC_GROUP> p224(EC_GROUP_new_by_curve_name(NID_secp224r1));
  bssl::UniquePtr<EC_POINT> X(EC_POINT_new(p224.get()));
  bssl::UniquePtr<BIGNUM> x_bn(ToBignum(x_));
  // x_bn may be >= the order, but |EC_POINT_mul| handles that. It doesn't do so
  // in constant-time, but the these values are locally generated and so this
  // occurs with negligible probability. (Same with |pw_|, just below.)
  CHECK(EC_POINT_mul(p224.get(), X.get(), x_bn.get(), nullptr, nullptr,
                     /*ctx=*/nullptr));

  // The client masks the Diffie-Hellman value, X, by adding M**pw and the
  // server uses N**pw.
  bssl::UniquePtr<EC_POINT> MNpw(GetMask(p224.get(), !is_server_, pw_));

  // X* = X + (N|M)**pw
  bssl::UniquePtr<EC_POINT> Xstar(EC_POINT_new(p224.get()));
  CHECK(EC_POINT_add(p224.get(), Xstar.get(), X.get(), MNpw.get(),
                     /*ctx=*/nullptr));

  next_message_ = ToMessage(p224.get(), Xstar.get());
}

const std::string& P224EncryptedKeyExchange::GetNextMessage() {
  if (state_ == kStateInitial) {
    state_ = kStateRecvDH;
    return next_message_;
  } else if (state_ == kStateSendHash) {
    state_ = kStateRecvHash;
    return next_message_;
  }

  LOG(FATAL) << "P224EncryptedKeyExchange::GetNextMessage called in"
                " bad state " << state_;
  next_message_ = "";
  return next_message_;
}

P224EncryptedKeyExchange::Result P224EncryptedKeyExchange::ProcessMessage(
    base::StringPiece message) {
  if (state_ == kStateRecvHash) {
    // This is the final state of the protocol: we are reading the peer's
    // authentication hash and checking that it matches the one that we expect.
    if (message.size() != sizeof(expected_authenticator_)) {
      error_ = "peer's hash had an incorrect size";
      return kResultFailed;
    }
    if (!SecureMemEqual(message.data(), expected_authenticator_,
                        message.size())) {
      error_ = "peer's hash had incorrect value";
      return kResultFailed;
    }
    state_ = kStateDone;
    return kResultSuccess;
  }

  if (state_ != kStateRecvDH) {
    LOG(FATAL) << "P224EncryptedKeyExchange::ProcessMessage called in"
                  " bad state " << state_;
    error_ = "internal error";
    return kResultFailed;
  }

  bssl::UniquePtr<EC_GROUP> p224(EC_GROUP_new_by_curve_name(NID_secp224r1));

  // Y* is the other party's masked, Diffie-Hellman value.
  bssl::UniquePtr<EC_POINT> Ystar(FromMessage(p224.get(), message));
  if (!Ystar) {
    error_ = "failed to parse peer's masked Diffie-Hellman value";
    return kResultFailed;
  }

  // We calculate the mask value: (N|M)**pw
  bssl::UniquePtr<EC_POINT> MNpw(GetMask(p224.get(), is_server_, pw_));
  // Y = Y* - (N|M)**pw
  CHECK(EC_POINT_invert(p224.get(), MNpw.get(), /*ctx=*/nullptr));
  bssl::UniquePtr<EC_POINT> Y(EC_POINT_new(p224.get()));
  CHECK(EC_POINT_add(p224.get(), Y.get(), Ystar.get(), MNpw.get(),
                     /*ctx=*/nullptr));

  // K = Y**x_
  bssl::UniquePtr<EC_POINT> K(EC_POINT_new(p224.get()));
  bssl::UniquePtr<BIGNUM> x_bn(ToBignum(x_));
  CHECK(EC_POINT_mul(p224.get(), K.get(), nullptr, Y.get(), x_bn.get(),
                     /*ctx=*/nullptr));

  // If everything worked out, then K is the same for both parties.
  key_ = ToMessage(p224.get(), K.get());

  std::string client_masked_dh, server_masked_dh;
  if (is_server_) {
    client_masked_dh = std::string(message);
    server_masked_dh = next_message_;
  } else {
    client_masked_dh = next_message_;
    server_masked_dh = std::string(message);
  }

  // Now we calculate the hashes that each side will use to prove to the other
  // that they derived the correct value for K.
  uint8_t client_hash[kSHA256Length], server_hash[kSHA256Length];
  CalculateHash(kPeerTypeClient, client_masked_dh, server_masked_dh, key_,
                client_hash);
  CalculateHash(kPeerTypeServer, client_masked_dh, server_masked_dh, key_,
                server_hash);

  const uint8_t* my_hash = is_server_ ? server_hash : client_hash;
  const uint8_t* their_hash = is_server_ ? client_hash : server_hash;

  next_message_ =
      std::string(reinterpret_cast<const char*>(my_hash), kSHA256Length);
  memcpy(expected_authenticator_, their_hash, kSHA256Length);
  state_ = kStateSendHash;
  return kResultPending;
}

void P224EncryptedKeyExchange::CalculateHash(
    PeerType peer_type,
    const std::string& client_masked_dh,
    const std::string& server_masked_dh,
    const std::string& k,
    uint8_t* out_digest) {
  std::string hash_contents;

  if (peer_type == kPeerTypeServer) {
    hash_contents = "server";
  } else {
    hash_contents = "client";
  }

  hash_contents += client_masked_dh;
  hash_contents += server_masked_dh;
  hash_contents +=
      std::string(reinterpret_cast<const char *>(pw_), sizeof(pw_));
  hash_contents += k;

  SHA256HashString(hash_contents, out_digest, kSHA256Length);
}

const std::string& P224EncryptedKeyExchange::error() const {
  return error_;
}

const std::string& P224EncryptedKeyExchange::GetKey() const {
  DCHECK_EQ(state_, kStateDone);
  return GetUnverifiedKey();
}

const std::string& P224EncryptedKeyExchange::GetUnverifiedKey() const {
  // Key is already final when state is kStateSendHash. Subsequent states are
  // used only for verification of the key. Some users may combine verification
  // with sending verifiable data instead of |expected_authenticator_|.
  DCHECK_GE(state_, kStateSendHash);
  return key_;
}

void P224EncryptedKeyExchange::SetXForTesting(const std::string& x) {
  memset(&x_, 0, sizeof(x_));
  memcpy(&x_, x.data(), std::min(x.size(), sizeof(x_)));
  Init();
}

}  // namespace crypto
