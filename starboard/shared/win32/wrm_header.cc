// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/win32/wrm_header.h"

#include <guiddef.h>
#include <initguid.h>

#include <algorithm>

#include "starboard/common/byte_swap.h"
#include "starboard/memory.h"
#include "starboard/shared/win32/error_utils.h"

namespace starboard {
namespace shared {
namespace win32 {

using Microsoft::WRL::ComPtr;

namespace {

// The playready system id used to create the content header.
DEFINE_GUID(kPlayreadyContentHeaderCLSID,
            0xF4637010,
            0x03C3,
            0x42CD,
            0xB9,
            0x32,
            0xB4,
            0x8A,
            0xDF,
            0x3A,
            0x6A,
            0x54);

// The playready system id used to identify the wrm header in the
// initialization data.
DEFINE_GUID(kPlayreadyInitializationDataCLSID,
            0x79f0049a,
            0x4098,
            0x8642,
            0xab,
            0x92,
            0xe6,
            0x5b,
            0xe0,
            0x88,
            0x5f,
            0x95);

const uint16_t kPlayreadyWRMTag = 0x0001;

class Reader {
 public:
  Reader(const uint8_t* data, size_t length)
      : start_(data), curr_(data), end_(data + length) {}

  size_t GetPosition() const { return curr_ - start_; }
  size_t GetRemaining() const { return end_ - curr_; }

  const uint8_t* curr() const { curr_; }

  bool Skip(size_t distance) {
    if (distance > GetRemaining())
      return false;
    curr_ += distance;
    return true;
  }

  bool ReadGUID(GUID* guid) {
    if (sizeof(*guid) > GetRemaining()) {
      return false;
    }
    memcpy(guid, curr_, sizeof(*guid));
    curr_ += sizeof(*guid);
    return true;
  }

  bool ReadBigEndianU32(uint32_t* i) {
    if (sizeof(*i) > GetRemaining())
      return false;
    *i =
        curr_[0] * 0x1000000 + curr_[1] * 0x10000 + curr_[2] * 0x100 + curr_[3];
    curr_ += sizeof(*i);
    return true;
  }

  bool ReadLittleEndianU16(uint16_t* i) {
    if (sizeof(*i) > GetRemaining())
      return false;
    *i = curr_[0] + curr_[1] * 0x100;
    curr_ += sizeof(*i);
    return true;
  }

 private:
  const uint8_t* const start_;
  const uint8_t* curr_;
  const uint8_t* const end_;
};

std::vector<uint8_t> ParseWrmHeaderFromInitializationData(
    const void* initialization_data,
    int initialization_data_size) {
  if (initialization_data_size == 0) {
    SB_NOTIMPLEMENTED();
    return std::vector<uint8_t>();
  }

  std::vector<uint8_t> output;
  const uint8_t* data = static_cast<const uint8_t*>(initialization_data);

  Reader reader(data, initialization_data_size);
  while (reader.GetRemaining() > 0) {
    // Parse pssh atom (big endian)
    //
    // 4 bytes -- size
    // 4 bytes -- "pssh"
    // 4 bytes -- flags
    // 16 bytes -- guid
    uint32_t pssh_size;
    if (!reader.ReadBigEndianU32(&pssh_size)) {
      return output;
    }

    // Skipping pssh and flags
    if (!reader.Skip(8)) {
      return output;
    }
    GUID system_id;
    if (!reader.ReadGUID(&system_id)) {
      return output;
    }
    if (system_id != kPlayreadyInitializationDataCLSID) {
      // Skip entire pssh atom
      if (!reader.Skip(pssh_size - 28)) {
        return output;
      }
      continue;
    }

    // 4 bytes -- size of PlayreadyObject
    // followed by PlayreadyObject

    // Skip size, and continue parsing
    if (!reader.Skip(4)) {
      return output;
    }

    // Parse Playready object (little endian)
    // 4 bytes -- size
    // 2 bytes -- record count
    //
    // Playready Record
    // 2 bytes -- type
    // 2 bytes -- size of record
    // n bytes -- record
    if (!reader.Skip(4)) {
      return output;
    }
    uint16_t num_records;
    if (!reader.ReadLittleEndianU16(&num_records)) {
      return output;
    }

    for (int i = 0; i < num_records; i++) {
      uint16_t record_type;
      if (!reader.ReadLittleEndianU16(&record_type)) {
        return output;
      }
      uint16_t record_size;
      if (!reader.ReadLittleEndianU16(&record_size)) {
        return output;
      }
      if ((record_type & kPlayreadyWRMTag) == kPlayreadyWRMTag) {
        std::copy(data + reader.GetPosition(),
                  data + reader.GetPosition() + record_size,
                  std::back_inserter(output));
        return output;
      }
      if (!reader.Skip(record_size)) {
        return output;
      }
    }
  }

  return output;
}

uint32_t Base64ToValue(uint8_t byte) {
  if (byte >= 'A' && byte <= 'Z') {
    return byte - 'A';
  }
  if (byte >= 'a' && byte <= 'z') {
    return byte - 'a' + 26;
  }
  if (byte >= '0' && byte <= '9') {
    return byte - '0' + 52;
  }
  if (byte == '+') {
    return 62;
  }
  if (byte == '/') {
    return 63;
  }
  SB_DCHECK(byte == '=');
  return 0;
}

std::string Base64Decode(const std::wstring& input) {
  SB_DCHECK(input.size() % 4 == 0);

  std::string output;

  output.reserve(input.size() / 4 * 3);
  for (size_t i = 0; i < input.size() - 3; i += 4) {
    uint32_t decoded =
        Base64ToValue(input[i]) * 4 + Base64ToValue(input[i + 1]) / 16;
    output += static_cast<char>(decoded);
    if (input[i + 2] != '=') {
      decoded = Base64ToValue(input[i + 1]) % 16 * 16 +
                Base64ToValue(input[i + 2]) / 4;
      output += static_cast<char>(decoded);
      if (input[i + 3] != '=') {
        decoded =
            Base64ToValue(input[i + 2]) % 4 * 64 + Base64ToValue(input[i + 3]);
        output += static_cast<char>(decoded);
      }
    }
  }

  return output;
}

GUID ParseKeyIdFromWrmHeader(const std::vector<uint8_t>& wrm_header) {
  // The wrm_header is an XML in wchar_t that contains the base64 encoded key
  // id in <KID> node in base64.  The original key id should be 16 characters,
  // so it is 24 characters after based64 encoded.
  const size_t kEncodedKeyIdLength = 24;

  SB_DCHECK(wrm_header.size() % 2 == 0);
  std::wstring wrm_header_copy(
      reinterpret_cast<const wchar_t*>(wrm_header.data()),
      wrm_header.size() / 2);
  std::wstring::size_type begin = wrm_header_copy.find(L"<KID>");
  if (begin == wrm_header_copy.npos) {
    return WrmHeader::kInvalidKeyId;
  }
  std::wstring::size_type end = wrm_header_copy.find(L"</KID>", begin);
  if (end == wrm_header_copy.npos) {
    return WrmHeader::kInvalidKeyId;
  }
  begin += 5;
  if (end - begin != kEncodedKeyIdLength) {
    return WrmHeader::kInvalidKeyId;
  }

  std::string key_id_in_string =
      Base64Decode(wrm_header_copy.substr(begin, kEncodedKeyIdLength));
  if (key_id_in_string.size() != sizeof(GUID)) {
    return WrmHeader::kInvalidKeyId;
  }

  GUID key_id = *reinterpret_cast<const GUID*>(key_id_in_string.data());

  key_id.Data1 = SbByteSwap(key_id.Data1);
  key_id.Data2 = SbByteSwap(key_id.Data2);
  key_id.Data3 = SbByteSwap(key_id.Data3);

  return key_id;
}

ComPtr<IStream> CreateWrmHeaderStream(const std::vector<uint8_t>& wrm_header) {
  ComPtr<IStream> stream;
  HRESULT hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);
  if (FAILED(hr)) {
    return NULL;
  }

  DWORD wrm_header_size = static_cast<DWORD>(wrm_header.size());
  ULONG bytes_written = 0;

  if (wrm_header_size != 0) {
    hr = stream->Write(wrm_header.data(), wrm_header_size, &bytes_written);
    if (FAILED(hr)) {
      return NULL;
    }
  }

  return stream;
}

ComPtr<IStream> CreateContentHeaderFromWrmHeader(
    const std::vector<uint8_t>& wrm_header) {
  ComPtr<IStream> content_header;
  // Assume we use one license for one stream.
  const DWORD kNumStreams = 1;
  const DWORD kNextStreamId = static_cast<DWORD>(-1);

  HRESULT hr = CreateStreamOnHGlobal(NULL, TRUE, &content_header);
  if (FAILED(hr)) {
    return NULL;
  }

  // Initialize spInitStm with the required data
  // Format: (All DWORD values are serialized in little-endian order)
  // [GUID (content protection system guid, see msprita.idl)]
  // [DWORD (stream count, use the actual stream count even if all streams are
  // encrypted using the same data, note that zero is invalid)] [DWORD (next
  // stream ID, use -1 if all remaining streams are encrypted using the same
  // data)] [DWORD (next stream's binary data size)] [BYTE* (next stream's
  // binary data)] { Repeat from "next stream ID" above for each stream }
  DWORD wrm_header_size = static_cast<DWORD>(wrm_header.size());
  ULONG bytes_written = 0;
  hr = content_header->Write(&kPlayreadyContentHeaderCLSID,
                             sizeof(kPlayreadyContentHeaderCLSID),
                             &bytes_written);
  if (FAILED(hr)) {
    return NULL;
  }

  hr = content_header->Write(&kNumStreams, sizeof(kNumStreams), &bytes_written);
  if (FAILED(hr)) {
    return NULL;
  }

  hr = content_header->Write(&kNextStreamId, sizeof(kNextStreamId),
                             &bytes_written);
  if (FAILED(hr)) {
    return NULL;
  }

  hr = content_header->Write(&wrm_header_size, sizeof(wrm_header_size),
                             &bytes_written);
  if (FAILED(hr)) {
    return NULL;
  }

  if (0 != wrm_header_size) {
    hr = content_header->Write(wrm_header.data(), wrm_header_size,
                               &bytes_written);
    if (FAILED(hr)) {
      return NULL;
    }
  }

  return content_header;
}

ComPtr<IStream> ResetStreamPosition(const ComPtr<IStream>& stream) {
  if (stream == NULL) {
    return NULL;
  }
  LARGE_INTEGER seek_position = {0};
  HRESULT hr = stream->Seek(seek_position, STREAM_SEEK_SET, NULL);
  CheckResult(hr);
  return stream;
}

}  // namespace.

// static
GUID WrmHeader::kInvalidKeyId;

WrmHeader::WrmHeader(const void* initialization_data,
                     int initialization_data_size) {
  wrm_header_ = ParseWrmHeaderFromInitializationData(initialization_data,
                                                     initialization_data_size);
  key_id_ = ParseKeyIdFromWrmHeader(wrm_header_);
}

ComPtr<IStream> WrmHeader::content_header() const {
  SB_DCHECK(is_valid());

  if (key_id_ == kInvalidKeyId) {
    return NULL;
  }
  return ResetStreamPosition(CreateContentHeaderFromWrmHeader(wrm_header_));
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
