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

#ifndef STARBOARD_SHARED_LIBIAMF_IAMF_BUFFER_PARSER_H_
#define STARBOARD_SHARED_LIBIAMF_IAMF_BUFFER_PARSER_H_

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

class BufferReader;

// TODO: Skip parsing if the OBUs are redundant
class IamfBufferParser {
 public:
  typedef ::starboard::shared::starboard::player::InputBuffer InputBuffer;

  struct IamfBufferInfo {
    bool is_valid() const {
      return mix_presentation_id.has_value() && sample_rate > 0 &&
             num_samples > 0;
    }

    uint32_t num_samples;
    int sample_rate;
    std::optional<uint32_t> mix_presentation_id;
    std::vector<uint8_t> config_obus;
    size_t config_obus_size;
    std::vector<uint8_t> data;
    size_t data_size;
    const scoped_refptr<InputBuffer> input_buffer;
  };

  IamfBufferParser();

  bool ParseInputBuffer(const scoped_refptr<InputBuffer>& input_buffer,
                        IamfBufferInfo* info,
                        const bool prefer_binaural_audio,
                        const bool prefer_surround_audio);

 private:
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

  bool ParseInputBufferInternal(const scoped_refptr<InputBuffer>& input_buffer,
                                IamfBufferInfo* info,
                                const bool prefer_binaural_audio,
                                const bool prefer_surround_audio);
  // Reads a single Descriptor OBU. Returns false on error.
  bool ParseDescriptorOBU(BufferReader* reader,
                          IamfBufferInfo* info,
                          const bool prefer_binaural_audio,
                          const bool prefer_surround_audio);
  bool ParseOBUHeader(BufferReader* reader,
                      uint8_t* obu_type,
                      uint32_t* obu_size) const;
  bool ParseCodecConfigOBU(BufferReader* reader, IamfBufferInfo* info);
  bool ParseAudioElementOBU(BufferReader* reader,
                            IamfBufferInfo* info,
                            const bool prefer_binaural_audio,
                            const bool prefer_surround_audio);
  bool ParseMixPresentationOBU(BufferReader* reader,
                               IamfBufferInfo* info,
                               const bool prefer_binaural_audio,
                               const bool prefer_surround_audio);
  // Helper function to skip parsing ParamDefinitions found in the config OBUs
  // https://aomediacodec.github.io/iamf/v1.0.0-errata.html#paramdefinition
  bool SkipParamDefinition(BufferReader* reader) const;

  std::unordered_set<uint32_t> binaural_audio_element_ids_;
  std::unordered_set<uint32_t> surround_audio_element_ids_;

  BinauralMixSelection binaural_mix_selection_ = kBinauralMixSelectionNotFound;
};

}  // namespace libiamf
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_LIBIAMF_IAMF_BUFFER_PARSER_H_
