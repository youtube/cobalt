// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
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

MATCHER_P2(ArrayEq, array, size, "") {
  return !memcmp(arg, array, size);
}

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
// different key. Frame 3 is unencrypted.
const WebmEncryptedData kWebmEncryptedFrames[] = {
  {
    // plaintext
    "Original data.", 14,
    // key_id
    { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
      0x10, 0x11, 0x12, 0x13
      }, 20,
    // key
    { 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b,
      0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23
      }, 16,
    // encrypted_data
    { 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xf0, 0xd1, 0x12, 0xd5, 0x24, 0x81, 0x96,
      0x55, 0x1b, 0x68, 0x9f, 0x38, 0x91, 0x85
      }, 23
  }, {
    // plaintext
    "Changed Original data.", 22,
    // key_id
    { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
      0x10, 0x11, 0x12, 0x13
      }, 20,
    // key
    { 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b,
      0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23
      }, 16,
    // encrypted_data
    { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x57, 0x66, 0xf4, 0x12, 0x1a, 0xed, 0xb5,
      0x79, 0x1c, 0x8e, 0x25, 0xd7, 0x17, 0xe7, 0x5e,
      0x16, 0xe3, 0x40, 0x08, 0x27, 0x11, 0xe9
      }, 31
  }, {
    // plaintext
    "Original data.", 14,
    // key_id
    { 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
      0x2c, 0x2d, 0x2e, 0x2f, 0x30
      }, 13,
    // key
    { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
      0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40
      }, 16,
    // encrypted_data
    { 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x9c, 0x71, 0x26, 0x57, 0x3e, 0x25, 0x37,
      0xf7, 0x31, 0x81, 0x19, 0x64, 0xce, 0xbc
      }, 23
  }, {
    // plaintext
    "Changed Original data.", 22,
    // key_id
    { 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
      0x2c, 0x2d, 0x2e, 0x2f, 0x30
      }, 13,
    // key
    { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
      0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40
      }, 16,
    // encrypted_data
    { 0x00, 0x43, 0x68, 0x61, 0x6e, 0x67, 0x65, 0x64,
      0x20, 0x4f, 0x72, 0x69, 0x67, 0x69, 0x6e, 0x61,
      0x6c, 0x20, 0x64, 0x61, 0x74, 0x61, 0x2e
      }, 23
  }
};

static const uint8 kWebmWrongSizedKey[] = { 0x20, 0x20 };

static const uint8 kSubsampleOriginalData[] = "Original subsample data.";
static const int kSubsampleOriginalDataSize = 24;

static const uint8 kSubsampleKeyId[] = { 0x00, 0x01, 0x02, 0x03 };

static const uint8 kSubsampleKey[] = {
  0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
  0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13
};

static const uint8 kSubsampleIv[] = {
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8 kSubsampleData[] = {
  0x4f, 0x72, 0x09, 0x16, 0x09, 0xe6, 0x79, 0xad,
  0x70, 0x73, 0x75, 0x62, 0x09, 0xbb, 0x83, 0x1d,
  0x4d, 0x08, 0xd7, 0x78, 0xa4, 0xa7, 0xf1, 0x2e
};

static const uint8 kPaddedSubsampleData[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x4f, 0x72, 0x09, 0x16, 0x09, 0xe6, 0x79, 0xad,
  0x70, 0x73, 0x75, 0x62, 0x09, 0xbb, 0x83, 0x1d,
  0x4d, 0x08, 0xd7, 0x78, 0xa4, 0xa7, 0xf1, 0x2e
};

// Encrypted with kSubsampleKey and kSubsampleIv but without subsamples.
static const uint8 kNoSubsampleData[] = {
  0x2f, 0x03, 0x09, 0xef, 0x71, 0xaf, 0x31, 0x16,
  0xfa, 0x9d, 0x18, 0x43, 0x1e, 0x96, 0x71, 0xb5,
  0xbf, 0xf5, 0x30, 0x53, 0x9a, 0x20, 0xdf, 0x95
};

static const SubsampleEntry kSubsampleEntries[] = {
  { 2, 7 },
  { 3, 11 },
  { 1, 0 }
};

// Generates a 16 byte CTR counter block. The CTR counter block format is a
// CTR IV appended with a CTR block counter. |iv| is an 8 byte CTR IV.
// |iv_size| is the size of |iv| in btyes. Returns a string of
// kDecryptionKeySize bytes.
static std::string GenerateCounterBlock(const uint8* iv, int iv_size) {
  CHECK_GT(iv_size, 0);
  CHECK_LE(iv_size, DecryptConfig::kDecryptionKeySize);

  std::string counter_block(reinterpret_cast<const char*>(iv), iv_size);
  counter_block.append(DecryptConfig::kDecryptionKeySize - iv_size, 0);
  return counter_block;
}

// Creates a WebM encrypted buffer that the demuxer would pass to the
// decryptor. |data| is the payload of a WebM encrypted Block. |key_id| is
// initialization data from the WebM file. Every encrypted Block has
// a signal byte prepended to a frame. If the frame is encrypted then an IV is
// prepended to the Block. Current encrypted WebM request for comments
// specification is here
// http://wiki.webmproject.org/encryption/webm-encryption-rfc
static scoped_refptr<DecoderBuffer> CreateWebMEncryptedBuffer(
    const uint8* data, int data_size,
    const uint8* key_id, int key_id_size) {
  scoped_refptr<DecoderBuffer> encrypted_buffer = DecoderBuffer::CopyFrom(
      data, data_size);
  CHECK(encrypted_buffer);
  DCHECK_EQ(kWebMSignalByteSize, 1);

  uint8 signal_byte = data[0];
  int data_offset = kWebMSignalByteSize;

  // Setting the DecryptConfig object of the buffer while leaving the
  // initialization vector empty will tell the decryptor that the frame is
  // unencrypted.
  std::string counter_block_str;

  if (signal_byte & kWebMFlagEncryptedFrame) {
    counter_block_str = GenerateCounterBlock(data + data_offset, kWebMIvSize);
    data_offset += kWebMIvSize;
  }

  encrypted_buffer->SetDecryptConfig(
      scoped_ptr<DecryptConfig>(new DecryptConfig(
          std::string(reinterpret_cast<const char*>(key_id), key_id_size),
          counter_block_str,
          data_offset,
          std::vector<SubsampleEntry>())));
  return encrypted_buffer;
}

static scoped_refptr<DecoderBuffer> CreateSubsampleEncryptedBuffer(
    const uint8* data, int data_size,
    const uint8* key_id, int key_id_size,
    const uint8* iv, int iv_size,
    int data_offset,
    const std::vector<SubsampleEntry>& subsample_entries) {
  scoped_refptr<DecoderBuffer> encrypted_buffer =
      DecoderBuffer::CopyFrom(data, data_size);
  CHECK(encrypted_buffer);
  encrypted_buffer->SetDecryptConfig(
      scoped_ptr<DecryptConfig>(new DecryptConfig(
          std::string(reinterpret_cast<const char*>(key_id), key_id_size),
          std::string(reinterpret_cast<const char*>(iv), iv_size),
          data_offset,
          subsample_entries)));
  return encrypted_buffer;
}

class AesDecryptorTest : public testing::Test {
 public:
  AesDecryptorTest()
      : decryptor_(&client_),
        decrypt_cb_(base::Bind(&AesDecryptorTest::BufferDecrypted,
                               base::Unretained(this))),
        subsample_entries_(kSubsampleEntries,
                           kSubsampleEntries + arraysize(kSubsampleEntries)) {
  }

 protected:
  void GenerateKeyRequest(const uint8* key_id, int key_id_size) {
    EXPECT_CALL(client_, KeyMessageMock(kClearKeySystem, StrNe(""),
                                        ArrayEq(key_id, key_id_size),
                                        key_id_size, ""))
        .WillOnce(SaveArg<1>(&session_id_string_));
    EXPECT_TRUE(decryptor_.GenerateKeyRequest(kClearKeySystem, "",
                                              key_id, key_id_size));
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

  MOCK_METHOD2(BufferDecrypted, void(Decryptor::Status,
                                     const scoped_refptr<DecoderBuffer>&));

  void DecryptAndExpectToSucceed(const scoped_refptr<DecoderBuffer>& encrypted,
                                 const uint8* plain_text, int plain_text_size) {
    scoped_refptr<DecoderBuffer> decrypted;
    EXPECT_CALL(*this, BufferDecrypted(AesDecryptor::kSuccess, NotNull()))
        .WillOnce(SaveArg<1>(&decrypted));

    decryptor_.Decrypt(Decryptor::kVideo, encrypted, decrypt_cb_);
    ASSERT_TRUE(decrypted);
    ASSERT_EQ(plain_text_size, decrypted->GetDataSize());
    EXPECT_EQ(0, memcmp(plain_text, decrypted->GetData(), plain_text_size));
  }

  void DecryptAndExpectDataMismatch(
        const scoped_refptr<DecoderBuffer>& encrypted,
        const uint8* plain_text, int plain_text_size) {
    scoped_refptr<DecoderBuffer> decrypted;
    EXPECT_CALL(*this, BufferDecrypted(AesDecryptor::kSuccess, NotNull()))
        .WillOnce(SaveArg<1>(&decrypted));

    decryptor_.Decrypt(Decryptor::kVideo, encrypted, decrypt_cb_);
    ASSERT_TRUE(decrypted);
    ASSERT_EQ(plain_text_size, decrypted->GetDataSize());
    EXPECT_NE(0, memcmp(plain_text, decrypted->GetData(), plain_text_size));
  }

  void DecryptAndExpectSizeDataMismatch(
        const scoped_refptr<DecoderBuffer>& encrypted,
        const uint8* plain_text, int plain_text_size) {
    scoped_refptr<DecoderBuffer> decrypted;
    EXPECT_CALL(*this, BufferDecrypted(AesDecryptor::kSuccess, NotNull()))
        .WillOnce(SaveArg<1>(&decrypted));

    decryptor_.Decrypt(Decryptor::kVideo, encrypted, decrypt_cb_);
    ASSERT_TRUE(decrypted);
    EXPECT_NE(plain_text_size, decrypted->GetDataSize());
    EXPECT_NE(0, memcmp(plain_text, decrypted->GetData(), plain_text_size));
  }

  void DecryptAndExpectToFail(const scoped_refptr<DecoderBuffer>& encrypted) {
    EXPECT_CALL(*this, BufferDecrypted(AesDecryptor::kError, IsNull()));
    decryptor_.Decrypt(Decryptor::kVideo, encrypted, decrypt_cb_);
  }

  MockDecryptorClient client_;
  AesDecryptor decryptor_;
  std::string session_id_string_;
  AesDecryptor::DecryptCB decrypt_cb_;
  std::vector<SubsampleEntry> subsample_entries_;
};

TEST_F(AesDecryptorTest, GenerateKeyRequestWithNullInitData) {
  EXPECT_FALSE(decryptor_.GenerateKeyRequest(kClearKeySystem, "", NULL, 0));
}

TEST_F(AesDecryptorTest, NormalWebMDecryption) {
  const WebmEncryptedData& frame = kWebmEncryptedFrames[0];
  GenerateKeyRequest(frame.key_id, frame.key_id_size);
  AddKeyAndExpectToSucceed(frame.key_id, frame.key_id_size,
                           frame.key, frame.key_size);
  scoped_refptr<DecoderBuffer> encrypted_data =
      CreateWebMEncryptedBuffer(frame.encrypted_data,
                                frame.encrypted_data_size,
                                frame.key_id, frame.key_id_size);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed(encrypted_data,
                                                    frame.plain_text,
                                                    frame.plain_text_size));
}

TEST_F(AesDecryptorTest, UnencryptedFrameWebMDecryption) {
  const WebmEncryptedData& frame = kWebmEncryptedFrames[3];
  GenerateKeyRequest(frame.key_id, frame.key_id_size);
  AddKeyAndExpectToSucceed(frame.key_id, frame.key_id_size,
                           frame.key, frame.key_size);
  scoped_refptr<DecoderBuffer> encrypted_data =
      CreateWebMEncryptedBuffer(frame.encrypted_data,
                                frame.encrypted_data_size,
                                frame.key_id, frame.key_id_size);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed(encrypted_data,
                                                    frame.plain_text,
                                                    frame.plain_text_size));
}

TEST_F(AesDecryptorTest, WrongKey) {
  const WebmEncryptedData& frame = kWebmEncryptedFrames[0];
  GenerateKeyRequest(frame.key_id, frame.key_id_size);

  // Change the first byte of the key.
  std::vector<uint8> wrong_key(frame.key, frame.key + frame.key_size);
  wrong_key[0]++;

  AddKeyAndExpectToSucceed(frame.key_id, frame.key_id_size,
                           &wrong_key[0], frame.key_size);
  scoped_refptr<DecoderBuffer> encrypted_data =
      CreateWebMEncryptedBuffer(frame.encrypted_data,
                                frame.encrypted_data_size,
                                frame.key_id, frame.key_id_size);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectDataMismatch(encrypted_data,
                                                       frame.plain_text,
                                                       frame.plain_text_size));
}

TEST_F(AesDecryptorTest, NoKey) {
  const WebmEncryptedData& frame = kWebmEncryptedFrames[0];
  GenerateKeyRequest(frame.key_id, frame.key_id_size);

  scoped_refptr<DecoderBuffer> encrypted_data =
      CreateWebMEncryptedBuffer(frame.encrypted_data, frame.encrypted_data_size,
                                frame.key_id, frame.key_id_size);
  EXPECT_CALL(*this, BufferDecrypted(AesDecryptor::kNoKey, IsNull()));
  decryptor_.Decrypt(Decryptor::kVideo, encrypted_data, decrypt_cb_);
}

TEST_F(AesDecryptorTest, KeyReplacement) {
  const WebmEncryptedData& frame = kWebmEncryptedFrames[0];
  GenerateKeyRequest(frame.key_id, frame.key_id_size);

  // Change the first byte of the key.
  std::vector<uint8> wrong_key(frame.key, frame.key + frame.key_size);
  wrong_key[0]++;

  AddKeyAndExpectToSucceed(frame.key_id, frame.key_id_size,
                           &wrong_key[0], frame.key_size);
  scoped_refptr<DecoderBuffer> encrypted_data =
      CreateWebMEncryptedBuffer(frame.encrypted_data,
                                frame.encrypted_data_size,
                                frame.key_id, frame.key_id_size);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectDataMismatch(encrypted_data,
                                                       frame.plain_text,
                                                       frame.plain_text_size));
  AddKeyAndExpectToSucceed(frame.key_id, frame.key_id_size,
                           frame.key, frame.key_size);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed(encrypted_data,
                                                    frame.plain_text,
                                                    frame.plain_text_size));
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
  scoped_refptr<DecoderBuffer> encrypted_data =
      CreateWebMEncryptedBuffer(frame.encrypted_data,
                                frame.encrypted_data_size,
                                frame.key_id, frame.key_id_size);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed(encrypted_data,
                                                    frame.plain_text,
                                                    frame.plain_text_size));

  const WebmEncryptedData& frame2 = kWebmEncryptedFrames[2];
  GenerateKeyRequest(frame2.key_id, frame2.key_id_size);
  AddKeyAndExpectToSucceed(frame2.key_id, frame2.key_id_size,
                           frame2.key, frame2.key_size);

  const WebmEncryptedData& frame1 = kWebmEncryptedFrames[1];
  scoped_refptr<DecoderBuffer> encrypted_data1 =
      CreateWebMEncryptedBuffer(frame1.encrypted_data,
                                frame1.encrypted_data_size,
                                frame1.key_id, frame1.key_id_size);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed(encrypted_data1,
                                                    frame1.plain_text,
                                                    frame1.plain_text_size));

  scoped_refptr<DecoderBuffer> encrypted_data2 =
      CreateWebMEncryptedBuffer(frame2.encrypted_data,
                                frame2.encrypted_data_size,
                                frame2.key_id, frame2.key_id_size);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed(encrypted_data2,
                                                    frame2.plain_text,
                                                    frame2.plain_text_size));
}

TEST_F(AesDecryptorTest, CorruptedIv) {
  const WebmEncryptedData& frame = kWebmEncryptedFrames[0];
  GenerateKeyRequest(frame.key_id, frame.key_id_size);
  AddKeyAndExpectToSucceed(frame.key_id, frame.key_id_size,
                           frame.key, frame.key_size);

  // Change byte 13 to modify the IV. Bytes 13-20 of WebM encrypted data
  // contains the IV.
  std::vector<uint8> frame_with_bad_iv(
      frame.encrypted_data, frame.encrypted_data + frame.encrypted_data_size);
  frame_with_bad_iv[1]++;

  scoped_refptr<DecoderBuffer> encrypted_data =
      CreateWebMEncryptedBuffer(&frame_with_bad_iv[0],
                                frame.encrypted_data_size,
                                frame.key_id, frame.key_id_size);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectDataMismatch(encrypted_data,
                                                       frame.plain_text,
                                                       frame.plain_text_size));
}

TEST_F(AesDecryptorTest, CorruptedData) {
  const WebmEncryptedData& frame = kWebmEncryptedFrames[0];
  GenerateKeyRequest(frame.key_id, frame.key_id_size);
  AddKeyAndExpectToSucceed(frame.key_id, frame.key_id_size,
                           frame.key, frame.key_size);

  // Change last byte to modify the data. Bytes 21+ of WebM encrypted data
  // contains the encrypted frame.
  std::vector<uint8> frame_with_bad_vp8_data(
      frame.encrypted_data, frame.encrypted_data + frame.encrypted_data_size);
  frame_with_bad_vp8_data[frame.encrypted_data_size - 1]++;

  scoped_refptr<DecoderBuffer> encrypted_data =
      CreateWebMEncryptedBuffer(&frame_with_bad_vp8_data[0],
                                frame.encrypted_data_size,
                                frame.key_id, frame.key_id_size);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectDataMismatch(encrypted_data,
                                                       frame.plain_text,
                                                       frame.plain_text_size));
}

TEST_F(AesDecryptorTest, EncryptedAsUnencryptedFailure) {
  const WebmEncryptedData& frame = kWebmEncryptedFrames[0];
  GenerateKeyRequest(frame.key_id, frame.key_id_size);
  AddKeyAndExpectToSucceed(frame.key_id, frame.key_id_size,
                           frame.key, frame.key_size);

  // Change signal byte from an encrypted frame to an unencrypted frame. Byte
  // 12 of WebM encrypted data contains the signal byte.
  std::vector<uint8> frame_with_wrong_signal_byte(
      frame.encrypted_data, frame.encrypted_data + frame.encrypted_data_size);
  frame_with_wrong_signal_byte[0] = 0;

  scoped_refptr<DecoderBuffer> encrypted_data =
      CreateWebMEncryptedBuffer(&frame_with_wrong_signal_byte[0],
                                frame.encrypted_data_size,
                                frame.key_id, frame.key_id_size);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpectSizeDataMismatch(encrypted_data,
                                       frame.plain_text,
                                       frame.plain_text_size));
}

TEST_F(AesDecryptorTest, UnencryptedAsEncryptedFailure) {
  const WebmEncryptedData& frame = kWebmEncryptedFrames[3];
  GenerateKeyRequest(frame.key_id, frame.key_id_size);
  AddKeyAndExpectToSucceed(frame.key_id, frame.key_id_size,
                           frame.key, frame.key_size);

  // Change signal byte from an unencrypted frame to an encrypted frame. Byte
  // 0 of WebM encrypted data contains the signal byte.
  std::vector<uint8> frame_with_wrong_signal_byte(
      frame.encrypted_data, frame.encrypted_data + frame.encrypted_data_size);
  frame_with_wrong_signal_byte[0] = kWebMFlagEncryptedFrame;

  scoped_refptr<DecoderBuffer> encrypted_data =
      CreateWebMEncryptedBuffer(&frame_with_wrong_signal_byte[0],
                                frame.encrypted_data_size,
                                frame.key_id, frame.key_id_size);
  ASSERT_NO_FATAL_FAILURE(
      DecryptAndExpectSizeDataMismatch(encrypted_data,
                                       frame.plain_text,
                                       frame.plain_text_size));
}

TEST_F(AesDecryptorTest, SubsampleDecryption) {
  GenerateKeyRequest(kSubsampleKeyId, arraysize(kSubsampleKeyId));
  AddKeyAndExpectToSucceed(kSubsampleKeyId, arraysize(kSubsampleKeyId),
                           kSubsampleKey, arraysize(kSubsampleKey));
  scoped_refptr<DecoderBuffer> encrypted_data = CreateSubsampleEncryptedBuffer(
      kSubsampleData, arraysize(kSubsampleData),
      kSubsampleKeyId, arraysize(kSubsampleKeyId),
      kSubsampleIv, arraysize(kSubsampleIv),
      0,
      subsample_entries_);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed(
      encrypted_data, kSubsampleOriginalData, kSubsampleOriginalDataSize));
}

// Ensures noninterference of data offset and subsample mechanisms. We never
// expect to encounter this in the wild, but since the DecryptConfig doesn't
// disallow such a configuration, it should be covered.
TEST_F(AesDecryptorTest, SubsampleDecryptionWithOffset) {
  GenerateKeyRequest(kSubsampleKeyId, arraysize(kSubsampleKeyId));
  AddKeyAndExpectToSucceed(kSubsampleKeyId, arraysize(kSubsampleKeyId),
                           kSubsampleKey, arraysize(kSubsampleKey));
  scoped_refptr<DecoderBuffer> encrypted_data = CreateSubsampleEncryptedBuffer(
      kPaddedSubsampleData, arraysize(kPaddedSubsampleData),
      kSubsampleKeyId, arraysize(kSubsampleKeyId),
      kSubsampleIv, arraysize(kSubsampleIv),
      arraysize(kPaddedSubsampleData) - arraysize(kSubsampleData),
      subsample_entries_);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed(
      encrypted_data, kSubsampleOriginalData, kSubsampleOriginalDataSize));
}

// No subsample or offset.
TEST_F(AesDecryptorTest, NormalDecryption) {
  GenerateKeyRequest(kSubsampleKeyId, arraysize(kSubsampleKeyId));
  AddKeyAndExpectToSucceed(kSubsampleKeyId, arraysize(kSubsampleKeyId),
                           kSubsampleKey, arraysize(kSubsampleKey));
  scoped_refptr<DecoderBuffer> encrypted_data = CreateSubsampleEncryptedBuffer(
      kNoSubsampleData, arraysize(kNoSubsampleData),
      kSubsampleKeyId, arraysize(kSubsampleKeyId),
      kSubsampleIv, arraysize(kSubsampleIv),
      0,
      std::vector<SubsampleEntry>());
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToSucceed(
      encrypted_data, kSubsampleOriginalData, kSubsampleOriginalDataSize));
}

TEST_F(AesDecryptorTest, IncorrectSubsampleSize) {
  GenerateKeyRequest(kSubsampleKeyId, arraysize(kSubsampleKeyId));
  AddKeyAndExpectToSucceed(kSubsampleKeyId, arraysize(kSubsampleKeyId),
                           kSubsampleKey, arraysize(kSubsampleKey));
  std::vector<SubsampleEntry> entries = subsample_entries_;
  entries[2].cypher_bytes += 1;

  scoped_refptr<DecoderBuffer> encrypted_data = CreateSubsampleEncryptedBuffer(
      kSubsampleData, arraysize(kSubsampleData),
      kSubsampleKeyId, arraysize(kSubsampleKeyId),
      kSubsampleIv, arraysize(kSubsampleIv),
      0,
      entries);
  ASSERT_NO_FATAL_FAILURE(DecryptAndExpectToFail(encrypted_data));
}

}  // namespace media
