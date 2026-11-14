// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/iamf_util.h"

#include <optional>
#include <string_view>
#include <vector>

#include "starboard/common/span.h"
#include "starboard/common/string.h"

namespace starboard {
namespace {

constexpr uint8_t kIamfSequenceHeaderObu = 31;

// A lightweight, forward-only reader for a raw byte buffer.
class BufferReader {
 public:
  explicit BufferReader(span<const uint8_t> buffer)
      : view_(reinterpret_cast<const char*>(buffer.data()), buffer.size()) {}

  std::optional<uint8_t> ReadByte() {
    if (view_.empty()) {
      return std::nullopt;
    }
    const uint8_t byte = static_cast<const uint8_t>(view_.front());
    view_.remove_prefix(1);
    return byte;
  }

  // Reads a LEB128-encoded unsigned integer.
  std::optional<uint32_t> ReadLeb128() {
    uint32_t decoded_value = 0;
    for (size_t i = 0; i < 5; ++i) {
      auto byte_opt = ReadByte();
      if (!byte_opt.has_value()) {
        // Not enough data.
        return std::nullopt;
      }
      uint8_t byte = *byte_opt;

      if (i == 4 && (byte & 0x7f) > 0x0f) {
        // Invalid 5-byte encoding.
        return std::nullopt;
      }

      decoded_value |= (uint32_t)(byte & 0x7f) << (i * 7);

      if (!(byte & 0x80)) {
        return decoded_value;
      }
    }
    return std::nullopt;  // Value exceeds 5 bytes
  }

  bool Skip(size_t bytes_to_skip) {
    if (BytesRemaining() < bytes_to_skip) {
      return false;
    }
    view_.remove_prefix(bytes_to_skip);
    return true;
  }

  const uint8_t* CurrentData() const {
    return reinterpret_cast<const uint8_t*>(view_.data());
  }

  size_t BytesRemaining() const { return view_.size(); }

 private:
  std::string_view view_;
};

// Parses |input| as a 3-digit ASCII IAMF profile value (000..255).
std::optional<uint32_t> ParseProfile(std::string_view input) {
  if (input.size() != 3) {
    return std::nullopt;
  }

  const auto is_ascii_digit = [](char c) { return c >= '0' && c <= '9'; };
  if (!is_ascii_digit(input[0]) || !is_ascii_digit(input[1]) ||
      !is_ascii_digit(input[2])) {
    return std::nullopt;
  }

  uint32_t converted_val = static_cast<uint32_t>(input[0] - '0') * 100 +
                           static_cast<uint32_t>(input[1] - '0') * 10 +
                           static_cast<uint32_t>(input[2] - '0');
  if (converted_val > kIamfProfileMax) {
    return std::nullopt;
  }
  return converted_val;
}

std::optional<IamfSubstreamCodec> ParseSubstreamCodec(std::string_view codec) {
  if (codec == "Opus") {
    return kIamfSubstreamCodecOpus;
  }
  if (codec == "mp4a.40.2") {
    return kIamfSubstreamCodecMp4a;
  }
  if (codec == "fLaC") {
    return kIamfSubstreamCodecFlac;
  }
  if (codec == "ipcm") {
    return kIamfSubstreamCodecIpcm;
  }
  return std::nullopt;
}
}  // namespace

// static
std::optional<IamfMimeUtil> IamfMimeUtil::Create(std::string_view mime_type) {
  // Reference: Immersive Audio Model and Formats;
  //            v1.0.0
  //            6.3. Codecs Parameter String
  // (https://aomediacodec.github.io/iamf/v1.0.0-errata.html#codecsparameter)
  // Format: "iamf.<3-digit primary>.<3-digit additional>.<substream_codec>"
  // where substream_codec is one of "Opus", "mp4a.40.2", "fLaC", or "ipcm".
  constexpr std::string_view kIamfPrefix = "iamf.";
  if (!StartsWith(mime_type, kIamfPrefix)) {
    return std::nullopt;
  }
  mime_type.remove_prefix(kIamfPrefix.size());

  // Need "<3-digit primary>.<3-digit additional>.<codec>" (at least 9 chars).
  if (mime_type.size() < 9 || mime_type[3] != '.' || mime_type[7] != '.') {
    return std::nullopt;
  }

  std::optional<uint32_t> primary_profile =
      ParseProfile(mime_type.substr(0, 3));
  std::optional<uint32_t> additional_profile =
      ParseProfile(mime_type.substr(4, 3));
  std::optional<IamfSubstreamCodec> substream_codec =
      ParseSubstreamCodec(mime_type.substr(8));
  if (!primary_profile || !additional_profile || !substream_codec) {
    return std::nullopt;
  }

  return IamfMimeUtil(*primary_profile, *additional_profile, *substream_codec);
}

IamfMimeUtil::IamfMimeUtil(uint32_t primary_profile,
                           uint32_t additional_profile,
                           IamfSubstreamCodec substream_codec)
    : primary_profile_(primary_profile),
      additional_profile_(additional_profile),
      substream_codec_(substream_codec) {}

// static.
Result<IamfMimeUtil::IamfProfileInfo> IamfMimeUtil::ParseIamfSequenceHeaderObu(
    const std::vector<uint8_t>& data) {
  BufferReader reader(data);

  auto header_byte_opt = reader.ReadByte();
  if (!header_byte_opt) {
    return Failure("Truncated OBU header.");
  }

  uint8_t obu_type = (*header_byte_opt >> 3) & 0x1f;
  if (obu_type != kIamfSequenceHeaderObu) {
    return Failure(FormatString(
        "Tried to read OBU: %d instead of IA Sequence Header OBU "
        "type %d in ParseIamfSequenceHeaderObu().",
        static_cast<int>(obu_type), static_cast<int>(kIamfSequenceHeaderObu)));
  }

  auto obu_size_opt = reader.ReadLeb128();
  if (!obu_size_opt) {
    return Failure("Failed to parse OBU size.");
  }
  const uint32_t obu_size = *obu_size_opt;

  if (reader.BytesRemaining() < obu_size) {
    return Failure(FormatString("Parsed OBU size %u exceeds the data size %zu.",
                                obu_size, reader.BytesRemaining()));
  }

  // Create a sub-reader for the OBU payload to ensure we don't read past the
  // specified OBU size.
  BufferReader obu_reader(
      {reader.CurrentData(), static_cast<size_t>(obu_size)});

  // Skip ia_code.
  if (!obu_reader.Skip(sizeof(uint32_t))) {
    return Failure("Truncated OBU payload: missing ia_code.");
  }

  auto primary_profile = obu_reader.ReadByte();
  auto additional_profile = obu_reader.ReadByte();

  if (!primary_profile || !additional_profile) {
    return Failure("Truncated OBU payload: missing profile info.");
  }

  return Success(IamfProfileInfo{*primary_profile, *additional_profile});
}

}  // namespace starboard
