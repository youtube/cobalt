// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quick_start_message.h"
#include <memory>
#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "chromeos/ash/components/quick_start/quick_start_message_type.h"
#include "sandbox/policy/sandbox.h"

namespace {

std::string GetStringKeyForQuickStartMessageType(
    ash::quick_start::QuickStartMessageType message_type) {
  switch (message_type) {
    case ash::quick_start::QuickStartMessageType::kSecondDeviceAuthPayload:
      return "secondDeviceAuthPayload";
    case ash::quick_start::QuickStartMessageType::kBootstrapConfigurations:
      return "bootstrapConfigurations";
    case ash::quick_start::QuickStartMessageType::kFidoMessage:
      return "fidoMessage";
    case ash::quick_start::QuickStartMessageType::kQuickStartPayload:
      return "quickStartPayload";
  }
}

bool IsMessagePayloadBase64Encoded(
    ash::quick_start::QuickStartMessageType message_type) {
  switch (message_type) {
    case ash::quick_start::QuickStartMessageType::kSecondDeviceAuthPayload:
    case ash::quick_start::QuickStartMessageType::kBootstrapConfigurations:
    case ash::quick_start::QuickStartMessageType::kFidoMessage:
      return false;
    case ash::quick_start::QuickStartMessageType::kQuickStartPayload:
      return true;
  }
}

}  // namespace

namespace ash::quick_start {

// static
bool QuickStartMessage::enable_sandbox_checks_ = true;

// static
void QuickStartMessage::DisableSandboxCheckForTesting() {
  enable_sandbox_checks_ = false;
}

// static
std::unique_ptr<QuickStartMessage> QuickStartMessage::ReadMessage(
    std::vector<uint8_t> data,
    QuickStartMessageType message_type) {
  /*
    Since this code could handle untrusted data, it is important this
    runs only from a strongly sandboxed process (i.e. not the browser process).

    This check forces this to hold true. For testing, the
    DisableSandboxCheckForTesting function can be used to override this
    - but that method cannot be called from production code.
  */
  if (enable_sandbox_checks_) {
    CHECK(sandbox::policy::Sandbox::IsProcessSandboxed());
  }
  std::string str_data(data.begin(), data.end());
  absl::optional<base::Value> data_value = base::JSONReader::Read(str_data);
  if (!data_value.has_value()) {
    LOG(ERROR) << "Message is not JSON";
    return nullptr;
  }

  if (!data_value->is_dict()) {
    LOG(ERROR) << "Message is not a JSON dictionary";
    return nullptr;
  }

  std::string payload_key = GetStringKeyForQuickStartMessageType(message_type);
  bool is_payload_base64_encoded = IsMessagePayloadBase64Encoded(message_type);

  base::Value::Dict& message = data_value.value().GetDict();
  base::Value::Dict* payload;

  if (is_payload_base64_encoded) {
    std::string* base64_encoded_payload = message.FindString(payload_key);
    if (base64_encoded_payload == nullptr) {
      LOG(ERROR) << "Message does not contain any payload";
      return nullptr;
    }

    std::string json_payload;
    bool base64_decoding_succeeded =
        base::Base64Decode(*base64_encoded_payload, &json_payload);
    if (!base64_decoding_succeeded) {
      LOG(ERROR) << "Message does not contain a valid base64 encoded payload";
      return nullptr;
    }

    absl::optional<base::Value> json_reader_result =
        base::JSONReader::Read(json_payload);
    if (!json_reader_result.has_value()) {
      LOG(ERROR) << "Unable to decode base64 encoded payload into JSON";
      return nullptr;
    }

    payload = json_reader_result->GetIfDict();

    if (payload == nullptr) {
      LOG(ERROR) << "Payload is not a JSON dictionary";
      return nullptr;
    }

    return std::make_unique<QuickStartMessage>(message_type, payload->Clone());
  } else {
    payload = message.FindDict(payload_key);
    if (payload == nullptr) {
      LOG(ERROR) << "Payload is not present in the message";
      return nullptr;
    }
    return std::make_unique<QuickStartMessage>(message_type, payload->Clone());
  }
}

QuickStartMessage::QuickStartMessage(QuickStartMessageType message_type)
    : message_type_(message_type) {
  payload_ = base::Value::Dict();
}

QuickStartMessage::QuickStartMessage(QuickStartMessageType message_type,
                                     base::Value::Dict payload)
    : message_type_(message_type), payload_(std::move(payload)) {}

QuickStartMessage::~QuickStartMessage() = default;

base::Value::Dict* QuickStartMessage::GetPayload() {
  return &payload_;
}

std::unique_ptr<base::Value::Dict> QuickStartMessage::GenerateEncodedMessage() {
  std::unique_ptr<base::Value::Dict> message =
      std::make_unique<base::Value::Dict>();
  std::string str_payload_key =
      GetStringKeyForQuickStartMessageType(message_type_);
  bool base64_encoded_payload_ = IsMessagePayloadBase64Encoded(message_type_);
  if (!base64_encoded_payload_) {
    message->Set(str_payload_key, std::move(payload_));
  } else {
    std::string json;
    bool json_writer_succeeded = base::JSONWriter::Write(payload_, &json);

    if (!json_writer_succeeded) {
      LOG(ERROR) << "Failed to write message payload to JSON.";
      return nullptr;
    }

    std::string base64_payload;
    base::Base64Encode(json, &base64_payload);

    message->Set(str_payload_key, base64_payload);
  }
  return message;
}

}  // namespace ash::quick_start
