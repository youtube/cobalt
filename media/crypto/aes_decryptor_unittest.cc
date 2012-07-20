// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/sys_byteorder.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/mock_filters.h"
#include "media/crypto/aes_decryptor.h"
#include "media/webm/webm_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Gt;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::SaveArg;
using ::testing::StrNe;

namespace media {

// |encrypted_data| is encrypted from |plain_text| using |key|. |key_id| is
// used to distinguish |key|.
struct WebmEncryptedData {
  uint8 plain_text[32];
  int plain_text_size;
  uint8 key_id[32];
  int key_id_size;
  uint8 key[32];
  int key_size;
  uint8 encrypted_data[64];
  int encrypted_data_size;
};

static const char kClearKeySystem[] = "org.w3.clearkey";

// Frames 0 & 1 are encrypted with the same key. Frame 2 is encrypted with a
// different key.
const WebmEncryptedData kWebmEncryptedFrames[] = {
  {
    // plaintext
    "Original data.", 14,
    // key_id
    { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
      0x10, 0x11, 0x12, 0x13,
      }, 20,
    // key
    { 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b,
      0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
      }, 16,
    // encrypted_data
    { 0xfb, 0xe7, 0x1d, 0xbb, 0x4c, 0x23, 0xce, 0xba,
      0xcc, 0xf8, 0xda, 0xc0, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0x99, 0xaa, 0xff, 0xb7,
      0x74, 0x02, 0x4e, 0x1c, 0x75, 0x3d, 0xee, 0xcb,
      0x64, 0xf7,
      }, 34,
  },
  {
    // plaintext
    "Changed Original data.", 22,
    // key_id
    { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
      0x10, 0x11, 0x12, 0x13,
      }, 20,
    // key
    { 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b,
      0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
      }, 16,
    // encrypted_data
    { 0x43, 0xe4, 0x78, 0x7a, 0x43, 0xe1, 0x49, 0xbb,
      0x44, 0x38, 0xdf, 0xfc, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0xec, 0x8e, 0x87, 0x21,
      0xd3, 0xb9, 0x1c, 0x61, 0xf6, 0x5a, 0x60, 0xaa,
      0x07, 0x0e, 0x96, 0xd0, 0x54, 0x5d, 0x35, 0x9a,
      0x4a, 0xd3,
      }, 42,
  },
  {
    // plaintext
    "Original data.", 14,
    // key_id
    { 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
      0x2c, 0x2d, 0x2e, 0x2f, 0x30,
      }, 13,
    // key
    { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
      0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40,
      }, 16,
    // encrypted_data
    { 0xd9, 0x43, 0x30, 0xfd, 0x82, 0x77, 0x62, 0x04,
      0x08, 0xc2, 0x48, 0x89, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x01, 0x48, 0x5e, 0x4a, 0x41,
      0x2a, 0x8b, 0xf4, 0xc6, 0x47, 0x54, 0x90, 0x34,
      0xf4, 0x8b,
      }, 34,
  },
};

static const uint8 kWebmWrongKey[] = {
  0x49, 0x27, 0x6d, 0x20, 0x61, 0x20, 0x77, 0x72,
  0x6f, 0x6e, 0x67, 0x20, 0x6b, 0x65, 0x79, 0x2e
};
static const uint8 kWebmWrongSizedKey[] = { 0x20, 0x20 };

// This is the encrypted data from frame 0 of |kWebmEncryptedFrames| except
// byte 0 is changed from 0xfb to 0xfc. Bytes 0-11 of WebM encrypted data
// contains the HMAC.
static const unsigned char kWebmFrame0HmacDataChanged[] = {
  0xfc, 0xe7, 0x1d, 0xbb, 0x4c, 0x23, 0xce, 0xba,
  0xcc, 0xf8, 0xda, 0xc0, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x99, 0xaa, 0xff, 0xb7,
  0x74, 0x02, 0x4e, 0x1c, 0x75, 0x3d, 0xee, 0xcb,
  0x64, 0xf7
};

// This is the encrypted data from frame 0 of |kWebmEncryptedFrames| except
// byte 12 is changed from 0xff to 0x0f. Bytes 12-19 of WebM encrypted data
// contains the IV.
static const unsigned char kWebmFrame0IvDataChanged[] = {
  0xfb, 0xe7, 0x1d, 0xbb, 0x4c, 0x23, 0xce, 0xba,
  0xcc, 0xf8, 0xda, 0xc0, 0x0f, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x99, 0xaa, 0xff, 0xb7,
  0x74, 0x02, 0x4e, 0x1c, 0x75, 0x3d, 0xee, 0xcb,
  0x64, 0xf7
};

// This is the encrypted data from frame 0 of |kWebmEncryptedFrames| except
// byte 33 is changed from 0xf7 to 0xf8. Bytes 20+ of WebM encrypted data
// contains the encrypted frame.
static const unsigned char kWebmFrame0FrameDataChanged[] = {
  0xfb, 0xe7, 0x1d, 0xbb, 0x4c, 0x23, 0xce, 0xba,
  0xcc, 0xf8, 0xda, 0xc0, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x99, 0xaa, 0xff, 0xb7,
  0x74, 0x02, 0x4e, 0x1c, 0x75, 0x3d, 0xee, 0xcb,
  0x64, 0xf8
};

class AesDecryptorTest : public testing::Test {
 public:
  AesDecryptorTest()
      : decryptor_(&client_),
        decrypt_cb_(base::Bind(&AesDecryptorTest::BufferDecrypted,
                               base::Unretained(this))) {
  }

 protected:
  // Returns a 16 byte CTR counter block. The CTR counter block format is a
  // CTR IV appended with a CTR block counter. |iv| is a CTR IV. |iv_size| is
  // the size of |iv| in bytes.
  static std::string GenerateCounterBlock(const uint8* iv, int iv_size) {
    const int kDecryptionKeySize = 16;
    CHECK_GT(iv_size, 0);
    CHECK_LE(iv_size, kDecryptionKeySize);
    char counter_block_data[kDecryptionKeySize];

    // Set the IV.
    memcpy(counter_block_data, iv, iv_size);

    // Set block counter to all 0's.
    memset(counter_block_data + iv_size, 0, kDecryptionKeySize - iv_size);

    return std::string(counter_block_data, kDecryptionKeySize);
  }

  // Creates a WebM encrypted buffer that the demuxer would pass to the
  // decryptor. |data| is the payload of a WebM encrypted Block. |key_id| is
  // initialization data from the WebM file. Every encrypted Block has
  // an HMAC and IV prepended to an encrypted frame. Current encrypted WebM
  // request for comments specification is here
  // http://wiki.webmproject.org/encryption/webm-encryption-rfc
  scoped_refptr<DecoderBuffer> CreateWebMEncryptedBuffer(const uint8* data,
                                                         int data_size,
                                                         const uint8* key_id,
                                                         int key_id_size) {
    scoped_refptr<DecoderBuffer> encrypted_buffer = DecoderBuffer::CopyFrom(
        data + kWebMHmacSize, data_size - kWebMHmacSize);
    CHECK(encrypted_buffer);

    uint64 network_iv;
    memcpy(&network_iv, data + kWebMHmacSize, sizeof(network_iv));
    const uint64 iv = base::NetToHost64(network_iv);
    std::string webm_iv =
        GenerateCounterBlock(reinterpret_cast<const uint8*>(&iv), sizeof(iv));
    encrypted_buffer->SetDecryptConfig(
        scoped_ptr<DecryptConfig>(new DecryptConfig(
            key_id, key_id_size,
            reinterpret_cast<const uint8*>(webm_iv.data()), webm_iv.size(),
            data, kWebMHmacSize,
            sizeof(iv))));
    return encrypted_buffer;
  }

  void GenerateKeyRequest(const uint8* key_id, int key_id_size) {
    EXPECT_CALL(client_, KeyMessageMock(kClearKeySystem, StrNe(std::string()),
                                        NotNull(), Gt(0), ""))
        .WillOnce(SaveArg<1>(&session_id_string_));
    decryptor_.GenerateKeyRequest(kClearKeySystem, key_id, key_id_size);
  }

  void AddKeyAndExpectToSucceed(const uint8* key_id, int key_id_size,
                                const uint8* key, int key_size) {
    EXPECT_CALL(client_, KeyAdded(kClearKeySystem, session_id_string_));
    decryptor_.AddKey(kClearKeySystem, key, key_size, key_id, key_id_size,
                      session_id_string_);
  }

  void AddKeyAndExpectToFail(const uint8* key_id, int key_id_size,
                             const uint8* key, int key_size) {
    EXPECT_CALL(client_, KeyError(kClearKeySystem, session_id_string_,
                                  Decryptor::kUnknownError, 0));
    decryptor_.AddKey(kClearKeySystem, key, key_size, key_id, key_id_size,
                      session_id_string_);
  }

  MOCK_METHOD2(BufferDecrypted, void(Decryptor::DecryptStatus,
                                     const scoped_refptr<DecoderBuffer>&));

  void DecryptAndExpectToSucceed(const uint8* data, int data_size,
                                 const uint8* plain_text,
                                 int plain_text_size,
                                 const uint8* key_id, int key_id_size) {
    scoped_refptr<DecoderBuffer> encrypted_data =
        CreateWebMEncryptedBuffer(data, data_size, key_id, key_id_size);
    scoped_refptr<DecoderBuffer> decrypted;
    EXPECT_CALL(*this, BufferDecrypted(AesDecryptor::kSuccess, NotNull()))
        .WillOnce(SaveArg<1>(&decrypted));

    decryptor_.Decrypt(encrypted_data, decrypt_cb_);
    ASSERT_TRUE(decrypted);
    ASSERT_EQ(plain_text_size, decrypted->GetDataSize());
    EXPECT_EQ(0, memcmp(plain_text, decrypted->GetData(), plain_text_size));
  }

  void DecryptAndExpectToFail(const uint8* data, int data_size,
                              const uint8* plain_text, int plain_text_size,
                              const uint8* key_id, int key_id_size) {
    scoped_refptr<DecoderBuffer> encrypted_data =
        CreateWebMEncryptedBuffer(data, data_size, key_id, key_id_size);
    EXPECT_CALL(*this, BufferDecrypted(AesDecryptor::kError, IsNull()));
    decryptor_.Decrypt(encrypted_data, decrypt_cb_);
  }

  scoped_refptr<DecoderBuffer> encrypted_data_;
  MockDecryptorClient client_;
  AesDecryptor decryptor_;
  std::string session_id_string_;
  AesDecryptor::DecryptCB decrypt_cb_;
};

TEST_F(AesDecryptorTest, NormalDecryption) {
  const WebmEncryptedData& frame = kWebmEncryptedFrames[0];
  GenerateKeyRequest(frame.key_id, frame.key_id_size);
  AddKeyAndExpectToSucceed(frame.key_id, frame.key_id_size,
                           frame.key, frame.key_size);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed(frame.encrypted_data,
                                                    frame.encrypted_data_size,
                                                    frame.plain_text,
                                                    frame.plain_text_size,
                                                    frame.key_id,
                                                    frame.key_id_size));
}

TEST_F(AesDecryptorTest, WrongKey) {
  const WebmEncryptedData& frame = kWebmEncryptedFrames[0];
  GenerateKeyRequest(frame.key_id, frame.key_id_size);
  AddKeyAndExpectToSucceed(frame.key_id, frame.key_id_size,
                           kWebmWrongKey, arraysize(kWebmWrongKey));
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToFail(frame.encrypted_data,
                                                 frame.encrypted_data_size,
                                                 frame.plain_text,
                                                 frame.plain_text_size,
                                                 frame.key_id,
                                                 frame.key_id_size));
}

TEST_F(AesDecryptorTest, KeyReplacement) {
  const WebmEncryptedData& frame = kWebmEncryptedFrames[0];
  GenerateKeyRequest(frame.key_id, frame.key_id_size);
  AddKeyAndExpectToSucceed(frame.key_id, frame.key_id_size,
                           kWebmWrongKey, arraysize(kWebmWrongKey));
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToFail(frame.encrypted_data,
                                                 frame.encrypted_data_size,
                                                 frame.plain_text,
                                                 frame.plain_text_size,
                                                 frame.key_id,
                                                 frame.key_id_size));
  AddKeyAndExpectToSucceed(frame.key_id, frame.key_id_size,
                           frame.key, frame.key_size);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed(frame.encrypted_data,
                                                    frame.encrypted_data_size,
                                                    frame.plain_text,
                                                    frame.plain_text_size,
                                                    frame.key_id,
                                                    frame.key_id_size));
}

TEST_F(AesDecryptorTest, WrongSizedKey) {
  const WebmEncryptedData& frame = kWebmEncryptedFrames[0];
  GenerateKeyRequest(frame.key_id, frame.key_id_size);
  AddKeyAndExpectToFail(frame.key_id, frame.key_id_size,
                        kWebmWrongSizedKey, arraysize(kWebmWrongSizedKey));
}

TEST_F(AesDecryptorTest, MultipleKeysAndFrames) {
  const WebmEncryptedData& frame = kWebmEncryptedFrames[0];
  GenerateKeyRequest(frame.key_id, frame.key_id_size);
  AddKeyAndExpectToSucceed(frame.key_id, frame.key_id_size,
                           frame.key, frame.key_size);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed(frame.encrypted_data,
                                                    frame.encrypted_data_size,
                                                    frame.plain_text,
                                                    frame.plain_text_size,
                                                    frame.key_id,
                                                    frame.key_id_size));

  const WebmEncryptedData& frame2 = kWebmEncryptedFrames[2];
  GenerateKeyRequest(frame2.key_id, frame2.key_id_size);
  AddKeyAndExpectToSucceed(frame2.key_id, frame2.key_id_size,
                           frame2.key, frame2.key_size);

  const WebmEncryptedData& frame1 = kWebmEncryptedFrames[1];
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed(frame1.encrypted_data,
                                                    frame1.encrypted_data_size,
                                                    frame1.plain_text,
                                                    frame1.plain_text_size,
                                                    frame1.key_id,
                                                    frame1.key_id_size));

  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed(frame2.encrypted_data,
                                                    frame2.encrypted_data_size,
                                                    frame2.plain_text,
                                                    frame2.plain_text_size,
                                                    frame2.key_id,
                                                    frame2.key_id_size));
}

TEST_F(AesDecryptorTest, HmacCheckFailure) {
  const WebmEncryptedData& frame = kWebmEncryptedFrames[0];
  GenerateKeyRequest(frame.key_id, frame.key_id_size);
  AddKeyAndExpectToSucceed(frame.key_id, frame.key_id_size,
                           frame.key, frame.key_size);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToFail(kWebmFrame0HmacDataChanged,
                                                 frame.encrypted_data_size,
                                                 frame.plain_text,
                                                 frame.plain_text_size,
                                                 frame.key_id,
                                                 frame.key_id_size));
}

TEST_F(AesDecryptorTest, IvCheckFailure) {
  const WebmEncryptedData& frame = kWebmEncryptedFrames[0];
  GenerateKeyRequest(frame.key_id, frame.key_id_size);
  AddKeyAndExpectToSucceed(frame.key_id, frame.key_id_size,
                           frame.key, frame.key_size);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToFail(kWebmFrame0IvDataChanged,
                                                 frame.encrypted_data_size,
                                                 frame.plain_text,
                                                 frame.plain_text_size,
                                                 frame.key_id,
                                                 frame.key_id_size));
}

TEST_F(AesDecryptorTest, DataCheckFailure) {
  const WebmEncryptedData& frame = kWebmEncryptedFrames[0];
  GenerateKeyRequest(frame.key_id, frame.key_id_size);
  AddKeyAndExpectToSucceed(frame.key_id, frame.key_id_size,
                           frame.key, frame.key_size);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToFail(kWebmFrame0FrameDataChanged,
                                                 frame.encrypted_data_size,
                                                 frame.plain_text,
                                                 frame.plain_text_size,
                                                 frame.key_id,
                                                 frame.key_id_size));
}

}  // namespace media
