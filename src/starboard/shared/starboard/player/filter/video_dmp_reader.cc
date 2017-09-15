// Copyright 2017 Google Inc. All Rights Reserved.
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

// This file is deliberately not using any Cobalt/Starboard specific API so it
// can be used in an independent application.
#include "cobalt/media/base/video_dmp_reader.h"

#include <algorithm>

namespace {

template <typename T>
bool ReadUnchecked(VideoDmpReader* reader, bool reverse_byte_order, T* value) {
  if (!value) {
    reader->ReportFatalError();
    return false;
  }

  int bytes_to_read = static_cast<int>(sizeof(*value));
  int bytes_read = reader->Read(value, bytes_to_read);

  if (reverse_byte_order) {
    std::reverse(reinterpret_cast<uint8_t*>(value),
                 reinterpret_cast<uint8_t*>(value + 1));
  }

  return bytes_to_read == bytes_read;
}
template <typename T>
void ReadChecked(VideoDmpReader* reader, bool reverse_byte_order, T* value) {
  if (!ReadUnchecked(reader, reverse_byte_order, value)) {
    reader->ReportFatalError();
  }
}
void ReadChecked(VideoDmpReader* reader, bool reverse_byte_order,
                 std::vector<uint8_t>* value) {
  if (!value) {
    reader->ReportFatalError();
  }

  uint32_t size;
  ReadChecked(reader, reverse_byte_order, &size);

  value->resize(size);

  if (size == 0) {
    return;
  }

  int bytes_read = reader->Read(value->data(), size);
  if (bytes_read != size) {
    reader->ReportFatalError();
  }
}

VideoDmpReader::AccessUnit ReadAccessUnitChecked(
    VideoDmpReader* reader, bool reverse_byte_order,
    VideoDmpReader::AccessUnitType access_unit_type) {
  int64_t timestamp;
  ReadChecked(reader, reverse_byte_order, &timestamp);

  std::vector<uint8_t> key_id, iv;
  ReadChecked(reader, reverse_byte_order, &key_id);
  ReadChecked(reader, reverse_byte_order, &iv);

  uint32_t subsample_count;
  ReadChecked(reader, reverse_byte_order, &subsample_count);

  VideoDmpReader::Subsamples subsamples(subsample_count);
  for (auto& subsample : subsamples) {
    ReadChecked(reader, reverse_byte_order, &subsample.clear_bytes);
    ReadChecked(reader, reverse_byte_order, &subsample.encrypted_bytes);
  }

  std::vector<uint8_t> data;
  ReadChecked(reader, reverse_byte_order, &data);

  return VideoDmpReader::AccessUnit(access_unit_type, timestamp, key_id, iv,
                                    subsamples, std::move(data));
}

}  // namespace

void VideoDmpReader::Parse() {
  reverse_byte_order_ = false;
  uint32_t bom;
  ReadChecked(this, reverse_byte_order_, &bom);
  if (bom != kBOM) {
    std::reverse(reinterpret_cast<uint8_t*>(&bom),
                 reinterpret_cast<uint8_t*>(&bom + 1));
    if (bom != kBOM) {
      ReportFatalError();
      return;
    }
    reverse_byte_order_ = true;
  }
  uint32_t type;
  EmeInitData eme_init_data;
  while (ReadUnchecked(this, reverse_byte_order_, &type)) {
    uint32_t size;
    switch (type) {
      case kRecordTypeAudioConfig:
        ReadChecked(this, reverse_byte_order_, &size);
        if (size != 0) {
          ReportFatalError();
        }
        break;
      case kRecordTypeVideoConfig:
        ReadChecked(this, reverse_byte_order_, &size);
        if (size != 0) {
          ReportFatalError();
        }
        break;
      case kRecordTypeEmeInitData:
        ReadChecked(this, reverse_byte_order_, &eme_init_data);
        eme_init_datas_.push_back(eme_init_data);
        break;
      case kRecordTypeAudioAccessUnit:
        access_units_.push_back(ReadAccessUnitChecked(this, reverse_byte_order_,
                                                      kAccessUnitTypeAudio));
        break;
      case kRecordTypeVideoAccessUnit:
        access_units_.push_back(ReadAccessUnitChecked(this, reverse_byte_order_,
                                                      kAccessUnitTypeVideo));
        break;
    }
  }
}
