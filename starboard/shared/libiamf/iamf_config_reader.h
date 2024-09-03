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

#ifndef STARBOARD_SHARED_LIBIAMF_IAMF_CONFIG_READER_H_
#define STARBOARD_SHARED_LIBIAMF_IAMF_CONFIG_READER_H_

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/ref_counted.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"

namespace starboard {
namespace shared {
namespace libiamf {

// TODO: Add handling for non-redundant OBUs
class IamfConfigReader {
 public:
  typedef ::starboard::shared::starboard::player::InputBuffer InputBuffer;

  IamfConfigReader(const scoped_refptr<InputBuffer>& input_buffer,
                   bool prefer_binaural_audio,
                   bool prefer_surround_audio);

  bool is_valid() const {
    return mix_presentation_id_.has_value() && sample_rate_ > 0 &&
           samples_per_buffer_ > 0;
  }
  int sample_rate() const { return sample_rate_; }
  uint32_t samples_per_buffer() const { return samples_per_buffer_; }
  uint32_t config_size() const { return config_size_; }
  uint32_t mix_presentation_id() const {
    SB_DCHECK(is_valid())
        << "Called mix_presentation_id() on invalid IamfConfigReader.";
    if (prefer_binaural_audio_ &&
        binaural_mix_selection_ == kBinauralMixSelectionNotFound) {
      SB_LOG(INFO) << "Could not find binaural mix presentation.";
    }
    return mix_presentation_id_.value();
  }

  const std::vector<uint8_t>& config_obus() const { return config_obus_; }
  const std::vector<uint8_t>& data() const { return data_; }

 private:
  class BufferReader {
   public:
    BufferReader(const uint8_t* buf, size_t size);

    bool Read1(uint8_t* ptr);
    bool Read4(uint32_t* ptr);
    bool ReadLeb128(uint32_t* ptr);
    bool ReadString(std::string* str);

    bool SkipBytes(size_t size);
    bool SkipLeb128();
    bool SkipString();

    size_t size() const { return size_; }
    size_t pos() const { return pos_; }
    const uint8_t* buf() const { return buf_; }
    bool error() const { return error_; }

   private:
    bool HasBytes(size_t size) const { return size + pos_ <= size_; }
    inline uint32_t ByteSwap(uint32_t x) {
#if defined(COMPILER_MSVC)
      return _byteswap_ulong(x);
#else
      return __builtin_bswap32(x);
#endif
    }
    // Decodes an Leb128 value and stores it in |value|. Returns the number of
    // bytes read, capped to sizeof(uint32_t). Returns the number of bytes read,
    // or -1 on error.
    int ReadLeb128Internal(const uint8_t* buf, uint32_t* value);
    // Reads a c-string into |str|. Returns the number of bytes read, capped to
    // 128 bytes, or -1 on error.
    int ReadStringInternal(const uint8_t* buf, std::string* str);

    int pos_ = 0;
    const uint8_t* buf_;
    const size_t size_ = 0;
    bool error_ = false;
  };

  // Used in the selection of a binaural mix presentation, using the strategy
  // defined in
  // https://aomediacodec.github.io/iamf/#processing-mixpresentation-selection.
  // The preferred methods of choosing a binaural mix presentation are listed
  // from high to low.
  enum BinauralMixSelection {
    kBinauralMixSelectionLoudspeakerLayout,
    kBinauralMixSelectionLoudnessLayout,
    kBinauralMixSelectionNotFound
  };

  bool Read(const scoped_refptr<InputBuffer>& input_buffer);
  // Reads a single Descriptor OBU. Returns false on error.
  bool ReadOBU(BufferReader* reader);
  bool ReadOBUHeader(BufferReader* reader,
                     uint8_t* obu_type,
                     uint32_t* obu_size);

  int sample_rate_ = 0;
  int sample_size_ = 0;
  uint32_t samples_per_buffer_ = 0;
  uint32_t config_size_ = 0;
  uint32_t data_size_ = 0;

  std::optional<uint32_t> mix_presentation_id_;
  std::unordered_set<uint32_t> binaural_audio_element_ids_;
  std::unordered_set<uint32_t> surround_audio_element_ids_;
  const bool prefer_binaural_audio_;
  const bool prefer_surround_audio_;

  BinauralMixSelection binaural_mix_selection_ = kBinauralMixSelectionNotFound;

  std::vector<uint8_t> config_obus_;
  std::vector<uint8_t> data_;
};

}  // namespace libiamf
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_LIBIAMF_IAMF_CONFIG_READER_H_
