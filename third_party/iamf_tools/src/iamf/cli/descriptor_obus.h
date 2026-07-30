/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef CLI_DESCRIPTOR_OBUS_H_
#define CLI_DESCRIPTOR_OBUS_H_

#include <list>

#include "absl/container/node_hash_map.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Collection of parsed OBUs.
 *
 * OBUs that are commonly pointed to are held via `absl::node_hash_map` for
 * pointer stability. For example, `AudioElementWithData` contains a pointer to
 * the corresponding `CodecConfigObu`. This container ensures this type is
 * move-safe.
 */
struct DescriptorObus {
  /*!\brief Map `CodecConfigOBU`s keyed by codec config ID.
   *
   * The IAMF specification has a `codec_config_id` field which is unique to
   * each (non-redundant) Codec Config OBU. This map is keyed by that field for
   * easy lookup.
   *
   * Held in a `node_hash_map` for pointer stability.
   */
  using CodecConfigsById = absl::node_hash_map<DecodedUleb128, CodecConfigObu>;

  /*!\brief Map of `AudioElementWithData` keyed by audio element ID.
   *
   * The IAMF specification has an `audio_element_id` field which is unique to
   * each (non-redundant) Audio Element OBU. This map is keyed by that field for
   * easy lookup.
   *
   * Held in a `node_hash_map` for pointer stability.
   */
  using AudioElementsById =
      absl::node_hash_map<DecodedUleb128, AudioElementWithData>;

  /*!\brief List of Mix Presentation OBUs.
   *
   * The IAMF specification allows several mix presentations to be included in
   * a bitstream.
   *
   * The specification uses the order within the bitstream to help determine
   * which one will be used for rendering. This type holds them in a list, which
   * helps preserve their order.
   */
  using MixPresentationObus = std::list<MixPresentationObu>;

  /*!\brief Default constructor. */
  DescriptorObus() = default;

  // IA sequence header processed from the bitstream.
  IASequenceHeaderObu ia_sequence_header;
  // Map of Codec Config OBUs processed from the bitstream.
  CodecConfigsById codec_config_obus;
  // Map of Audio Elements and metadata processed from the bitstream.
  AudioElementsById audio_elements;
  // List of Mix Presentation OBUs processed from the bitstream.
  MixPresentationObus mix_presentation_obus;
};

}  // namespace iamf_tools

#endif  // CLI_DESCRIPTOR_OBUS_H_
