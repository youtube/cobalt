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

#ifndef COBALT_MEDIA_BASE_VIDEO_DMP_READER_H_
#define COBALT_MEDIA_BASE_VIDEO_DMP_READER_H_

// This file is deliberately not using any Cobalt/Starboard specific API so it
// can be used in an independent application.

#include <algorithm>
#include <string>
#include <vector>

// File: <BOM> <Record>*
//   BOM: 0x76543210
//   Record: <4 bytes type> + <4 bytes size> + <|size| bytes binary data>
//     type: 0: audio config, 1: video config, 2: eme init data,
//           3: audio access unit, 4: video access unit.
//     audio/video access unit;
//       <8 bytes time stamp in microseconds>
//       <4 bytes size of key_id> + |size| bytes of key id
//       <4 bytes size of iv> + |size| bytes of iv
//       <4 bytes count> (0 for non-encrypted AU/frame)
//         (subsample: 4 bytes clear size, 4 bytes encrypted size) * |count|
//       <4 bytes size>
//         |size| bytes encoded binary data
class VideoDmpReader {
 public:
  enum {
    kRecordTypeAudioConfig,
    kRecordTypeVideoConfig,
    kRecordTypeEmeInitData,
    kRecordTypeAudioAccessUnit,
    kRecordTypeVideoAccessUnit,
  };

  enum AccessUnitType { kAccessUnitTypeAudio, kAccessUnitTypeVideo };

  struct Subsample {
    uint32_t clear_bytes;
    uint32_t encrypted_bytes;
  };

  typedef std::vector<Subsample> Subsamples;
  typedef std::vector<uint8_t> EmeInitData;

  class AccessUnit {
   public:
    AccessUnit(AccessUnitType access_unit_type, int64_t timestamp,
               const std::vector<uint8_t>& key_id,
               const std::vector<uint8_t>& iv, const Subsamples& subsamples,
               std::vector<uint8_t> data)
        : access_unit_type_(access_unit_type),
          timestamp_(timestamp),
          key_id_(key_id),
          iv_(iv),
          subsamples_(subsamples),
          data_(std::move(data)) {}
    AccessUnitType access_unit_type() const { return access_unit_type_; }
    int64_t timestamp() const { return timestamp_; }
    // Returns empty vector when the AU is not encrypted.
    const std::vector<uint8_t>& key_id() const { return key_id_; }
    const std::vector<uint8_t>& iv() const { return iv_; }
    const Subsamples& subsamples() const { return subsamples_; }
    const std::vector<uint8_t> data() const { return data_; }

   private:
    AccessUnitType access_unit_type_;
    int64_t timestamp_;
    std::vector<uint8_t> key_id_;
    std::vector<uint8_t> iv_;
    Subsamples subsamples_;
    std::vector<uint8_t> data_;
  };

  static const uint32_t kBOM = 0x76543210;

  VideoDmpReader() : reverse_byte_order_(false) {}

  // Abstract function implemented by the derived class to read data.  When
  // the return value is less than |bytes_to_read|, the stream reaches the end.
  virtual size_t Read(void* buffer, size_t bytes_to_read) = 0;
  virtual void ReportFatalError() = 0;

  const std::vector<EmeInitData> eme_init_datas() const {
    return eme_init_datas_;
  }
  const std::vector<AccessUnit>& access_units() const { return access_units_; }

  void Parse() {
    uint32_t bom;
    ReadChecked(&bom);
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
    while (ReadUnchecked(&type)) {
      uint32_t size;
      switch (type) {
        case kRecordTypeAudioConfig:
          ReadChecked(&size);
          if (size != 0) {
            ReportFatalError();
          }
          break;
        case kRecordTypeVideoConfig:
          ReadChecked(&size);
          if (size != 0) {
            ReportFatalError();
          }
          break;
        case kRecordTypeEmeInitData:
          ReadChecked(&eme_init_data);
          eme_init_datas_.push_back(eme_init_data);
          break;
        case kRecordTypeAudioAccessUnit:
          ReadChecked(&size);
          ReadAndAppendAccessUnitChecked(kAccessUnitTypeAudio);
          break;
        case kRecordTypeVideoAccessUnit:
          ReadChecked(&size);
          ReadAndAppendAccessUnitChecked(kAccessUnitTypeVideo);
          break;
      }
    }
  }

 private:
  void ReadAndAppendAccessUnitChecked(AccessUnitType access_unit_type) {
    int64_t timestamp;
    ReadChecked(&timestamp);

    std::vector<uint8_t> key_id, iv;
    ReadChecked(&key_id);
    ReadChecked(&iv);

    uint32_t subsample_count;
    ReadChecked(&subsample_count);

    Subsamples subsamples(subsample_count);
    for (auto& subsample : subsamples) {
      ReadChecked(&subsample.clear_bytes);
      ReadChecked(&subsample.encrypted_bytes);
    }

    std::vector<uint8_t> data;
    ReadChecked(&data);

    access_units_.emplace_back(access_unit_type, timestamp, key_id, iv,
                               subsamples, std::move(data));
  }

  template <typename T>
  bool ReadUnchecked(T* value) {
    if (!value) {
      ReportFatalError();
      return false;
    }

    int bytes_to_read = static_cast<int>(sizeof(*value));
    int bytes_read = Read(value, bytes_to_read);

    if (reverse_byte_order_) {
      std::reverse(reinterpret_cast<uint8_t*>(value),
                   reinterpret_cast<uint8_t*>(value + 1));
    }

    return bytes_to_read == bytes_read;
  }
  template <typename T>
  void ReadChecked(T* value) {
    if (!ReadUnchecked(value)) {
      ReportFatalError();
    }
  }
  void ReadChecked(std::vector<uint8_t>* value) {
    if (!value) {
      ReportFatalError();
    }

    uint32_t size;
    ReadChecked(&size);

    value->resize(size);

    if (size == 0) {
      return;
    }

    int bytes_read = Read(value->data(), size);
    if (bytes_read != size) {
      ReportFatalError();
    }
  }

  bool reverse_byte_order_;
  std::vector<EmeInitData> eme_init_datas_;
  std::vector<AccessUnit> access_units_;
};

#endif  // COBALT_MEDIA_BASE_VIDEO_DMP_READER_H_
