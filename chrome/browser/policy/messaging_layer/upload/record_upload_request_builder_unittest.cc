// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"

#include <cstdint>
#include <string>
#include "base/logging.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/test/task_environment.h"
#include "base/token.h"
#include "chrome/browser/policy/messaging_layer/util/test_request_payload.h"
#include "components/reporting/resources/resource_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Not;

namespace reporting {
namespace {

constexpr int64_t kGenerationId = 4321;
constexpr Priority kPriority = Priority::IMMEDIATE;

// Default values for EncryptionInfo
constexpr char kEncryptionKey[] = "abcdef";
constexpr int64_t kPublicKeyId = 9876;

// Default values for CompressionInformation.
constexpr CompressionInformation::CompressionAlgorithm kCompressionAlgorithm =
    CompressionInformation::COMPRESSION_SNAPPY;

class RecordUploadRequestBuilderTest : public ::testing::TestWithParam<bool> {
 public:
  RecordUploadRequestBuilderTest() = default;

 protected:
  static EncryptedRecord GenerateEncryptedRecord(
      const base::StringPiece encrypted_wrapped_record,
      const bool set_compression = false) {
    EncryptedRecord record;
    record.set_encrypted_wrapped_record(std::string(encrypted_wrapped_record));

    auto* const sequence_information = record.mutable_sequence_information();
    sequence_information->set_sequencing_id(GetNextSequencingId());
    sequence_information->set_generation_id(kGenerationId);
    sequence_information->set_priority(kPriority);

    auto* const encryption_info = record.mutable_encryption_info();
    encryption_info->set_encryption_key(kEncryptionKey);
    encryption_info->set_public_key_id(kPublicKeyId);

    if (set_compression) {
      auto* const compression_information =
          record.mutable_compression_information();
      compression_information->set_compression_algorithm(kCompressionAlgorithm);
    }

    return record;
  }

  std::pair<ScopedReservation, std::vector<EncryptedRecord>>
  GetRecordListWithCorruptionAtIndex(size_t num_records,
                                     size_t corrupted_record_index) {
    DCHECK(corrupted_record_index < num_records)
        << "Corrupted record index greater than or equal to record count";

    std::vector<EncryptedRecord> records;
    records.reserve(num_records);
    ScopedReservation total_reservation;
    for (size_t counter = 0; counter < num_records; ++counter) {
      records.push_back(GenerateEncryptedRecord(
          base::StrCat({"TEST_INFO_", base::NumberToString(counter)})));
      ScopedReservation record_reservation(records.back().ByteSizeLong(),
                                           memory_resource_);
      EXPECT_TRUE(record_reservation.reserved());
      total_reservation.HandOver(record_reservation);
    }
    // Corrupt one record.
    records[corrupted_record_index]
        .mutable_sequence_information()
        ->clear_generation_id();

    return std::make_pair(std::move(total_reservation), std::move(records));
  }

  static int64_t GetNextSequencingId() {
    static int64_t sequencing_id = 0;
    return sequencing_id++;
  }

  void SetUp() override {
    memory_resource_ =
        base::MakeRefCounted<ResourceManager>(4u * 1024LLu * 1024LLu);  // 4 MiB
  }

  void TearDown() override {
    EXPECT_THAT(memory_resource_->GetUsed(), Eq(0uL));
  }

  bool need_encryption_key() const { return GetParam(); }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<ResourceManager> memory_resource_;
};

TEST_P(RecordUploadRequestBuilderTest, AcceptEncryptedRecordsList) {
  static constexpr size_t kNumRecords = 10;

  std::vector<EncryptedRecord> records;
  records.reserve(kNumRecords);
  ScopedReservation total_reservation;
  for (size_t counter = 0; counter < kNumRecords; ++counter) {
    records.push_back(GenerateEncryptedRecord(
        base::StrCat({"TEST_INFO_", base::NumberToString(counter)})));
    ScopedReservation record_reservation(records.back().ByteSizeLong(),
                                         memory_resource_);
    EXPECT_TRUE(record_reservation.reserved());
    total_reservation.HandOver(record_reservation);
  }

  UploadEncryptedReportingRequestBuilder builder(need_encryption_key());
  for (auto record : records) {
    builder.AddRecord(std::move(record), total_reservation);
  }
  auto request_payload = builder.Build();
  ASSERT_TRUE(request_payload.has_value());
  EXPECT_TRUE(total_reservation.reserved());

  EXPECT_THAT(
      request_payload.value(),
      AllOf(IsDataUploadRequestValid(),
            IsEncryptionKeyRequestUploadRequestValid(need_encryption_key())));
  base::Value::List* const record_list = request_payload->FindList(
      UploadEncryptedReportingRequestBuilder::GetEncryptedRecordListPath());
  ASSERT_TRUE(record_list);
  EXPECT_EQ(record_list->size(), records.size());

  ScopedReservation verify_reservation;
  size_t counter = 0;
  for (auto record : records) {
    ScopedReservation record_reservation(record.ByteSizeLong(),
                                         memory_resource_);
    EXPECT_TRUE(record_reservation.reserved());
    verify_reservation.HandOver(record_reservation);
    auto record_value_result =
        EncryptedRecordDictionaryBuilder(std::move(record), verify_reservation)
            .Build();
    ASSERT_TRUE(record_value_result.has_value());
    EXPECT_EQ((*record_list)[counter++].GetDict(), record_value_result.value());
  }
}

TEST_P(RecordUploadRequestBuilderTest, BreakListOnSingleBadRecord) {
  static constexpr size_t kNumRecords = 10;
  auto records = GetRecordListWithCorruptionAtIndex(
      kNumRecords, /*corrupted_record_index=*/kNumRecords - 2);
  UploadEncryptedReportingRequestBuilder builder(need_encryption_key());
  for (auto record : records.second) {
    builder.AddRecord(std::move(record), records.first);
    EXPECT_TRUE(records.first.reserved());
  }
  auto request_payload = builder.Build();
  ASSERT_FALSE(request_payload.has_value()) << request_payload.value();
}

TEST_P(RecordUploadRequestBuilderTest, DenyPoorlyFormedEncryptedRecords) {
  // Reject empty record.
  EncryptedRecord record;
  ScopedReservation empty_record_reservation(record.ByteSizeLong(),
                                             memory_resource_);
  EXPECT_FALSE(empty_record_reservation.reserved());  // Size is 0

  EXPECT_FALSE(
      EncryptedRecordDictionaryBuilder(record, empty_record_reservation)
          .Build()
          .has_value());
  EXPECT_FALSE(empty_record_reservation.reserved());  // Reservation is still 0

  // Reject encrypted_wrapped_record without sequence information.
  record.set_encrypted_wrapped_record("Enterprise");
  ScopedReservation record_reservation(record.ByteSizeLong(), memory_resource_);
  EXPECT_TRUE(record_reservation.reserved());

  EXPECT_FALSE(EncryptedRecordDictionaryBuilder(record, record_reservation)
                   .Build()
                   .has_value());

  // Reject incorrectly set sequence information by only setting sequencing id.
  auto* sequence_information = record.mutable_sequence_information();
  sequence_information->set_sequencing_id(1701);

  EXPECT_FALSE(EncryptedRecordDictionaryBuilder(record, record_reservation)
                   .Build()
                   .has_value());

  // Finish correctly setting sequence information but incorrectly set
  // encryption info.
  sequence_information->set_generation_id(12345678);
  sequence_information->set_priority(IMMEDIATE);

  auto* encryption_info = record.mutable_encryption_info();
  encryption_info->set_encryption_key("Key");

  EXPECT_FALSE(EncryptedRecordDictionaryBuilder(record, record_reservation)
                   .Build()
                   .has_value());

  // Finish correctly setting encryption info - expect complete call.
  encryption_info->set_public_key_id(1234);

  const auto record_dict =
      EncryptedRecordDictionaryBuilder(record, record_reservation).Build();
  ASSERT_TRUE(record_dict.has_value());
  EXPECT_THAT(record_dict.value(), IsRecordValid<>());
  EXPECT_TRUE(record_reservation.reserved());
}

TEST_P(RecordUploadRequestBuilderTest, AcceptRequestId) {
  const auto request_id = base::Token::CreateRandom().ToString();
  UploadEncryptedReportingRequestBuilder builder(need_encryption_key());
  builder.SetRequestId(request_id);

  const auto request_payload = builder.Build();
  ASSERT_TRUE(request_payload.has_value());
  EXPECT_THAT(request_payload.value(),
              IsEncryptionKeyRequestUploadRequestValid(need_encryption_key()));
  auto* payload_request_id = request_payload->FindString(
      UploadEncryptedReportingRequestBuilder::kRequestId);
  EXPECT_THAT(*payload_request_id, ::testing::StrEq(request_id));
}

TEST_P(RecordUploadRequestBuilderTest, DenyRequestIdWhenBadRecordSet) {
  static constexpr size_t kNumRecords = 5;
  auto records = GetRecordListWithCorruptionAtIndex(
      kNumRecords, /*corrupted_record_index=*/kNumRecords - 2);
  UploadEncryptedReportingRequestBuilder builder;
  for (auto record : records.second) {
    builder.AddRecord(std::move(record), records.first);
    EXPECT_TRUE(records.first.reserved());
  }

  const auto request_id = base::Token::CreateRandom().ToString();
  builder.SetRequestId(request_id);

  const auto request_payload = builder.Build();
  ASSERT_FALSE(request_payload.has_value());
}

TEST_P(RecordUploadRequestBuilderTest,
       DontBuildCompressionRequestIfNoInformation) {
  EncryptedRecord compressionless_record = GenerateEncryptedRecord("TEST_INFO");
  ASSERT_FALSE(compressionless_record.has_compression_information());

  ScopedReservation record_reservation(compressionless_record.ByteSizeLong(),
                                       memory_resource_);
  EXPECT_TRUE(record_reservation.reserved());

  absl::optional<base::Value::Dict> compressionless_payload =
      EncryptedRecordDictionaryBuilder(std::move(compressionless_record),
                                       record_reservation)
          .Build();
  ASSERT_TRUE(compressionless_payload.has_value());
  EXPECT_THAT(compressionless_payload.value(), IsRecordValid<>());
  EXPECT_FALSE(compressionless_payload.value().Find(
      EncryptedRecordDictionaryBuilder::GetCompressionInformationPath()));
}

TEST_P(RecordUploadRequestBuilderTest, IncludeCompressionRequest) {
  EncryptedRecord compressed_record =
      GenerateEncryptedRecord("TEST_INFO", true);
  ASSERT_TRUE(compressed_record.has_compression_information());

  ScopedReservation record_reservation(compressed_record.ByteSizeLong(),
                                       memory_resource_);
  EXPECT_TRUE(record_reservation.reserved());

  absl::optional<base::Value::Dict> compressed_record_payload =
      EncryptedRecordDictionaryBuilder(std::move(compressed_record),
                                       record_reservation)
          .Build();
  ASSERT_TRUE(compressed_record_payload.has_value());
  EXPECT_THAT(
      compressed_record_payload.value(),
      RequestValidityMatcherBuilder<>::CreateRecord()
          .AppendMatcher(RecordMatcher::SetMode(
              CompressionInformationMatcher(), RecordMatcher::Mode::RecordOnly))
          .Build());
}

INSTANTIATE_TEST_SUITE_P(NeedOrNoNeedKey,
                         RecordUploadRequestBuilderTest,
                         testing::Bool());

}  // namespace
}  // namespace reporting
