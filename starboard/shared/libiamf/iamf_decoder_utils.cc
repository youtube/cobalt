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

#include "starboard/shared/libiamf/iamf_decoder_utils.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_set>

#include "starboard/common/log.h"
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

// Used in the selection of a binaural mix presentation, using the strategy
// defined in
// https://aomediacodec.github.io/iamf/#processing-mixpresentation-selection.
// The preferred methods of choosing a binaural mix presentation are listed
// from high to low.
enum BinauralMixSelection {
  kBinauralMixSelectionNotFound,
  kBinauralMixSelectionLoudspeakerLayout,
  kBinauralMixSelectionLoudnessLayout
};

class BufferReader {
 public:
  BufferReader(const uint8_t* buf, size_t size)
      : buf_(buf), pos_(0), size_(size) {
#if SB_IS_BIG_ENDIAN
#error BufferReader assumes little-endianness.
#endif  // SB_IS_BIG_ENDIAN
  }

  bool Read1(uint8_t* ptr) {
    if (!HasBytes(sizeof(uint8_t)) || !ptr) {
      return false;
    }
    *ptr = buf_[pos_++];
    return true;
  }

  bool Read4(uint32_t* ptr) {
    if (!HasBytes(sizeof(uint32_t)) || !ptr) {
      return false;
    }
    std::memcpy(ptr, &buf_[pos_], sizeof(uint32_t));
    *ptr = ByteSwap(*ptr);
    pos_ += sizeof(uint32_t);
    return true;
  }

  bool ReadLeb128(uint32_t* ptr) {
    if (!HasBytes(sizeof(uint32_t)) || !ptr) {
      return false;
    }
    int bytes_read = ReadLeb128Internal(buf_ + pos_, ptr);
    if (bytes_read < 0) {
      return false;
    }
    pos_ += bytes_read;
    return true;
  }

  bool ReadString(std::string* str) {
    int bytes_read = ReadStringInternal(buf_ + pos_, str);
    if (bytes_read < 0) {
      return false;
    }
    pos_ += bytes_read;
    return true;
  }

  bool SkipBytes(size_t size) {
    if (!HasBytes(size)) {
      return false;
    }
    pos_ += size;
    return true;
  }

  bool SkipLeb128() {
    uint32_t val;
    return ReadLeb128(&val);
  }

  bool SkipString() {
    std::string str;
    return ReadString(&str);
  }

  size_t size() const { return size_; }
  size_t pos() const { return pos_; }
  const uint8_t* buf() const { return buf_; }

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
  int ReadLeb128Internal(const uint8_t* buf, uint32_t* value) {
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

  // Reads a c-string into |str|. Returns the number of bytes read, capped to
  // 128 bytes, or -1 on error.
  int ReadStringInternal(const uint8_t* buf, std::string* str) {
    SB_DCHECK(buf);

    int remaining_size = static_cast<int>(size_) - pos_;
    if (remaining_size <= 0) {
      return -1;
    }

    // The size of the string is capped to 128 bytes.
    const int kMaxStringSize = 128;
    int str_size = std::min(remaining_size, kMaxStringSize);
    str->clear();

    size_t bytes_read = 0;
    while (bytes_read < str_size && buf[bytes_read] != '\0') {
      bytes_read++;
    }

    if (bytes_read == str_size) {
      if (buf[bytes_read - 1] != '\0') {
        return -1;
      }
    } else {
      if (buf[bytes_read] != '\0') {
        return -1;
      }
    }

    if (bytes_read > 0) {
      str->resize(bytes_read);
      std::memcpy(str->data(), reinterpret_cast<const char*>(buf), bytes_read);
    }

    // Account for null terminator byte.
    return ++bytes_read;
  }

  int pos_ = 0;
  const uint8_t* buf_;
  const size_t size_ = 0;
};

// Helper function to skip parsing ParamDefinitions found in the config OBUs
// https://aomediacodec.github.io/iamf/v1.0.0-errata.html#paramdefinition
bool SkipParamDefinition(BufferReader* reader) {
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
}

// Helper function to check if |info| contains valid information.
bool BufferInfoIsValid(IamfBufferInfo* info) {
  return info->mix_presentation_id.has_value() && info->sample_rate > 0 &&
         info->num_samples > 0;
}

bool ParseOBUHeader(BufferReader* reader,
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
  // from the flags from |obu_size| before returning to ParseDescriptorOBU().
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

bool ParseCodecConfigOBU(BufferReader* reader, IamfBufferInfo* info) {
  RCHECK(reader->SkipLeb128());

  uint32_t codec_id = 0;
  RCHECK(reader->Read4(&codec_id));

  RCHECK(reader->ReadLeb128(&info->num_samples));

  // audio_roll_distance
  RCHECK(reader->SkipBytes(2));

  const int kOpusSampleRate = 48000;
  switch (codec_id) {
    case kFourccOpus:
      info->sample_rate = kOpusSampleRate;
      break;
    case kFourccIpcm: {
      // sample_format_flags
      RCHECK(reader->SkipBytes(1));

      // sample_size
      RCHECK(reader->SkipBytes(1));

      uint32_t sample_rate_unsigned;
      RCHECK(reader->Read4(&sample_rate_unsigned));
      info->sample_rate = static_cast<int>(sample_rate_unsigned);
      break;
    }
    default:
      SB_NOTREACHED();
      return false;
  }

  return true;
}

bool ParseAudioElementOBU(
    BufferReader* reader,
    IamfBufferInfo* info,
    std::unordered_set<uint32_t>* binaural_audio_element_ids,
    std::unordered_set<uint32_t>* surround_audio_element_ids,
    const bool prefer_binaural_audio,
    const bool prefer_surround_audio) {
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
      SkipParamDefinition(reader);
      // DemixingParamDefintion
      RCHECK(reader->SkipBytes(1));
    } else if (param_definition_type == IAMF_PARAMETER_TYPE_RECON_GAIN) {
      SkipParamDefinition(reader);
    } else if (param_definition_type > 2) {
      uint32_t param_definition_size;
      RCHECK(reader->ReadLeb128(&param_definition_size));
      RCHECK(reader->SkipBytes(param_definition_size));
    }
  }

  if (static_cast<uint32_t>(audio_element_type) ==
          AUDIO_ELEMENT_CHANNEL_BASED &&
      (prefer_binaural_audio || prefer_surround_audio)) {
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
        binaural_audio_element_ids->insert(audio_element_id);
      } else if (loudspeaker_layout > IA_CHANNEL_LAYOUT_STEREO &&
                 loudspeaker_layout < IA_CHANNEL_LAYOUT_COUNT) {
        surround_audio_element_ids->insert(audio_element_id);
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
  return true;
}

bool ParseMixPresentationOBU(
    BufferReader* reader,
    IamfBufferInfo* info,
    std::unordered_set<uint32_t>* binaural_audio_element_ids,
    std::unordered_set<uint32_t>* surround_audio_element_ids,
    const bool prefer_binaural_audio,
    const bool prefer_surround_audio) {
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
  BinauralMixSelection binaural_mix_selection = kBinauralMixSelectionNotFound;
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
      if (!info->mix_presentation_id.has_value() ||
          (prefer_binaural_audio &&
           binaural_mix_selection != kBinauralMixSelectionLoudspeakerLayout)) {
        if (prefer_binaural_audio &&
            binaural_audio_element_ids->find(audio_element_id) !=
                binaural_audio_element_ids->end()) {
          info->mix_presentation_id = mix_presentation_id;
          binaural_mix_selection = kBinauralMixSelectionLoudspeakerLayout;
        } else if (prefer_surround_audio &&
                   surround_audio_element_ids->find(audio_element_id) !=
                       surround_audio_element_ids->end()) {
          info->mix_presentation_id = mix_presentation_id;
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
      SkipParamDefinition(reader);
      // default_mix_gain
      RCHECK(reader->SkipBytes(2));
    }

    // The following fields are for the OutputMixConfig
    SkipParamDefinition(reader);
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
          prefer_binaural_audio &&
          (!info->mix_presentation_id.has_value() ||
           binaural_mix_selection == kBinauralMixSelectionNotFound)) {
        info->mix_presentation_id = mix_presentation_id;
        binaural_mix_selection = kBinauralMixSelectionLoudnessLayout;
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
  if (!info->mix_presentation_id.has_value()) {
    info->mix_presentation_id = mix_presentation_id;
  }

  return true;
}

bool ParseDescriptorOBU(BufferReader* reader,
                        IamfBufferInfo* info,
                        const bool prefer_binaural_audio,
                        const bool prefer_surround_audio) {
  SB_DCHECK(reader);
  uint8_t obu_type = 0;
  uint32_t obu_size = 0;
  if (!ParseOBUHeader(reader, &obu_type, &obu_size)) {
    SB_LOG(ERROR) << "Error parsing OBU header";
    return false;
  }

  int next_obu_pos = reader->pos() + obu_size;

  std::unordered_set<uint32_t> binaural_audio_element_ids;
  std::unordered_set<uint32_t> surround_audio_element_ids;
  switch (static_cast<int>(obu_type)) {
    case kObuTypeCodecConfig:
      RCHECK(ParseCodecConfigOBU(reader, info));
      break;
    case kObuTypeAudioElement:
      RCHECK(ParseAudioElementOBU(reader, info, &binaural_audio_element_ids,
                                  &surround_audio_element_ids,
                                  prefer_binaural_audio,
                                  prefer_surround_audio));
      break;
    case kObuTypeSequenceHeader:
      break;
    case kObuTypeMixPresentation:
      RCHECK(ParseMixPresentationOBU(reader, info, &binaural_audio_element_ids,
                                     &surround_audio_element_ids,
                                     prefer_binaural_audio,
                                     prefer_surround_audio));
      break;
    default:
      // This executes if a non descriptor OBU is read. The buffer info must be
      // valid by this point.
      SB_DCHECK(BufferInfoIsValid(info));
      return true;
  }

  // Skip to the next OBU.
  const size_t remaining_size = next_obu_pos - reader->pos();
  RCHECK(reader->SkipBytes(remaining_size));
  info->config_obus_size = reader->pos();
  return true;
}

}  // namespace

bool ParseInputBuffer(const scoped_refptr<InputBuffer>& input_buffer,
                      IamfBufferInfo* info,
                      const bool prefer_binaural_audio,
                      const bool prefer_surround_audio) {
  SB_DCHECK(info);
  SB_DCHECK(input_buffer->data());
  SB_DCHECK(!(prefer_binaural_audio && prefer_surround_audio));

  BufferReader reader(input_buffer->data(), input_buffer->size());

  while (!BufferInfoIsValid(info) && reader.pos() < reader.size()) {
    RCHECK(ParseDescriptorOBU(&reader, info, prefer_binaural_audio,
                              prefer_surround_audio));
  }
  RCHECK(BufferInfoIsValid(info));

  info->data_size = reader.size() - info->config_obus_size;
  info->config_obus.assign(reader.buf(), reader.buf() + info->config_obus_size);
  info->data.assign(reader.buf() + info->config_obus_size,
                    reader.buf() + reader.size());

  return true;
}

}  // namespace libiamf
}  // namespace shared
}  // namespace starboard
