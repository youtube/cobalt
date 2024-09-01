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

#include "starboard/shared/libiamf/iamf_config_reader.h"

#include <algorithm>
#include <string>

#include "starboard/common/string.h"
#include "third_party/libiamf/source/code/include/IAMF_defines.h"

namespace starboard {
namespace shared {
namespace libiamf {

namespace {

// From //media/formats/mp4/rcheck.h.
#define RCHECK(condition)                                                 \
  do {                                                                    \
    if (!(condition)) {                                                   \
      SB_DLOG(ERROR) << "Failure while parsing IAMF config: " #condition; \
      return false;                                                       \
    }                                                                     \
  } while (0)

// From
// https://aomediacodec.github.io/iamf/v1.0.0-errata.html#obu-header-syntax.
constexpr int kObuTypeCodecConfig = 0;
constexpr int kObuTypeAudioElement = 1;
constexpr int kObuTypeMixPresentation = 2;
constexpr int kObuTypeSequenceHeader = 31;

// From
// https://aomediacodec.github.io/iamf/v1.0.0-errata.html#obu-codecconfig.
constexpr int kFourccOpus = 0x4f707573;
constexpr int kFourccIpcm = 0x6970636d;

}  // namespace

IamfConfigReader::BufferReader::BufferReader(const uint8_t* buf, size_t size)
    : buf_(buf), pos_(0), size_(size), error_(false) {}

bool IamfConfigReader::BufferReader::Read1(uint8_t* ptr) {
  if (!HasBytes(sizeof(uint8_t)) || !ptr) {
    error_ = true;
    return false;
  }
  *ptr = buf_[pos_++];
  return true;
}

bool IamfConfigReader::BufferReader::Read4(uint32_t* ptr) {
  if (!HasBytes(sizeof(uint32_t)) || !ptr) {
    error_ = true;
    return false;
  }
  std::memcpy(ptr, &buf_[pos_], sizeof(uint32_t));
  *ptr = ByteSwap(*ptr);
  pos_ += sizeof(uint32_t);
  return true;
}

bool IamfConfigReader::BufferReader::ReadLeb128(uint32_t* ptr) {
  if (!HasBytes(sizeof(uint32_t)) || !ptr) {
    error_ = true;
    return false;
  }
  int bytes_read = ReadLeb128Internal(buf_ + pos_, ptr);
  if (bytes_read < 0) {
    error_ = true;
    return false;
  }
  pos_ += bytes_read;
  return true;
}

bool IamfConfigReader::BufferReader::ReadString(std::string& str) {
  int bytes_read = ReadStringInternal(buf_ + pos_, str);
  if (bytes_read < 0) {
    error_ = true;
    return false;
  }
  pos_ += bytes_read;
  return true;
}

bool IamfConfigReader::BufferReader::SkipBytes(size_t size) {
  if (!HasBytes(size)) {
    error_ = true;
    return false;
  }
  pos_ += size;
  return true;
}

bool IamfConfigReader::BufferReader::SkipLeb128() {
  uint32_t val;
  return ReadLeb128(&val);
}

bool IamfConfigReader::BufferReader::SkipString() {
  std::string str;
  return ReadString(str);
}

int IamfConfigReader::BufferReader::ReadLeb128Internal(const uint8_t* buf,
                                                       uint32_t* value) {
  SB_DCHECK(buf);
  SB_DCHECK(value);
  *value = 0;
  bool error = true;
  size_t i = 0;
  for (; i < sizeof(uint32_t); ++i) {
    uint8_t byte = buf[i];
    *value |= ((byte & 0x7f) << (i * 7));
    if (!(byte & 0x80)) {
      error = false;
      break;
    }
  }

  if (error) {
    return -1;
  }
  return i + 1;
}

int IamfConfigReader::BufferReader::ReadStringInternal(const uint8_t* buf,
                                                       std::string& str) {
  SB_DCHECK(buf);

  int remaining_size = static_cast<int>(size_) - pos_;
  if (remaining_size <= 0) {
    return -1;
  }

  // The size of the string is capped to 128 bytes.
  int max_str_size = std::min(remaining_size, 128);
  str.clear();

  size_t size = 0;
  while (buf[size] != '\0' && size < max_str_size) {
    size++;
  }

  if (buf[size] != '\0') {
    return -1;
  }

  if (size > 0) {
    str.resize(size);
    std::memcpy(str.data(), reinterpret_cast<const char*>(buf), size);
  }

  // Account for null terminator byte.
  return ++size;
}

IamfConfigReader::IamfConfigReader(bool prefer_binaural_audio,
                                   bool prefer_surround_audio)
    : prefer_binaural_audio_(prefer_binaural_audio),
      prefer_surround_audio_(prefer_surround_audio) {
#if SB_IS_BIG_ENDIAN
#error IamfConfigReader assumes little-endianness.
#endif  // SB_IS_BIG_ENDIAN
  SB_DCHECK(!(prefer_binaural_audio && prefer_surround_audio));
}

bool IamfConfigReader::ResetAndRead(scoped_refptr<InputBuffer> input_buffer) {
  Reset();
  return Read(input_buffer);
}

void IamfConfigReader::Reset() {
  mix_presentation_id_ = std::optional<uint32_t>();
  binaural_audio_element_ids_ = std::unordered_set<uint32_t>();
  config_size_ = 0;
  data_size_ = 0;
  sample_rate_ = 0;
  samples_per_buffer_ = 0;
  sample_size_ = 0;
}

bool IamfConfigReader::Read(scoped_refptr<InputBuffer> input_buffer) {
  SB_DCHECK(input_buffer->data());
  BufferReader reader(input_buffer->data(), input_buffer->size());

  while (!is_valid() && reader.pos() < reader.size()) {
    RCHECK(ReadOBU(&reader));
  }

  if (reader.error()) {
    return false;
  }

  data_size_ = reader.size() - config_size_;
  config_obus_.assign(reader.buf(), reader.buf() + config_size_);
  data_.assign(reader.buf() + config_size_, reader.buf() + reader.size());

  return true;
}

bool IamfConfigReader::ReadOBU(BufferReader* reader) {
  SB_DCHECK(reader);
  uint8_t obu_type = 0;
  uint32_t obu_size = 0;
  if (!ReadOBUHeader(reader, &obu_type, &obu_size)) {
    SB_DLOG(ERROR) << "Error reading OBU header";
    return false;
  }

  int next_obu_pos = reader->pos() + obu_size;

  auto skip_param_definition = [&]() {
    // parameter_id
    RCHECK(reader->SkipLeb128());
    // parameter_rate
    RCHECK(reader->SkipLeb128());
    uint8_t param_definition_mode;
    RCHECK(reader->Read1(&param_definition_mode));
    param_definition_mode = param_definition_mode >> 7;
    if (param_definition_mode == static_cast<uint8_t>(0)) {
      // duration
      RCHECK(reader->SkipLeb128());
      uint32_t constant_subblock_duration;
      RCHECK(reader->ReadLeb128(&constant_subblock_duration));
      if (constant_subblock_duration == 0) {
        uint32_t num_subblocks;
        RCHECK(reader->ReadLeb128(&num_subblocks));
        for (int i = 0; i < num_subblocks; ++i) {
          // subblock_duration
          RCHECK(reader->SkipLeb128());
        }
      }
    }
    return true;
  };

  switch (static_cast<int>(obu_type)) {
    case kObuTypeCodecConfig: {
      RCHECK(reader->SkipLeb128());

      uint32_t codec_id = 0;
      RCHECK(reader->Read4(&codec_id));

      RCHECK(reader->ReadLeb128(&samples_per_buffer_));

      // audio_roll_distance
      RCHECK(reader->SkipBytes(2));

      switch (codec_id) {
        case kFourccOpus:
          sample_rate_ = 48000;
          break;
        case kFourccIpcm: {
          // sample_format_flags
          RCHECK(reader->SkipBytes(1));

          uint8_t sample_size_unsigned;
          RCHECK(reader->Read1(&sample_size_unsigned));
          sample_size_ = static_cast<int>(sample_size_unsigned);

          uint32_t sample_rate_unsigned;
          RCHECK(reader->Read4(&sample_rate_unsigned));
          sample_rate_ = static_cast<int>(sample_rate_unsigned);
          break;
        }
        default:
          SB_NOTREACHED();
          return false;
      }

      break;
    }
    case kObuTypeAudioElement: {
      uint32_t audio_element_id;
      RCHECK(reader->ReadLeb128(&audio_element_id));

      uint8_t audio_element_type;
      RCHECK(reader->Read1(&audio_element_type));
      audio_element_type = audio_element_type >> 5;

      // codec_config_id
      RCHECK(reader->SkipLeb128());

      uint32_t num_substreams;
      RCHECK(reader->ReadLeb128(&num_substreams));

      for (int i = 0; i < num_substreams; ++i) {
        // audio_substream_id
        RCHECK(reader->SkipLeb128());
      }

      uint32_t num_parameters;
      RCHECK(reader->ReadLeb128(&num_parameters));

      for (int i = 0; i < num_parameters; ++i) {
        uint32_t param_definition_type;
        RCHECK(reader->ReadLeb128(&param_definition_type));

        if (param_definition_type == IAMF_PARAMETER_TYPE_DEMIXING) {
          skip_param_definition();
          // DemixingParamDefintion
          RCHECK(reader->SkipBytes(1));
        } else if (param_definition_type == IAMF_PARAMETER_TYPE_RECON_GAIN) {
          skip_param_definition();
        } else if (param_definition_type > 2) {
          uint32_t param_definition_size;
          RCHECK(reader->ReadLeb128(&param_definition_size));
          RCHECK(reader->SkipBytes(param_definition_size));
        }
      }

      if (static_cast<uint32_t>(audio_element_type) ==
              AUDIO_ELEMENT_CHANNEL_BASED &&
          (prefer_binaural_audio_ || prefer_surround_audio_)) {
        // Parse ScalableChannelLayoutConfig for binaural and surround
        // loudspeaker layouts
        uint8_t num_layers;
        RCHECK(reader->Read1(&num_layers));
        num_layers = num_layers >> 5;
        // Read ChannelAudioLayerConfigs
        for (int i = 0; i < static_cast<int>(num_layers); ++i) {
          uint8_t loudspeaker_layout;
          bool output_gain_is_present_flag;
          RCHECK(reader->Read1(&loudspeaker_layout));
          output_gain_is_present_flag = (loudspeaker_layout >> 3) & 0x01;
          loudspeaker_layout = loudspeaker_layout >> 4;
          if (loudspeaker_layout == IA_CHANNEL_LAYOUT_BINAURAL) {
            binaural_audio_element_ids_.insert(audio_element_id);
          } else if (loudspeaker_layout > IA_CHANNEL_LAYOUT_STEREO &&
                     loudspeaker_layout < IA_CHANNEL_LAYOUT_COUNT) {
            surround_audio_element_ids_.insert(audio_element_id);
          }

          // substream_count and coupled_substream_count
          RCHECK(reader->SkipBytes(2));

          if (output_gain_is_present_flag) {
            // output_gain_flags and output_gain
            RCHECK(reader->SkipBytes(3));
          }

          if (i == 1 && loudspeaker_layout == static_cast<uint8_t>(15)) {
            // expanded_loudspeaker_layout
            RCHECK(reader->SkipBytes(1));
          }
        }
      }
      break;
    }
    case kObuTypeSequenceHeader:
      break;
    case kObuTypeMixPresentation: {
      uint32_t mix_presentation_id;
      RCHECK(reader->ReadLeb128(&mix_presentation_id));

      uint32_t count_label;
      RCHECK(reader->ReadLeb128(&count_label));
      for (int i = 0; i < count_label; ++i) {
        // language_label;
        RCHECK(reader->SkipString());
      }

      for (int i = 0; i < count_label; ++i) {
        // MixPresentationAnnotations;
        RCHECK(reader->SkipString());
      }

      uint32_t num_sub_mixes;
      RCHECK(reader->ReadLeb128(&num_sub_mixes));
      for (int i = 0; i < num_sub_mixes; ++i) {
        uint32_t num_audio_elements;
        RCHECK(reader->ReadLeb128(&num_audio_elements));
        for (int j = 0; j < num_audio_elements; ++j) {
          uint32_t audio_element_id;
          RCHECK(reader->ReadLeb128(&audio_element_id));

          // Set a mix presentation for binaural or surround streams. The mix
          // presentation is chosen if there exists an audio element that has
          // the qualities it requires - such as an audio element with a
          // binaural loudspeaker layout.
          if (!mix_presentation_id_.has_value() ||
              (prefer_binaural_audio_ &&
               binaural_mix_selection_ >
                   kBinauralMixSelectionLoudspeakerLayout)) {
            if (prefer_binaural_audio_ &&
                binaural_audio_element_ids_.find(audio_element_id) !=
                    binaural_audio_element_ids_.end()) {
              mix_presentation_id_ = mix_presentation_id;
              binaural_mix_selection_ = kBinauralMixSelectionLoudspeakerLayout;
            } else if (prefer_surround_audio_ &&
                       surround_audio_element_ids_.find(audio_element_id) !=
                           surround_audio_element_ids_.end()) {
              mix_presentation_id_ = mix_presentation_id;
            }
          }

          for (int k = 0; k < count_label; ++k) {
            // MixPresentationElementAnnotatoions
            RCHECK(reader->SkipString());
          }

          // The following fields are for the RenderingConfig
          // headphones_rendering_mode
          RCHECK(reader->SkipBytes(1));
          uint32_t rendering_config_extension_size;
          RCHECK(reader->ReadLeb128(&rendering_config_extension_size));
          // rendering_config_extension_bytes
          RCHECK(reader->SkipBytes(rendering_config_extension_size));

          // The following fields are for the ElementMixConfig
          RCHECK(skip_param_definition());
          // default_mix_gain
          RCHECK(reader->SkipBytes(2));
        }
        // The following fields are for the OutputMixConfig
        RCHECK(skip_param_definition());
        // default_mix_gain
        RCHECK(reader->SkipBytes(2));

        uint32_t num_layouts;
        RCHECK(reader->ReadLeb128(&num_layouts));
        for (int j = 0; j < num_layouts; ++j) {
          uint8_t layout_type;
          RCHECK(reader->Read1(&layout_type));
          layout_type = layout_type >> 6;
          // If a binaural mix presentation is preferred and the mix
          // presentation id has not yet been set, set the mix presentation id
          // if the current mix presentation has a binaural loudness layout. The
          // mix presentation id will change if a different mix presentation is
          // found that uses an audio element with a binaural loudspeaker
          // layout, as that is higher priority.
          if (static_cast<uint32_t>(layout_type) == IAMF_LAYOUT_TYPE_BINAURAL &&
              prefer_binaural_audio_ &&
              (!mix_presentation_id_.has_value() ||
               binaural_mix_selection_ > kBinauralMixSelectionLoudnessLayout)) {
            mix_presentation_id_ = mix_presentation_id;
            binaural_mix_selection_ = kBinauralMixSelectionLoudnessLayout;
          }

          // The following fields are for the LoudnessInfo
          uint8_t info_type;
          RCHECK(reader->Read1(&info_type));
          // integrated_loudness and digital_loudness
          RCHECK(reader->SkipBytes(4));
          if (info_type & 1) {
            // true_peak
            RCHECK(reader->SkipBytes(2));
          }
          if (info_type & 2) {
            uint8_t num_anchored_loudness;
            RCHECK(reader->Read1(&num_anchored_loudness));
            for (uint8_t k = 0; k < num_anchored_loudness; ++k) {
              // anchor_element and anchored_loudness
              RCHECK(reader->SkipBytes(3));
            }
          }
          if ((info_type & 0b11111100) > 0) {
            uint32_t info_type_size;
            RCHECK(reader->ReadLeb128(&info_type_size));
            // info_type_bytes
            RCHECK(reader->SkipBytes(info_type_size));
          }
        }
      }

      // If the mix presentation id is unassigned at this point, the stream is
      // stereo, or a proper mix presentation for binaural or surround preferred
      // streams hasn't yet been parsed. Default to the first read mix
      // presentation in case a preferred mix does not exist.
      if (!mix_presentation_id_.has_value()) {
        mix_presentation_id_ = mix_presentation_id;
      }

      break;
    }
    default:
      // Once an OBU is read that is not a descriptor, descriptor parsing is
      // assumed to be complete.
      SB_DCHECK(mix_presentation_id_.has_value());
      return true;
  }

  // Skip to the next OBU.
  const size_t remaining_size = next_obu_pos - reader->pos();
  RCHECK(reader->SkipBytes(remaining_size));
  config_size_ = reader->pos();
  return true;
}

bool IamfConfigReader::ReadOBUHeader(BufferReader* reader,
                                     uint8_t* obu_type,
                                     uint32_t* obu_size) {
  uint8_t header_flags;
  RCHECK(reader->Read1(&header_flags));
  *obu_type = (header_flags >> 3) & 0x1f;

  const bool obu_redundant_copy = (header_flags >> 2) & 1;
  const bool obu_trimming_status_flag = (header_flags >> 1) & 1;
  const bool obu_extension_flag = header_flags & 1;

  *obu_size = 0;

  RCHECK(reader->ReadLeb128(obu_size));

  // |obu_size| contains the size of the OBU after its own field.
  // If either of the flags are set, subtract the number of bytes read
  // from the flags from |obu_size| before returning to ReadOBU().
  size_t reader_pos_before_flags = reader->pos();

  if (obu_trimming_status_flag) {
    RCHECK(reader->SkipLeb128());
    RCHECK(reader->SkipLeb128());
  }

  if (obu_extension_flag) {
    RCHECK(reader->SkipLeb128());
  }

  size_t flag_bytes_read = reader->pos() - reader_pos_before_flags;
  if (flag_bytes_read >= *obu_size) {
    return false;
  }
  *obu_size -= flag_bytes_read;
  return true;
}

}  // namespace libiamf
}  // namespace shared
}  // namespace starboard
