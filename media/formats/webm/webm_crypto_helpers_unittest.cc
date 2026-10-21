// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/webm/webm_crypto_helpers.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace {

const uint8_t kKeyId[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

}  // namespace

namespace media {

TEST(WebMCryptoHelpersTest, EmptyData) {
  std::unique_ptr<DecryptConfig> decrypt_config;
  size_t data_offset;
  ASSERT_FALSE(WebMCreateDecryptConfig(nullptr, 0, kKeyId, sizeof(kKeyId),
                                       &decrypt_config, &data_offset));
}

TEST(WebMCryptoHelpersTest, ClearData) {
  const uint8_t kData[] = {0x00, 0x0d, 0x0a, 0x0d, 0x0a};
  std::unique_ptr<DecryptConfig> decrypt_config;
  size_t data_offset;
  ASSERT_TRUE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                      sizeof(kKeyId), &decrypt_config,
                                      &data_offset));
  EXPECT_EQ(1u, data_offset);
  EXPECT_FALSE(decrypt_config);
}

TEST(WebMCryptoHelpersTest, EncryptedButNotEnoughBytes) {
  const uint8_t kData[] = {0x01, 0x0d, 0x0a, 0x0d, 0x0a};
  std::unique_ptr<DecryptConfig> decrypt_config;
  size_t data_offset;
  ASSERT_FALSE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                       sizeof(kKeyId), &decrypt_config,
                                       &data_offset));
}

TEST(WebMCryptoHelpersTest, EncryptedNotPartitioned) {
  const uint8_t kData[] = {
      // Encrypted
      0x01,
      // IV
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      // Data
      0x01, 0x02,
  };
  // Extracted from kData and zero extended to 16 bytes.
  const uint8_t kExpectedIv[] = {
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  size_t data_offset;
  ASSERT_TRUE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                      sizeof(kKeyId), &decrypt_config,
                                      &data_offset));
  EXPECT_TRUE(decrypt_config);
  EXPECT_EQ(std::string(kKeyId, kKeyId + sizeof(kKeyId)),
            decrypt_config->key_id());
  EXPECT_EQ(std::string(kExpectedIv, kExpectedIv + sizeof(kExpectedIv)),
            decrypt_config->iv());
  EXPECT_TRUE(decrypt_config->subsamples().empty());
}

TEST(WebMCryptoHelpersTest, EncryptedPartitionedMissingNumPartitionField) {
  const uint8_t kData[] = {
      // Encrypted and Partitioned
      0x03,
      // IV
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  size_t data_offset;
  ASSERT_FALSE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                       sizeof(kKeyId), &decrypt_config,
                                       &data_offset));
}

TEST(WebMCryptoHelpersTest, EncryptedPartitionedNotEnoughBytesForOffsets) {
  const uint8_t kData[] = {
      // Encrypted and Partitioned
      0x03,
      // IV
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      // Num partitions = 2
      0x02,
      // Partition 0 @ offset 3
      0x00, 0x00, 0x00, 0x03,
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  size_t data_offset;
  ASSERT_FALSE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                       sizeof(kKeyId), &decrypt_config,
                                       &data_offset));
}

TEST(WebMCryptoHelpersTest, EncryptedPartitionedNotEnoughBytesForData) {
  const uint8_t kData[] = {
      // Encrypted and Partitioned
      0x03,
      // IV
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      // Num partitions = 2
      0x02,
      // Partition 0 @ offset 3, partition 2 @ offset 5
      0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05,
      // Should have more than 5 bytes of data
      0x00, 0x01, 0x02, 0x03,
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  size_t data_offset;
  ASSERT_FALSE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                       sizeof(kKeyId), &decrypt_config,
                                       &data_offset));
}

TEST(WebMCryptoHelpersTest, EncryptedPartitionedNotEnoughBytesForData2) {
  const uint8_t kData[] = {
      // Encrypted and Partitioned
      0x03,
      // IV
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      // Num partitions = 2
      0x02,
      // Partition 0 @ offset 3, partition 1 @ offset 5
      0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05,
      // Should have more than 5 bytes of data
      0x00, 0x01, 0x02, 0x03, 0x04,
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  size_t data_offset;
  ASSERT_FALSE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                       sizeof(kKeyId), &decrypt_config,
                                       &data_offset));
}

TEST(WebMCryptoHelpersTest, EncryptedPartitionedDecreasingOffsets) {
  const uint8_t kData[] = {
      // Encrypted and Partitioned
      0x03,
      // IV
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      // Num partitions = 2
      0x02,
      // Partition 0 @ offset 3, partition 1 @ offset 2
      0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02,
      // Should have more than 5 bytes of data
      0x00, 0x01, 0x02, 0x03, 0x04,
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  size_t data_offset;
  ASSERT_FALSE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                       sizeof(kKeyId), &decrypt_config,
                                       &data_offset));
}

TEST(WebMCryptoHelpersTest, EncryptedPartitionedEvenNumberOfPartitions) {
  const uint8_t kData[] = {
      // Encrypted and Partitioned
      0x03,
      // IV
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      // Num partitions = 2
      0x02,
      // Partition 0 @ offset 3, partition 1 @ offset 5
      0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05,
      // Should have more than 5 bytes of data
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
  };
  // Extracted from kData and zero extended to 16 bytes.
  const uint8_t kExpectedIv[] = {
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  size_t data_offset;
  ASSERT_TRUE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                      sizeof(kKeyId), &decrypt_config,
                                      &data_offset));
  EXPECT_TRUE(decrypt_config);
  EXPECT_EQ(std::string(kKeyId, kKeyId + sizeof(kKeyId)),
            decrypt_config->key_id());
  EXPECT_EQ(std::string(kExpectedIv, kExpectedIv + sizeof(kExpectedIv)),
            decrypt_config->iv());
  EXPECT_THAT(decrypt_config->subsamples(),
              ElementsAre(SubsampleEntry(3, 2), SubsampleEntry(1, 0)));
  EXPECT_EQ(18u, data_offset);
}

TEST(WebMCryptoHelpersTest, EncryptedPartitionedOddNumberOfPartitions) {
  const uint8_t kData[] = {
      // Encrypted and Partitioned
      0x03,
      // IV
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      // Num partitions = 1
      0x01,
      // Partition 0 @ offset 3,
      0x00, 0x00, 0x00, 0x03,
      // Should have more than 3 bytes of data
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
  };
  // Extracted from kData and zero extended to 16 bytes.
  const uint8_t kExpectedIv[] = {
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  size_t data_offset;
  ASSERT_TRUE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                      sizeof(kKeyId), &decrypt_config,
                                      &data_offset));
  EXPECT_TRUE(decrypt_config);
  EXPECT_EQ(std::string(kKeyId, kKeyId + sizeof(kKeyId)),
            decrypt_config->key_id());
  EXPECT_EQ(std::string(kExpectedIv, kExpectedIv + sizeof(kExpectedIv)),
            decrypt_config->iv());
  EXPECT_THAT(decrypt_config->subsamples(), ElementsAre(SubsampleEntry(3, 3)));
  EXPECT_EQ(14u, data_offset);
}

TEST(WebMCryptoHelpersTest, EncryptedPartitionedZeroNumberOfPartitions) {
  const uint8_t kData[] = {
      // Encrypted and Partitioned
      0x03,
      // IV
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      // Num partitions = 0
      0x00,
      // Some random data.
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
  };
  // Extracted from kData and zero extended to 16 bytes.
  const uint8_t kExpectedIv[] = {
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  size_t data_offset;
  ASSERT_TRUE(WebMCreateDecryptConfig(kData, sizeof(kData), kKeyId,
                                      sizeof(kKeyId), &decrypt_config,
                                      &data_offset));
  EXPECT_TRUE(decrypt_config);
  EXPECT_EQ(std::string(kKeyId, kKeyId + sizeof(kKeyId)),
            decrypt_config->key_id());
  EXPECT_EQ(std::string(kExpectedIv, kExpectedIv + sizeof(kExpectedIv)),
            decrypt_config->iv());
  EXPECT_THAT(decrypt_config->subsamples(), ElementsAre(SubsampleEntry(6, 0)));
  EXPECT_EQ(10u, data_offset);
}

#if BUILDFLAG(USE_STARBOARD_MEDIA)
namespace {

// Complete frames with each kind of encryption header, taken from the tests
// above.
std::vector<std::vector<uint8_t>> SampleFrames() {
  return {
      // Clear.
      {0x00, 0x0d, 0x0a, 0x0d, 0x0a},
      // Encrypted, not partitioned.
      {
          0x01,                                            // Signal byte.
          0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,  // IV.
          0x01, 0x02,                                      // Data.
      },
      // Partitioned, with no partition offsets.
      {
          0x03,                                            // Signal byte.
          0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,  // IV.
          0x00,                                            // Num partitions.
          0x00, 0x01, 0x02, 0x03, 0x04, 0x05,              // Data.
      },
      // Partitioned, with 1 partition offset: 3.
      {
          0x03,                                            // Signal byte.
          0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,  // IV.
          0x01,                                            // Num partitions.
          0x00, 0x00, 0x00, 0x03,                          // Offsets.
          0x00, 0x01, 0x02, 0x03, 0x04, 0x05,              // Data.
      },
      // Partitioned, with 2 partition offsets: 3 and 5.
      {
          0x03,                                            // Signal byte.
          0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,  // IV.
          0x02,                                            // Num partitions.
          0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05,  // Offsets.
          0x00, 0x01, 0x02, 0x03, 0x04, 0x05,              // Data.
      },
      // Partitioned, with 2 decreasing partition offsets: 3 and 2.
      {
          0x03,                                            // Signal byte.
          0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,  // IV.
          0x02,                                            // Num partitions.
          0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02,  // Offsets.
          0x00, 0x01, 0x02, 0x03, 0x04,                    // Data.
      },
  };
}

// The outputs of one WebMCreateDecryptConfig*() call.
struct DecryptConfigResult {
  bool success = false;
  std::unique_ptr<DecryptConfig> decrypt_config;
  size_t data_offset = 0;
};

DecryptConfigResult CreateFromWholeFrame(base::span<const uint8_t> frame) {
  DecryptConfigResult result;
  result.success = WebMCreateDecryptConfig(
      frame.data(), static_cast<int>(frame.size()), kKeyId, sizeof(kKeyId),
      &result.decrypt_config, &result.data_offset);
  return result;
}

DecryptConfigResult CreateFromPrefix(base::span<const uint8_t> prefix,
                                     size_t frame_size) {
  DecryptConfigResult result;
  result.success = WebMCreateDecryptConfigFromPrefix(
      prefix, frame_size, kKeyId, &result.decrypt_config, &result.data_offset);
  return result;
}

void ExpectSameResult(const DecryptConfigResult& expected,
                      const DecryptConfigResult& actual) {
  ASSERT_EQ(expected.success, actual.success);
  if (!expected.success) {
    return;
  }
  EXPECT_EQ(expected.data_offset, actual.data_offset);
  if (!expected.decrypt_config) {
    EXPECT_FALSE(actual.decrypt_config);
    return;
  }
  ASSERT_TRUE(actual.decrypt_config);
  EXPECT_TRUE(actual.decrypt_config->Matches(*expected.decrypt_config))
      << *actual.decrypt_config << " vs. " << *expected.decrypt_config;
}

}  // namespace

// Given the whole frame, WebMCreateDecryptConfigFromPrefix() must behave like
// WebMCreateDecryptConfig(). Every truncation of each sample frame is tried as
// a frame of its own, which also covers the failure cases.
TEST(WebMCryptoHelpersTest, FromPrefixWithWholeFrame) {
  const std::vector<std::vector<uint8_t>> samples = SampleFrames();
  for (size_t i = 0; i < samples.size(); ++i) {
    for (size_t size = 0; size <= samples[i].size(); ++size) {
      SCOPED_TRACE(::testing::Message()
                   << "sample " << i << " truncated to " << size << " bytes");
      const base::span<const uint8_t> frame =
          base::span(samples[i]).first(size);
      ExpectSameResult(CreateFromWholeFrame(frame),
                       CreateFromPrefix(frame, frame.size()));
    }
  }
}

// A prefix that covers the encryption header must give the same result as the
// whole frame, and a shorter one must be rejected.
TEST(WebMCryptoHelpersTest, FromPrefixWithPartialFrame) {
  const std::vector<std::vector<uint8_t>> samples = SampleFrames();
  for (size_t i = 0; i < samples.size(); ++i) {
    const DecryptConfigResult expected = CreateFromWholeFrame(samples[i]);
    // On success, `data_offset` is the size of the encryption header.
    const size_t header_size = expected.success ? expected.data_offset : 0;
    for (size_t size = 0; size <= samples[i].size(); ++size) {
      SCOPED_TRACE(::testing::Message()
                   << "sample " << i << " with a " << size << " byte prefix");
      const DecryptConfigResult actual = CreateFromPrefix(
          base::span(samples[i]).first(size), samples[i].size());
      if (size < header_size) {
        EXPECT_FALSE(actual.success);
      } else {
        ExpectSameResult(expected, actual);
      }
    }
  }
}

TEST(WebMCryptoHelpersTest, FromPrefixWithHeaderOnly) {
  const uint8_t kHeader[] = {
      0x03,                                            // Signal byte.
      0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a, 0x0d, 0x0a,  // IV.
      0x02,                                            // Num partitions.
      0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05,  // Offsets.
  };
  std::unique_ptr<DecryptConfig> decrypt_config;
  size_t data_offset;

  // The 6 bytes of frame data after the header are not passed in, but the last
  // subsample still extends to their end.
  ASSERT_TRUE(WebMCreateDecryptConfigFromPrefix(
      kHeader, sizeof(kHeader) + 6, kKeyId, &decrypt_config, &data_offset));
  ASSERT_TRUE(decrypt_config);
  EXPECT_THAT(decrypt_config->subsamples(),
              ElementsAre(SubsampleEntry(3, 2), SubsampleEntry(1, 0)));
  EXPECT_EQ(sizeof(kHeader), data_offset);

  // A frame that ends with its header has no data to partition.
  EXPECT_FALSE(WebMCreateDecryptConfigFromPrefix(
      kHeader, sizeof(kHeader), kKeyId, &decrypt_config, &data_offset));
}
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

}  // namespace media
