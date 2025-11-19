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

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "third_party/libiamf/source/code/include/IAMF_defines.h"

namespace starboard {

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
  BufferReader(const uint8_t* buf, size_t size) : buf_(buf), size_(size) {
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
    if (!HasBytes(1) || !ptr) {
      return false;
    }
    int bytes_read = ReadLeb128Internal(
        buf_ + pos_, ptr,
        std::min(static_cast<uint64_t>(RemainingSize()), sizeof(uint32_t)));
    if (bytes_read < 0) {
      return false;
    }
    pos_ += bytes_read;
    return true;
  }

  bool ReadString(std::string* str) {
    if (!HasBytes(1) || !str) {
      return false;
    }

    // The size of the string is capped to 128 bytes.
    // https://aomediacodec.github.io/iamf/v1.0.0-errata.html#convention-data-types
    const int kMaxIamfStringSize = 128;
    int bytes_read = ReadStringInternal(
        buf_ + pos_, str, std::min(RemainingSize(), kMaxIamfStringSize));
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
  int RemainingSize() const { return size_ - pos_; }
  inline uint32_t ByteSwap(uint32_t x) const {
#if defined(COMPILER_MSVC)
    return _byteswap_ulong(x);
#else
    return __builtin_bswap32(x);
#endif
  }

  // Decodes an Leb128 value and stores it in |value|. Returns the number of
  // bytes read, capped to |max_bytes_to_read|. Returns the number of bytes
  // read, or -1 on error.
  static int ReadLeb128Internal(const uint8_t* buf,
                                uint32_t* value,
                                const int max_bytes_to_read) {
    SB_DCHECK(buf);
    SB_DCHECK(value);

    *value = 0;
    bool error = true;
    size_t i = 0;
    for (; i < max_bytes_to_read; ++i) {
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
  static int ReadStringInternal(const uint8_t* buf,
                                std::string* str,
                                int max_bytes_to_read) {
    SB_DCHECK(buf);
    SB_DCHECK(str);

    str->clear();
    str->resize(max_bytes_to_read);

    int bytes_read = ::starboard::strlcpy(
        str->data(), reinterpret_cast<const char*>(buf), max_bytes_to_read);
    if (bytes_read == max_bytes_to_read) {
      // Ensure that the string is null terminated.
      if (buf[bytes_read] != '\0') {
        return -1;
      }
    }
    str->resize(bytes_read);

    // Account for null terminator.
    return ++bytes_read;
  }

  const uint8_t* buf_;
  const size_t size_;
  int pos_ = 0;
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

// Parses an IAMF OBU header.
// https://aomediacodec.github.io/iamf/v1.0.0-errata.html#obu-header-syntax
bool ParseOBUHeader(BufferReader* reader,
                    uint8_t* obu_type,
                    uint32_t* obu_size) {
  uint8_t header_flags;
  RCHECK(reader->Read1(&header_flags));
  *obu_type = (header_flags >> 3) & 0x1f;

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

// Parses an IAMF Config OBU for the substream sample rate. This only handles
// Opus and ipcm substreams, FLAC and AAC substreams are unsupported.
// https://aomediacodec.github.io/iamf/v1.0.0-errata.html#obu-codecconfig
bool ParseCodecConfigOBU(BufferReader* reader, IamfBufferInfo* info) {
  RCHECK(reader->SkipLeb128());

  uint32_t codec_id = 0;
  RCHECK(reader->Read4(&codec_id));

  RCHECK(reader->ReadLeb128(&info->num_samples));

  // audio_roll_distance
  RCHECK(reader->SkipBytes(2));

  switch (codec_id) {
    case kFourccOpus:
      // The Opus sample rate is always 48kHz.
      // https://aomediacodec.github.io/iamf/v1.0.0-errata.html#opus-specific
      info->sample_rate = 48000;
      break;
    case kFourccIpcm: {
      // sample_format_flags
      RCHECK(reader->SkipBytes(1));

      // sample_size
      RCHECK(reader->SkipBytes(1));

      RCHECK(reader->Read4(reinterpret_cast<uint32_t*>(&info->sample_rate)));
      break;
    }
    default:
      SB_NOTREACHED();
      return false;
  }

  return true;
}

// Checks if the Audio Element indicated by |audio_element_id| contains layouts
// for binaural and surround configurations, and sets
// |binaural_audio_element_id| and |surround_audio_element_id| if so.
bool CheckForAdvancedAudioElements(
    BufferReader* reader,
    uint32_t audio_element_id,
    std::optional<uint32_t>* binaural_audio_element_id,
    std::optional<uint32_t>* surround_audio_element_id) {
  // Parse ScalableChannelLayoutConfig for binaural and surround
  // loudspeaker layouts.
  uint8_t num_layers;
  RCHECK(reader->Read1(&num_layers));
  num_layers = num_layers >> 5;
  // Read ChannelAudioLayerConfigs.
  uint8_t max_loudspeaker_layout = 0;
  for (int i = 0; i < static_cast<int>(num_layers); ++i) {
    uint8_t loudspeaker_layout;
    bool output_gain_is_present_flag;
    RCHECK(reader->Read1(&loudspeaker_layout));
    output_gain_is_present_flag = (loudspeaker_layout >> 3) & 0x01;
    loudspeaker_layout = loudspeaker_layout >> 4;
    if (loudspeaker_layout == IA_CHANNEL_LAYOUT_BINAURAL &&
        !binaural_audio_element_id->has_value()) {
      *binaural_audio_element_id = audio_element_id;
    } else if (loudspeaker_layout > IA_CHANNEL_LAYOUT_STEREO &&
               loudspeaker_layout < IA_CHANNEL_LAYOUT_COUNT &&
               loudspeaker_layout > max_loudspeaker_layout) {
      *surround_audio_element_id = audio_element_id;
      max_loudspeaker_layout = loudspeaker_layout;
    }

    // substream_count and coupled_substream_count.
    RCHECK(reader->SkipBytes(2));

    if (output_gain_is_present_flag) {
      // output_gain_flags and output_gain.
      RCHECK(reader->SkipBytes(3));
    }

    if (i == 1 && loudspeaker_layout == static_cast<uint8_t>(15)) {
      // expanded_loudspeaker_layout.
      RCHECK(reader->SkipBytes(1));
    }
  }
  return true;
}

// Parses an IAMF Audio Element OBU for audio element ids. When
// |prefer_binaural_audio| is true, |binaural_audio_element_id| is set to
// the first audioelement id containing a binaural loudspeaker layout.
// The same is done for |surround_audio_element_id| when
// |prefer_surround_audio| is true.
// https://aomediacodec.github.io/iamf/v1.0.0-errata.html#obu-audioelement
bool ParseAudioElementOBU(BufferReader* reader,
                          std::optional<uint32_t>* binaural_audio_element_id,
                          std::optional<uint32_t>* surround_audio_element_id,
                          const bool prefer_binaural_audio,
                          const bool prefer_surround_audio) {
  uint32_t audio_element_id;
  RCHECK(reader->ReadLeb128(&audio_element_id));

  uint8_t audio_element_type;
  RCHECK(reader->Read1(&audio_element_type));
  audio_element_type = audio_element_type >> 5;

  // codec_config_id.
  RCHECK(reader->SkipLeb128());

  uint32_t num_substreams;
  RCHECK(reader->ReadLeb128(&num_substreams));

  for (int i = 0; i < num_substreams; ++i) {
    // audio_substream_id.
    RCHECK(reader->SkipLeb128());
  }

  uint32_t num_parameters;
  RCHECK(reader->ReadLeb128(&num_parameters));

  for (int i = 0; i < num_parameters; ++i) {
    uint32_t param_definition_type;
    RCHECK(reader->ReadLeb128(&param_definition_type));

    if (param_definition_type == IAMF_PARAMETER_TYPE_DEMIXING) {
      RCHECK(SkipParamDefinition(reader));
      // DemixingParamDefintion.
      RCHECK(reader->SkipBytes(1));
    } else if (param_definition_type == IAMF_PARAMETER_TYPE_RECON_GAIN) {
      RCHECK(SkipParamDefinition(reader));
    } else if (param_definition_type > IAMF_PARAMETER_TYPE_RECON_GAIN) {
      uint32_t param_definition_size;
      RCHECK(reader->ReadLeb128(&param_definition_size));
      RCHECK(reader->SkipBytes(param_definition_size));
    }
  }

  if (static_cast<uint32_t>(audio_element_type) ==
          AUDIO_ELEMENT_CHANNEL_BASED &&
      (prefer_binaural_audio || prefer_surround_audio)) {
    RCHECK(CheckForAdvancedAudioElements(reader, audio_element_id,
                                         binaural_audio_element_id,
                                         surround_audio_element_id));
  }
  return true;
}

// Parses a Mix Presentation OBU for a mix presentation to be used when
// configuring the IAMF decoder.
// When |prefer_surround_audio| is true, |info->mix_presentation_id| is
// set to |surround_audio_element_id|, if it has a value.
// When |prefer_binaural_audio| is true, |info->mix_presentation_id| is
// set to |binaural_audio_element_id|, if it has a value. If
// |binaural_audio_element_id| is unset, |info->mix_presentation_id| will be set
// to a mix presentation containing a binaural loudness layout.
// If a matching audio element wasn't found, or if a stereo layout is requested,
// |info->mix_presentation_id| is set to the first read mix presentation
// by default.
// https://aomediacodec.github.io/iamf/v1.0.0-errata.html#obu-mixpresentation
bool ParseMixPresentationOBU(
    BufferReader* reader,
    IamfBufferInfo* info,
    const std::optional<uint32_t>* binaural_audio_element_id,
    const std::optional<uint32_t>* surround_audio_element_id,
    const bool prefer_binaural_audio,
    const bool prefer_surround_audio) {
  uint32_t mix_presentation_id;
  RCHECK(reader->ReadLeb128(&mix_presentation_id));

  uint32_t count_label;
  RCHECK(reader->ReadLeb128(&count_label));
  for (int i = 0; i < count_label; ++i) {
    // language_label.
    RCHECK(reader->SkipString());
  }

  for (int i = 0; i < count_label; ++i) {
    // MixPresentationAnnotations.
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
      // presentation is chosen if an audio element exists that has
      // the qualities it requires - such as an audio element with a
      // binaural loudspeaker layout.
      if (!info->mix_presentation_id.has_value() ||
          (prefer_binaural_audio &&
           binaural_mix_selection != kBinauralMixSelectionLoudspeakerLayout)) {
        if (prefer_binaural_audio &&
            (binaural_audio_element_id->has_value() &&
             binaural_audio_element_id->value() == audio_element_id)) {
          info->mix_presentation_id = mix_presentation_id;
          binaural_mix_selection = kBinauralMixSelectionLoudspeakerLayout;
        } else if (prefer_surround_audio &&
                   (surround_audio_element_id->has_value() &&
                    surround_audio_element_id->value() == audio_element_id)) {
          info->mix_presentation_id = mix_presentation_id;
        }
      }

      for (int k = 0; k < count_label; ++k) {
        // MixPresentationElementAnnotatoions.
        RCHECK(reader->SkipString());
      }

      // The following fields are for the RenderingConfig
      // headphones_rendering_mode.
      RCHECK(reader->SkipBytes(1));
      uint32_t rendering_config_extension_size;
      RCHECK(reader->ReadLeb128(&rendering_config_extension_size));
      // rendering_config_extension_bytes.
      RCHECK(reader->SkipBytes(rendering_config_extension_size));

      // The following fields are for the ElementMixConfig.
      RCHECK(SkipParamDefinition(reader));
      // default_mix_gain.
      RCHECK(reader->SkipBytes(2));
    }

    // The following fields are for the OutputMixConfig.
    RCHECK(SkipParamDefinition(reader));
    // default_mix_gain.
    RCHECK(reader->SkipBytes(2));

    uint32_t num_layouts;
    RCHECK(reader->ReadLeb128(&num_layouts));
    for (int j = 0; j < num_layouts; ++j) {
      uint8_t layout_type;
      RCHECK(reader->Read1(&layout_type));
      layout_type = layout_type >> 6;
      // Set the mix presentation ID for a binaural loudness layout. This mix
      // presentation ID will be overridden if a different mix presentation has
      // a binarual loudspeaker layout, as that is prioritized.
      // https://aomediacodec.github.io/iamf/v1.0.0-errata.html#processing-mixpresentation-selection
      if (static_cast<uint32_t>(layout_type) == IAMF_LAYOUT_TYPE_BINAURAL &&
          prefer_binaural_audio &&
          (!info->mix_presentation_id.has_value() ||
           binaural_mix_selection == kBinauralMixSelectionNotFound)) {
        info->mix_presentation_id = mix_presentation_id;
        binaural_mix_selection = kBinauralMixSelectionLoudnessLayout;
      }

      // The following fields are for the LoudnessInfo.
      uint8_t info_type;
      RCHECK(reader->Read1(&info_type));
      // integrated_loudness and digital_loudness.
      RCHECK(reader->SkipBytes(4));
      if (info_type & 1) {
        // true_peak.
        RCHECK(reader->SkipBytes(2));
      }
      if (info_type & 2) {
        uint8_t num_anchored_loudness;
        RCHECK(reader->Read1(&num_anchored_loudness));
        for (uint8_t k = 0; k < num_anchored_loudness; ++k) {
          // anchor_element and anchored_loudness.
          RCHECK(reader->SkipBytes(3));
        }
      }
      if ((info_type & 0b11111100) > 0) {
        uint32_t info_type_size;
        RCHECK(reader->ReadLeb128(&info_type_size));
        // info_type_bytes.
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

  std::optional<uint32_t> binaural_audio_element_id;
  std::optional<uint32_t> surround_audio_element_id;
  switch (static_cast<int>(obu_type)) {
    case kObuTypeCodecConfig:
      RCHECK(ParseCodecConfigOBU(reader, info));
      break;
    case kObuTypeAudioElement:
      RCHECK(ParseAudioElementOBU(
          reader, &binaural_audio_element_id, &surround_audio_element_id,
          prefer_binaural_audio, prefer_surround_audio));
      break;
    case kObuTypeSequenceHeader:
      break;
    case kObuTypeMixPresentation:
      RCHECK(ParseMixPresentationOBU(
          reader, info, &binaural_audio_element_id, &surround_audio_element_id,
          prefer_binaural_audio, prefer_surround_audio));
      break;
    default:
      // This executes if a non descriptor OBU is read. The buffer info must be
      // valid by this point.
      SB_DCHECK(info->is_valid());
      return info->is_valid();
  }

  // Skip to the next OBU.
  const size_t remaining_size = next_obu_pos - reader->pos();
  RCHECK(reader->SkipBytes(remaining_size));
  info->config_obus_size = reader->pos();
  return true;
}

}  // namespace

bool IamfBufferInfo::is_valid() const {
  return mix_presentation_id.has_value() && sample_rate > 0 && num_samples > 0;
}

bool ParseInputBuffer(const scoped_refptr<InputBuffer>& input_buffer,
                      IamfBufferInfo* info,
                      const bool prefer_binaural_audio,
                      const bool prefer_surround_audio) {
  SB_DCHECK(info);
  SB_DCHECK(input_buffer->data());
  SB_DCHECK(!(prefer_binaural_audio && prefer_surround_audio));

  BufferReader reader(input_buffer->data(), input_buffer->size());

  while (!info->is_valid() && reader.pos() < reader.size()) {
    RCHECK(ParseDescriptorOBU(&reader, info, prefer_binaural_audio,
                              prefer_surround_audio));
  }
  RCHECK(info->is_valid());

  info->config_obus.assign(reader.buf(), reader.buf() + info->config_obus_size);
  info->data.assign(reader.buf() + info->config_obus_size,
                    reader.buf() + reader.size());

  return true;
}

}  // namespace starboard
