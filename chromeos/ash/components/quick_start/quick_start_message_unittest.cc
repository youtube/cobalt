// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quick_start_message.h"
#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chromeos/ash/components/quick_start/quick_start_message_type.h"
#include "testing/gtest/include/gtest/gtest.h"

using QuickStartMessage = ash::quick_start::QuickStartMessage;

class QuickStartMessageTest : public testing::Test {
 public:
  QuickStartMessageTest() = default;
  QuickStartMessageTest(QuickStartMessageTest&) = delete;
  QuickStartMessageTest& operator=(QuickStartMessageTest&) = delete;
  ~QuickStartMessageTest() override = default;

 protected:
  void SetUp() override {
    ash::quick_start::QuickStartMessage::DisableSandboxCheckForTesting();
  }
};

TEST_F(QuickStartMessageTest, ReadMessageSucceedsForNonBase64Message) {
  base::Value::Dict payload;
  payload.Set("key", "value");

  base::Value::Dict message;
  message.Set("bootstrapConfigurations", payload.Clone());
  std::string json_message;
  ASSERT_TRUE(base::JSONWriter::Write(message, &json_message));
  std::vector<uint8_t> data(json_message.begin(), json_message.end());

  std::unique_ptr<QuickStartMessage> result =
      ash::quick_start::QuickStartMessage::ReadMessage(
          data,
          ash::quick_start::QuickStartMessageType::kBootstrapConfigurations);

  ASSERT_NE(result, nullptr);
  ASSERT_EQ(*result->GetPayload()->FindString("key"), "value");
}

TEST_F(QuickStartMessageTest, ReadMessageFailsIfBase64WhenNotExpected) {
  base::Value::Dict payload;
  payload.Set("key", "value");
  std::string json_payload;
  ASSERT_TRUE(base::JSONWriter::Write(payload, &json_payload));
  std::string base64_payload;
  base::Base64Encode(json_payload, &base64_payload);

  base::Value::Dict message;
  message.Set("bootstrapConfigurations", base64_payload);
  std::string json_message;
  ASSERT_TRUE(base::JSONWriter::Write(message, &json_message));
  std::vector<uint8_t> data(json_message.begin(), json_message.end());

  std::unique_ptr<QuickStartMessage> result =
      ash::quick_start::QuickStartMessage::ReadMessage(
          data,
          ash::quick_start::QuickStartMessageType::kBootstrapConfigurations);

  ASSERT_EQ(result, nullptr);
}

TEST_F(QuickStartMessageTest, ReadMessageDecodesBase64Message) {
  base::Value::Dict payload;
  payload.Set("key", "value");
  std::string json_payload;
  ASSERT_TRUE(base::JSONWriter::Write(payload, &json_payload));
  std::string base64_payload;
  base::Base64Encode(json_payload, &base64_payload);

  base::Value::Dict message;
  message.Set("quickStartPayload", base64_payload);
  std::string json_message;
  ASSERT_TRUE(base::JSONWriter::Write(message, &json_message));
  std::vector<uint8_t> data(json_message.begin(), json_message.end());

  std::unique_ptr<QuickStartMessage> result =
      ash::quick_start::QuickStartMessage::ReadMessage(
          data, ash::quick_start::QuickStartMessageType::kQuickStartPayload);

  ASSERT_NE(result, nullptr);
  ASSERT_EQ(*result->GetPayload()->FindString("key"), "value");
}

TEST_F(QuickStartMessageTest,
       ReadMessageFailsIfPayloadIsNotBase64WhenExpected) {
  base::Value::Dict payload;
  payload.Set("key", "value");
  std::string json_payload;
  ASSERT_TRUE(base::JSONWriter::Write(payload, &json_payload));

  base::Value::Dict message;
  message.Set("quickStartPayload", json_payload);
  std::string json_message;
  ASSERT_TRUE(base::JSONWriter::Write(message, &json_message));
  std::vector<uint8_t> data(json_message.begin(), json_message.end());

  std::unique_ptr<QuickStartMessage> result =
      ash::quick_start::QuickStartMessage::ReadMessage(
          data, ash::quick_start::QuickStartMessageType::kQuickStartPayload);

  ASSERT_EQ(result, nullptr);
}

TEST_F(QuickStartMessageTest, ReadMessageFailsIfPayloadIsNotPresent) {
  base::Value::Dict message;
  std::string json_message;
  ASSERT_TRUE(base::JSONWriter::Write(message, &json_message));
  std::vector<uint8_t> data(json_message.begin(), json_message.end());

  std::unique_ptr<QuickStartMessage> result =
      ash::quick_start::QuickStartMessage::ReadMessage(
          data, ash::quick_start::QuickStartMessageType::kQuickStartPayload);

  ASSERT_EQ(result, nullptr);
}

TEST_F(QuickStartMessageTest, EncodeThenDecodeResultsInSameValue) {
  QuickStartMessage message(
      ash::quick_start::QuickStartMessageType::kQuickStartPayload);
  message.GetPayload()->Set("key", "value");

  std::unique_ptr<base::Value::Dict> encoded_message =
      message.GenerateEncodedMessage();
  std::string json_serialized_payload;
  ASSERT_TRUE(
      base::JSONWriter::Write(*encoded_message, &json_serialized_payload));
  std::vector<uint8_t> request_payload(json_serialized_payload.begin(),
                                       json_serialized_payload.end());

  std::unique_ptr<QuickStartMessage> decoded_message =
      QuickStartMessage::ReadMessage(
          request_payload,
          ash::quick_start::QuickStartMessageType::kQuickStartPayload);

  ASSERT_NE(decoded_message, nullptr);
  EXPECT_EQ(*(decoded_message->GetPayload()->FindString("key")), "value");
}
