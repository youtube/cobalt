/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef OBU_RENDERING_CONFIG_H_
#define OBU_RENDERING_CONFIG_H_

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/element_gain_offset_config.h"
#include "iamf/obu/param_definitions/cart16_param_definition.h"
#include "iamf/obu/param_definitions/cart8_param_definition.h"
#include "iamf/obu/param_definitions/dual_cart16_param_definition.h"
#include "iamf/obu/param_definitions/dual_cart8_param_definition.h"
#include "iamf/obu/param_definitions/dual_polar_param_definition.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/param_definitions/polar_param_definition.h"

namespace iamf_tools {

using PositionParamVariant =
    std::variant<PolarParamDefinition, Cart8ParamDefinition,
                 Cart16ParamDefinition, DualPolarParamDefinition,
                 DualCart8ParamDefinition, DualCart16ParamDefinition>;

struct RenderingConfigParamDefinition {
  friend bool operator==(const RenderingConfigParamDefinition& lhs,
                         const RenderingConfigParamDefinition& rhs) = default;

  // Use `Create` or `CreateFromBuffer` instead.
  RenderingConfigParamDefinition() = delete;

  /*!\brief Move constructor. */
  RenderingConfigParamDefinition(RenderingConfigParamDefinition&& other) =
      default;

  /*!\brief Copy constructor. */
  RenderingConfigParamDefinition(const RenderingConfigParamDefinition& other) =
      default;

  /*!\brief Copy assignment operator. */
  RenderingConfigParamDefinition& operator=(
      const RenderingConfigParamDefinition& other) = default;

  /*!\brief Creates an `RenderingConfigParamDefinition` from a buffer.
   *
   * \param rb Buffer to read from.
   *
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  static absl::StatusOr<RenderingConfigParamDefinition> CreateFromBuffer(
      ReadBitBuffer& rb);

  /*!\brief Creates an `RenderingConfigParamDefinition` from the given
   * parameters.
   *
   * \param param_definition Param definition.
   * \param param_definition_bytes Param definition bytes.
   *
   * \return Created `RenderingConfigParamDefinition`.
   */
  static RenderingConfigParamDefinition Create(
      PositionParamVariant param_definition,
      const std::vector<uint8_t>& param_definition_bytes);

  ParamDefinition::ParameterDefinitionType param_definition_type;
  PositionParamVariant param_definition;
  // `param_definition_bytes_size` is inferred from the size of
  // `param_definition_bytes`.
  std::vector<uint8_t> param_definition_bytes;

 private:
  // Private constructor. Use `Create` or `CreateFromBuffer` instead.
  RenderingConfigParamDefinition(
      ParamDefinition::ParameterDefinitionType param_definition_type,
      PositionParamVariant param_definition,
      std::vector<uint8_t> param_definition_bytes);
};

struct RenderingConfig {
 public:
  /*!\brief A 2-bit enum describing how to render the content to headphones. */
  enum HeadphonesRenderingMode : uint8_t {
    kHeadphonesRenderingModeStereo = 0,
    kHeadphonesRenderingModeBinauralWorldLocked = 1,
    kHeadphonesRenderingModeBinauralHeadLocked = 2,
    kHeadphonesRenderingModeReserved3 = 3,
  };

  /*!\brief A 2-bit enum indicating the binaural filter profile. */
  enum BinauralFilterProfile : uint8_t {
    kBinauralFilterProfileAmbient = 0,
    kBinauralFilterProfileDirect = 1,
    kBinauralFilterProfileReverberant = 2,
    kBinauralFilterProfileReserved3 = 3,
  };

  friend bool operator==(const RenderingConfig& lhs,
                         const RenderingConfig& rhs) = default;

  /*!\brief Creates a `RenderingConfig` from a buffer.
   *
   * \param rb Buffer to read from.
   *
   * \return `RenderingConfig` if successful. A status on failure.
   */
  static absl::StatusOr<RenderingConfig> CreateFromBuffer(ReadBitBuffer& rb);

  /*!\brief Writes the `RenderingConfig` to a buffer.
   *
   * \param wb Buffer to write to.
   *
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  /*!\brief Prints the `RenderingConfig`. */
  void Print() const;

  HeadphonesRenderingMode headphones_rendering_mode =
      kHeadphonesRenderingModeStereo;  // 2 bits.

  // `element_gain_offset_flag` (1 bit) is implicit based on the presence of
  // `element_gain_offset_config`.

  // TODO(b/476923149): Implement the logic to render binaurally using the
  //                    specified filter profile.
  BinauralFilterProfile binaural_filter_profile =
      kBinauralFilterProfileAmbient;  // 2 bits.

  uint8_t reserved = 0;  // 3 bits.

  // `num_parameters` is implicit based on the size of
  // `rendering_config_param_definitions`.
  std::vector<RenderingConfigParamDefinition>
      rendering_config_param_definitions;

  std::optional<ElementGainOffsetConfig> element_gain_offset_config;

  // `rendering_config_extension_size` is inferred from the length of
  // `rendering_config_extension_bytes`.
  std::vector<uint8_t> rendering_config_extension_bytes;
};

}  // namespace iamf_tools

#endif  // OBU_RENDERING_CONFIG_H_
