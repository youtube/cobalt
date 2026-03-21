// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
#include <utility>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/shared/libiamf/iamf_buffer_reader.h"
// TODO (b/494691786): Add libiamf to //third_party.
// #include "third_party/libiamf/source/code/include/IAMF_defines.h"

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

// Helper macro to unpack an optional value into a variable or return false.
#define ASSIGN_OR_RETURN_FALSE(lhs, optional_rhs)                            \
  do {                                                                       \
    auto&& opt = (optional_rhs);                                             \
    if (!opt.has_value()) {                                                  \
      SB_DLOG(ERROR) << "Failure while parsing IAMF config: " #optional_rhs; \
      return false;                                                          \
    }                                                                        \
    lhs = *std::move(opt);                                                   \
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

// Helper function to skip parsing ParamDefinitions found in the config OBUs
// https://aomediacodec.github.io/iamf/v1.0.0-errata.html#paramdefinition
bool SkipParamDefinition(IamfBufferReader* reader) {
  // parameter_id
  RCHECK(reader->ReadLeb128().has_value());
  // parameter_rate
  RCHECK(reader->ReadLeb128().has_value());
  uint8_t param_definition_mode;
  ASSIGN_OR_RETURN_FALSE(param_definition_mode, reader->Read1());
  param_definition_mode = param_definition_mode >> 7;
  if (param_definition_mode == static_cast<uint8_t>(0)) {
    // duration
    RCHECK(reader->ReadLeb128().has_value());
    uint32_t constant_subblock_duration;
    ASSIGN_OR_RETURN_FALSE(constant_subblock_duration, reader->ReadLeb128());
    if (constant_subblock_duration == 0) {
      uint32_t num_subblocks;
      ASSIGN_OR_RETURN_FALSE(num_subblocks, reader->ReadLeb128());
      for (int i = 0; i < num_subblocks; ++i) {
        // subblock_duration
        RCHECK(reader->ReadLeb128().has_value());
      }
    }
  }
  return true;
}

// Parses ParamDefinition sections from various OBUs.
// https://aomediacodec.github.io/iamf/v1.0.0-errata.html#paramdefinition
bool ParseParamDefinitions(IamfBufferReader* reader) {
  uint32_t num_parameters;
  ASSIGN_OR_RETURN_FALSE(num_parameters, reader->ReadLeb128());

  for (int i = 0; i < num_parameters; ++i) {
    uint32_t param_definition_type;
    ASSIGN_OR_RETURN_FALSE(param_definition_type, reader->ReadLeb128());

    if (param_definition_type == IAMF_PARAMETER_TYPE_DEMIXING) {
      RCHECK(SkipParamDefinition(reader));
      // DemixingParamDefintion.
      RCHECK(reader->Skip(1));
    } else if (param_definition_type == IAMF_PARAMETER_TYPE_RECON_GAIN) {
      RCHECK(SkipParamDefinition(reader));
    } else if (param_definition_type > IAMF_PARAMETER_TYPE_RECON_GAIN) {
      uint32_t param_definition_size;
      ASSIGN_OR_RETURN_FALSE(param_definition_size, reader->ReadLeb128());
      RCHECK(reader->Skip(param_definition_size));
    }
  }
  return true;
}

// Parses an IAMF OBU header.
// https://aomediacodec.github.io/iamf/v1.0.0-errata.html#obu-header-syntax
bool ParseOBUHeader(IamfBufferReader* reader,
                    uint8_t* obu_type,
                    uint32_t* obu_size) {
  uint8_t header_flags;
  ASSIGN_OR_RETURN_FALSE(header_flags, reader->Read1());
  *obu_type = (header_flags >> 3) & 0x1f;

  const bool obu_trimming_status_flag = (header_flags >> 1) & 1;
  const bool obu_extension_flag = header_flags & 1;

  ASSIGN_OR_RETURN_FALSE(*obu_size, reader->ReadLeb128());

  // |obu_size| contains the size of the OBU after its own field.
  // If either of the flags are set, subtract the number of bytes read
  // from the flags from |obu_size| before returning to ParseDescriptorOBU().
  size_t reader_pos_before_flags = reader->pos();

  if (obu_trimming_status_flag) {
    RCHECK(reader->ReadLeb128().has_value());
    RCHECK(reader->ReadLeb128().has_value());
  }

  if (obu_extension_flag) {
    RCHECK(reader->ReadLeb128().has_value());
  }

  size_t flag_bytes_read = reader->pos() - reader_pos_before_flags;
  if (flag_bytes_read > *obu_size) {
    return false;
  }
  *obu_size -= flag_bytes_read;
  return true;
}

// Parses an IAMF Config OBU for the substream sample rate. This only handles
// Opus and ipcm substreams, FLAC and AAC substreams are unsupported.
// https://aomediacodec.github.io/iamf/v1.0.0-errata.html#obu-codecconfig
bool ParseCodecConfigOBU(IamfBufferReader* reader, IamfBufferInfo* info) {
  RCHECK(reader->ReadLeb128().has_value());

  uint32_t codec_id = 0;
  ASSIGN_OR_RETURN_FALSE(codec_id, reader->Read4());

  ASSIGN_OR_RETURN_FALSE(info->num_samples, reader->ReadLeb128());

  // audio_roll_distance
  RCHECK(reader->Skip(2));

  switch (codec_id) {
    case kFourccOpus:
      // The Opus sample rate is always 48kHz.
      // https://aomediacodec.github.io/iamf/v1.0.0-errata.html#opus-specific
      info->sample_rate = 48000;
      break;
    case kFourccIpcm: {
      // sample_format_flags
      RCHECK(reader->Skip(1));

      // sample_size
      RCHECK(reader->Skip(1));

      uint32_t sample_rate;
      ASSIGN_OR_RETURN_FALSE(sample_rate, reader->Read4());
      info->sample_rate = sample_rate;
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
    IamfBufferReader* reader,
    uint32_t audio_element_id,
    std::optional<uint32_t>* binaural_audio_element_id,
    std::optional<uint32_t>* surround_audio_element_id) {
  // Parse ScalableChannelLayoutConfig for binaural and surround
  // loudspeaker layouts.
  uint8_t num_layers;
  ASSIGN_OR_RETURN_FALSE(num_layers, reader->Read1());
  num_layers = num_layers >> 5;
  // Read ChannelAudioLayerConfigs.
  uint8_t max_loudspeaker_layout = 0;
  for (int i = 0; i < static_cast<int>(num_layers); ++i) {
    uint8_t loudspeaker_layout_byte;
    ASSIGN_OR_RETURN_FALSE(loudspeaker_layout_byte, reader->Read1());
    bool output_gain_is_present_flag = (loudspeaker_layout_byte >> 3) & 0x01;
    uint8_t loudspeaker_layout = loudspeaker_layout_byte >> 4;
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
    RCHECK(reader->Skip(2));

    if (output_gain_is_present_flag) {
      // output_gain_flags and output_gain.
      RCHECK(reader->Skip(3));
    }

    if (i == 1 && loudspeaker_layout == static_cast<uint8_t>(15)) {
      // expanded_loudspeaker_layout.
      RCHECK(reader->Skip(1));
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
bool ParseAudioElementOBU(IamfBufferReader* reader,
                          std::optional<uint32_t>* binaural_audio_element_id,
                          std::optional<uint32_t>* surround_audio_element_id,
                          const bool prefer_binaural_audio,
                          const bool prefer_surround_audio) {
  uint32_t audio_element_id;
  ASSIGN_OR_RETURN_FALSE(audio_element_id, reader->ReadLeb128());

  uint8_t audio_element_type;
  ASSIGN_OR_RETURN_FALSE(audio_element_type, reader->Read1());
  audio_element_type = audio_element_type >> 5;

  // codec_config_id.
  RCHECK(reader->ReadLeb128().has_value());

  uint32_t num_substreams;
  ASSIGN_OR_RETURN_FALSE(num_substreams, reader->ReadLeb128());

  for (int i = 0; i < num_substreams; ++i) {
    // audio_substream_id.
    RCHECK(reader->ReadLeb128().has_value());
  }

  RCHECK(ParseParamDefinitions(reader));

  if (static_cast<uint32_t>(audio_element_type) ==
          AUDIO_ELEMENT_CHANNEL_BASED &&
      (prefer_binaural_audio || prefer_surround_audio)) {
    RCHECK(CheckForAdvancedAudioElements(reader, audio_element_id,
                                         binaural_audio_element_id,
                                         surround_audio_element_id));
  }
  return true;
}

// Parses a LoudnessInfo block from a Mix Presentation OBU.
// https://aomediacodec.github.io/iamf/v1.0.0-errata.html#loudness-info-syntax
bool ParseLoudnessInfo(IamfBufferReader* reader) {
  uint8_t info_type;
  ASSIGN_OR_RETURN_FALSE(info_type, reader->Read1());
  // integrated_loudness and digital_loudness.
  RCHECK(reader->Skip(4));
  if (info_type & 1) {
    // true_peak.
    RCHECK(reader->Skip(2));
  }
  if (info_type & 2) {
    uint8_t num_anchored_loudness;
    ASSIGN_OR_RETURN_FALSE(num_anchored_loudness, reader->Read1());
    for (uint8_t k = 0; k < num_anchored_loudness; ++k) {
      // anchor_element and anchored_loudness.
      RCHECK(reader->Skip(3));
    }
  }
  if ((info_type & 0b11111100) > 0) {
    uint32_t info_type_size;
    ASSIGN_OR_RETURN_FALSE(info_type_size, reader->ReadLeb128());
    // info_type_bytes.
    RCHECK(reader->Skip(info_type_size));
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
    IamfBufferReader* reader,
    IamfBufferInfo* info,
    const std::optional<uint32_t>* binaural_audio_element_id,
    const std::optional<uint32_t>* surround_audio_element_id,
    const bool prefer_binaural_audio,
    const bool prefer_surround_audio) {
  uint32_t mix_presentation_id;
  ASSIGN_OR_RETURN_FALSE(mix_presentation_id, reader->ReadLeb128());

  uint32_t count_label;
  ASSIGN_OR_RETURN_FALSE(count_label, reader->ReadLeb128());
  for (int i = 0; i < count_label; ++i) {
    // language_label.
    RCHECK(reader->ReadString().has_value());
  }

  for (int i = 0; i < count_label; ++i) {
    // MixPresentationAnnotations.
    RCHECK(reader->ReadString().has_value());
  }

  uint32_t num_sub_mixes;
  BinauralMixSelection binaural_mix_selection = kBinauralMixSelectionNotFound;
  ASSIGN_OR_RETURN_FALSE(num_sub_mixes, reader->ReadLeb128());
  for (int i = 0; i < num_sub_mixes; ++i) {
    uint32_t num_audio_elements;
    ASSIGN_OR_RETURN_FALSE(num_audio_elements, reader->ReadLeb128());
    for (int j = 0; j < num_audio_elements; ++j) {
      uint32_t audio_element_id;
      ASSIGN_OR_RETURN_FALSE(audio_element_id, reader->ReadLeb128());

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
        RCHECK(reader->ReadString().has_value());
      }

      // The following fields are for the RenderingConfig
      // headphones_rendering_mode.
      RCHECK(reader->Skip(1));
      uint32_t rendering_config_extension_size;
      ASSIGN_OR_RETURN_FALSE(rendering_config_extension_size,
                             reader->ReadLeb128());
      // rendering_config_extension_bytes.
      RCHECK(reader->Skip(rendering_config_extension_size));

      // The following fields are for the ElementMixConfig.
      RCHECK(SkipParamDefinition(reader));
      // default_mix_gain.
      RCHECK(reader->Skip(2));
    }

    // The following fields are for the OutputMixConfig.
    RCHECK(SkipParamDefinition(reader));
    // default_mix_gain.
    RCHECK(reader->Skip(2));

    uint32_t num_layouts;
    ASSIGN_OR_RETURN_FALSE(num_layouts, reader->ReadLeb128());
    for (int j = 0; j < num_layouts; ++j) {
      uint8_t layout_type;
      ASSIGN_OR_RETURN_FALSE(layout_type, reader->Read1());
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

      RCHECK(ParseLoudnessInfo(reader));
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

bool ParseDescriptorOBU(IamfBufferReader* reader,
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

  const size_t next_obu_pos = reader->pos() + obu_size;
  RCHECK(next_obu_pos <= reader->size());

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
  RCHECK(reader->Skip(remaining_size));
  info->config_obus_size = reader->pos();
  return true;
}

}  // namespace

bool IamfBufferInfo::is_valid() const {
  return mix_presentation_id.has_value() && sample_rate > 0 &&
         num_samples > 0 && input_buffer;
}

bool ParseInputBuffer(const scoped_refptr<InputBuffer>& input_buffer,
                      IamfBufferInfo* info,
                      const bool prefer_binaural_audio,
                      const bool prefer_surround_audio) {
  SB_DCHECK(info);
  SB_DCHECK(input_buffer->data());
  SB_DCHECK(!(prefer_binaural_audio && prefer_surround_audio));

  IamfBufferReader reader(input_buffer->data(), input_buffer->size());
  info->input_buffer = input_buffer;

  while (!info->is_valid() && reader.pos() < reader.size()) {
    RCHECK(ParseDescriptorOBU(&reader, info, prefer_binaural_audio,
                              prefer_surround_audio));
  }
  RCHECK(info->is_valid());

  info->config_obus = reader.buf();
  // config_obus_size is set inside ParseDescriptorOBU.
  info->data = reader.buf() + info->config_obus_size;
  info->data_size = reader.size() - info->config_obus_size;

  return true;
}

}  // namespace starboard
