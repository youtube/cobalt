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
#include "iamf/obu/rendering_config.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/element_gain_offset_config.h"
#include "iamf/obu/param_definitions/cart16_param_definition.h"
#include "iamf/obu/param_definitions/cart8_param_definition.h"
#include "iamf/obu/param_definitions/dual_cart16_param_definition.h"
#include "iamf/obu/param_definitions/dual_cart8_param_definition.h"
#include "iamf/obu/param_definitions/dual_polar_param_definition.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/param_definitions/polar_param_definition.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

absl::Status WriteRenderingConfigParamDefinition(
    const RenderingConfigParamDefinition& rendering_config_param_definition,
    WriteBitBuffer& wb) {
  RETURN_IF_NOT_OK(
      wb.WriteUleb128(rendering_config_param_definition.param_definition_type));
  switch (rendering_config_param_definition.param_definition_type) {
    case ParamDefinition::ParameterDefinitionType::kParameterDefinitionPolar:
      RETURN_IF_NOT_OK(std::get<PolarParamDefinition>(
                           rendering_config_param_definition.param_definition)
                           .ValidateAndWrite(wb));
      break;
    case ParamDefinition::ParameterDefinitionType::kParameterDefinitionCart8:
      RETURN_IF_NOT_OK(std::get<Cart8ParamDefinition>(
                           rendering_config_param_definition.param_definition)
                           .ValidateAndWrite(wb));
      break;
    case ParamDefinition::ParameterDefinitionType::kParameterDefinitionCart16:
      RETURN_IF_NOT_OK(std::get<Cart16ParamDefinition>(
                           rendering_config_param_definition.param_definition)
                           .ValidateAndWrite(wb));
      break;
    case ParamDefinition::ParameterDefinitionType::
        kParameterDefinitionDualPolar:
      RETURN_IF_NOT_OK(std::get<DualPolarParamDefinition>(
                           rendering_config_param_definition.param_definition)
                           .ValidateAndWrite(wb));
      break;
    case ParamDefinition::ParameterDefinitionType::
        kParameterDefinitionDualCart8:
      RETURN_IF_NOT_OK(std::get<DualCart8ParamDefinition>(
                           rendering_config_param_definition.param_definition)
                           .ValidateAndWrite(wb));
      break;
    case ParamDefinition::ParameterDefinitionType::
        kParameterDefinitionDualCart16:
      RETURN_IF_NOT_OK(std::get<DualCart16ParamDefinition>(
                           rendering_config_param_definition.param_definition)
                           .ValidateAndWrite(wb));
      break;
    default:
      RETURN_IF_NOT_OK(wb.WriteUleb128(
          rendering_config_param_definition.param_definition_bytes.size()));
      RETURN_IF_NOT_OK(wb.WriteUint8Span(absl::MakeConstSpan(
          rendering_config_param_definition.param_definition_bytes)));
  }
  return absl::OkStatus();
}

absl::Status WriteRenderingConfigParamDefinitions(
    const std::vector<RenderingConfigParamDefinition>&
        rendering_config_param_definitions,
    WriteBitBuffer& wb) {
  // Write `num_parameters`.
  RETURN_IF_NOT_OK(wb.WriteUleb128(rendering_config_param_definitions.size()));
  for (const auto& rendering_config_param_definition :
       rendering_config_param_definitions) {
    RETURN_IF_NOT_OK(WriteRenderingConfigParamDefinition(
        rendering_config_param_definition, wb));
  }
  return absl::OkStatus();
}

absl::Status WriteRenderingConfigExtension(
    const std::vector<RenderingConfigParamDefinition>&
        rendering_config_param_definitions,
    const std::optional<ElementGainOffsetConfig>& element_gain_offset_config,
    const std::vector<uint8_t>& rendering_config_extension_bytes,
    WriteBitBuffer& wb) {
  // Allocate a temporary buffer to write the rendering config param
  // definitions to.
  static const int64_t kInitialBufferSize = 1024;
  WriteBitBuffer temp_wb(kInitialBufferSize, wb.leb_generator_);
  // Write the rendering config param definitions to the temporary buffer.
  RETURN_IF_NOT_OK(WriteRenderingConfigParamDefinitions(
      rendering_config_param_definitions, temp_wb));
  if (element_gain_offset_config.has_value()) {
    RETURN_IF_NOT_OK(element_gain_offset_config->Write(temp_wb));
  }
  RETURN_IF_NOT_OK(temp_wb.WriteUint8Span(
      absl::MakeConstSpan(rendering_config_extension_bytes)));
  ABSL_CHECK(temp_wb.IsByteAligned());
  // Write the header now that the payload size is known.
  const int64_t rendering_config_extension_size = temp_wb.bit_buffer().size();
  RETURN_IF_NOT_OK(wb.WriteUleb128(rendering_config_extension_size));
  // Copy over rendering config param definitions into the actual write
  // buffer.
  return wb.WriteUint8Span(absl::MakeConstSpan(temp_wb.bit_buffer()));
}

absl::Status ReadRenderingConfigExtension(
    bool element_gain_offset_flag, ReadBitBuffer& rb,
    std::vector<RenderingConfigParamDefinition>&
        output_rendering_config_param_definitions,
    std::optional<ElementGainOffsetConfig>& output_element_gain_offset_config) {
  DecodedUleb128 num_params;
  RETURN_IF_NOT_OK(rb.ReadULeb128(num_params));
  for (DecodedUleb128 i = 0; i < num_params; ++i) {
    auto rendering_config_param_definition =
        RenderingConfigParamDefinition::CreateFromBuffer(rb);
    if (!rendering_config_param_definition.ok()) {
      return rendering_config_param_definition.status();
    }
    output_rendering_config_param_definitions.push_back(
        std::move(*rendering_config_param_definition));
  }

  if (element_gain_offset_flag) {
    auto read_element_gain_offset_config =
        ElementGainOffsetConfig::CreateFromBuffer(rb);
    if (!read_element_gain_offset_config.ok()) {
      return read_element_gain_offset_config.status();
    }
    output_element_gain_offset_config =
        *std::move(read_element_gain_offset_config);
  }

  return absl::OkStatus();
}

}  // namespace

RenderingConfigParamDefinition::RenderingConfigParamDefinition(
    ParamDefinition::ParameterDefinitionType param_definition_type,
    PositionParamVariant param_definition,
    std::vector<uint8_t> param_definition_bytes)
    : param_definition_type(param_definition_type),
      param_definition(std::move(param_definition)),
      param_definition_bytes(std::move(param_definition_bytes)) {}

RenderingConfigParamDefinition RenderingConfigParamDefinition::Create(
    PositionParamVariant param_definition,
    const std::vector<uint8_t>& param_definition_bytes) {
  std::optional<ParamDefinition::ParameterDefinitionType>
      param_definition_type = std::visit(
          [](auto&& param_definition) { return param_definition.GetType(); },
          param_definition);
  // `PositionParamVariants` are well-defined and always have a param definition
  // type. The `nullopt` case is only for "extensions".
  ABSL_CHECK(param_definition_type.has_value());
  return RenderingConfigParamDefinition(*param_definition_type,
                                        std::move(param_definition),
                                        std::move(param_definition_bytes));
}

absl::StatusOr<RenderingConfigParamDefinition>
RenderingConfigParamDefinition::CreateFromBuffer(ReadBitBuffer& rb) {
  DecodedUleb128 param_definition_type;
  RETURN_IF_NOT_OK(rb.ReadULeb128(param_definition_type));
  PositionParamVariant param_definition =
      PolarParamDefinition(ParamDefinition::BaseArgs{});
  switch (static_cast<ParamDefinition::ParameterDefinitionType>(
      param_definition_type)) {
    using enum ParamDefinition::ParameterDefinitionType;
    case kParameterDefinitionPolar:
      RETURN_IF_NOT_OK(
          param_definition
              .emplace<PolarParamDefinition>(ParamDefinition::BaseArgs{})
              .ReadAndValidate(rb));
      break;
    case kParameterDefinitionCart8:
      RETURN_IF_NOT_OK(
          param_definition
              .emplace<Cart8ParamDefinition>(ParamDefinition::BaseArgs{})
              .ReadAndValidate(rb));
      break;
    case kParameterDefinitionCart16:
      RETURN_IF_NOT_OK(
          param_definition
              .emplace<Cart16ParamDefinition>(ParamDefinition::BaseArgs{})
              .ReadAndValidate(rb));
      break;
    case kParameterDefinitionDualPolar:
      RETURN_IF_NOT_OK(
          param_definition
              .emplace<DualPolarParamDefinition>(ParamDefinition::BaseArgs{})
              .ReadAndValidate(rb));
      break;
    case kParameterDefinitionDualCart8:
      RETURN_IF_NOT_OK(
          param_definition
              .emplace<DualCart8ParamDefinition>(ParamDefinition::BaseArgs{})
              .ReadAndValidate(rb));
      break;
    case kParameterDefinitionDualCart16:
      RETURN_IF_NOT_OK(
          param_definition
              .emplace<DualCart16ParamDefinition>(ParamDefinition::BaseArgs{})
              .ReadAndValidate(rb));
      break;
    default:
      // Only positional parameters are defined and directly supported. Others
      // are bypassed.
      DecodedUleb128 param_definition_bytes_size;
      RETURN_IF_NOT_OK(rb.ReadULeb128(param_definition_bytes_size));
      RETURN_IF_NOT_OK(rb.IgnoreBytes(param_definition_bytes_size));
      return absl::UnimplementedError(absl::StrCat(
          "Unsupported param definition type: ", param_definition_type));
  }
  return RenderingConfigParamDefinition::Create(std::move(param_definition),
                                                {});
}

absl::StatusOr<RenderingConfig> RenderingConfig::CreateFromBuffer(
    ReadBitBuffer& rb) {
  uint8_t headphones_rendering_mode;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(2, headphones_rendering_mode));
  HeadphonesRenderingMode headphones_rendering_mode_enum =
      static_cast<HeadphonesRenderingMode>(headphones_rendering_mode);

  bool element_gain_offset_flag;
  RETURN_IF_NOT_OK(rb.ReadBoolean(element_gain_offset_flag));

  uint8_t binaural_filter_profile;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(2, binaural_filter_profile));
  BinauralFilterProfile binaural_filter_profile_enum =
      static_cast<BinauralFilterProfile>(binaural_filter_profile);

  uint8_t reserved;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(3, reserved));
  DecodedUleb128 rendering_config_extension_size;
  RETURN_IF_NOT_OK(rb.ReadULeb128(rendering_config_extension_size));
  if (rendering_config_extension_size > kEntireObuSizeMaxTwoMegabytes) {
    return absl::InvalidArgumentError(absl::StrCat(
        "rendering_config_extension_size= ", rendering_config_extension_size,
        " exceeds maximum."));
  }
  if (rendering_config_extension_size == 0) {
    return RenderingConfig{
        .headphones_rendering_mode = headphones_rendering_mode_enum,
        .binaural_filter_profile = binaural_filter_profile_enum,
        .reserved = reserved};
  }

  const auto position_after_rendering_config_extension_size = rb.Tell();
  // Read in the well-defined fields. These are related to parameters, and
  // (optionally), the element gain offset config.
  std::vector<RenderingConfigParamDefinition>
      rendering_config_param_definitions;
  std::optional<ElementGainOffsetConfig> element_gain_offset_config;
  auto status = ReadRenderingConfigExtension(element_gain_offset_flag, rb,
                                             rendering_config_param_definitions,
                                             element_gain_offset_config);

  int64_t extension_bytes_to_read = rendering_config_extension_size;
  if (status.ok()) {
    extension_bytes_to_read =
        rendering_config_extension_size -
        ((rb.Tell() - position_after_rendering_config_extension_size) / 8);
    if (extension_bytes_to_read < 0) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Expected `rendering_config_extension_size` to be greater than or "
          "equal to the size of `rendering_config_param_definitions`., but "
          "got: ",
          extension_bytes_to_read));
    }
  } else {
    rendering_config_param_definitions.clear();
    element_gain_offset_config = std::nullopt;
    // Failed to read extension, so seek back to the position before reading the
    // extension size so we can read these bytes as generic extension bytes
    // instead.
    RETURN_IF_NOT_OK(rb.Seek(position_after_rendering_config_extension_size));
  }
  std::vector<uint8_t> rendering_config_extension_bytes(
      extension_bytes_to_read);
  RETURN_IF_NOT_OK(
      rb.ReadUint8Span(absl::MakeSpan(rendering_config_extension_bytes)));
  return RenderingConfig{
      .headphones_rendering_mode = headphones_rendering_mode_enum,
      .binaural_filter_profile = binaural_filter_profile_enum,
      .reserved = reserved,
      .rendering_config_param_definitions =
          std::move(rendering_config_param_definitions),
      .element_gain_offset_config = element_gain_offset_config,
      .rendering_config_extension_bytes =
          std::move(rendering_config_extension_bytes)};
}

absl::Status RenderingConfig::ValidateAndWrite(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(
      static_cast<uint8_t>(headphones_rendering_mode), 2));
  const bool element_gain_offset_flag = element_gain_offset_config.has_value();
  RETURN_IF_NOT_OK(wb.WriteBoolean(element_gain_offset_flag));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(
      static_cast<uint8_t>(binaural_filter_profile), 2));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(static_cast<uint8_t>(reserved), 3));

  if (rendering_config_param_definitions.empty() &&
      !element_gain_offset_config.has_value() &&
      rendering_config_extension_bytes.empty()) {
    // TODO(b/468358730): Check if we can remove this branch, without breaking
    //                    compatibility.
    // Older profiles had nothing in the rendering config extension. For maximum
    // backwards compatibility, if both extensions are empty. Write an empty
    // `rendering_config_extension_size`.
    return wb.WriteUleb128(0);
  } else {
    return WriteRenderingConfigExtension(rendering_config_param_definitions,
                                         element_gain_offset_config,
                                         rendering_config_extension_bytes, wb);
  }
}

void RenderingConfig::Print() const {
  ABSL_LOG(INFO) << "        rendering_config:";
  ABSL_LOG(INFO) << "          headphones_rendering_mode= "
                 << absl::StrCat(headphones_rendering_mode);
  ABSL_LOG(INFO) << "          element_gain_offset_flag= "
                 << absl::StrCat(element_gain_offset_config.has_value());
  ABSL_LOG(INFO) << "          binaural_filter_profile= "
                 << absl::StrCat(binaural_filter_profile);
  ABSL_LOG(INFO) << "          reserved= " << absl::StrCat(reserved);
  ABSL_LOG(INFO) << "          rendering_config_extension_size= "
                 << rendering_config_extension_bytes.size();
  ABSL_LOG(INFO) << "          rendering_config_extension_bytes omitted.";
}

}  // namespace iamf_tools
