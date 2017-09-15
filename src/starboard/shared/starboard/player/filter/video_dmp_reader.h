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

// TODO: Move this into Starboard

// This file is deliberately not using any Cobalt/Starboard specific API so it
// can be used in an independent application.
#ifndef COBALT_MEDIA_BASE_VIDEO_DMP_READER_H_
#define COBALT_MEDIA_BASE_VIDEO_DMP_READER_H_

#include <string>
#include <vector>

// File: <BOM> <Record>*
//   BOM: 0x76543210
//   Record: <4 bytes fourcc type> + <4 bytes size> + <|size| bytes binary data>
//
//     audio config:
//       fourcc type: 'acfg'
//       2 bytes audio codec type in SbMediaAudioCodec
//       2 bytes format_tag
//       2 bytes number_of_channels
//       4 bytes samples_per_second
//       4 bytes average_bytes_per_second
//       2 bytes block_alignment
//       2 bytes bits_per_sample
//       2 bytes audio_specific_config_size
//       |audio_specific_config_size| bytes audio specific config
//
//     video config:
//       fourcc type: 'vcfg'
//       2 bytes video codec type in SbMediaVideoCodec
//
//     eme init data
//       fourcc type: 'emei'
//       4 bytes size
//         |size| bytes eme init data
//     audio/video access unit;
//       fourcc type: 'adat'/'vdat'
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
    kRecordTypeAudioConfig = 'acfg',
    kRecordTypeVideoConfig = 'vcfg',
    kRecordTypeEmeInitData = 'emei',
    kRecordTypeAudioAccessUnit = 'adat',
    kRecordTypeVideoAccessUnit = 'vdat',
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

  void Parse();

 private:
  bool reverse_byte_order_;
  std::vector<EmeInitData> eme_init_datas_;
  std::vector<AccessUnit> access_units_;
};

#endif  // COBALT_MEDIA_BASE_VIDEO_DMP_READER_H_
