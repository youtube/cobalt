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
#include "iamf/obu/element_gain_offset_config.h"

#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/common/q_format_or_floating_point.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

using ::absl::MakeConstSpan;
using ::absl::MakeSpan;

namespace {

enum class ElementGainOffsetConfigType : uint8_t {
  kValueType = 0,
  kRangeType = 1
};

}  // namespace

absl::Status ElementGainOffsetConfig::ValueType::Write(
    WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(
      static_cast<uint32_t>(ElementGainOffsetConfigType::kValueType), 8));
  return wb.WriteSigned16(element_gain_offset.GetQ7_8());
}

absl::Status ElementGainOffsetConfig::RangeType::Write(
    WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(
      static_cast<uint32_t>(ElementGainOffsetConfigType::kRangeType), 8));
  RETURN_IF_NOT_OK(wb.WriteSigned16(default_element_gain_offset.GetQ7_8()));
  RETURN_IF_NOT_OK(wb.WriteSigned16(min_element_gain_offset.GetQ7_8()));
  return wb.WriteSigned16(max_element_gain_offset.GetQ7_8());
}

absl::Status ElementGainOffsetConfig::ExtensionType::Write(
    WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(element_gain_offset_config_type, 8));
  RETURN_IF_NOT_OK(wb.WriteUleb128(element_gain_offset_bytes.size()));
  return wb.WriteUint8Span(MakeConstSpan(element_gain_offset_bytes));
}

void ElementGainOffsetConfig::ValueType::Print() const {
  ABSL_LOG(INFO) << "value_type: " << element_gain_offset;
}

void ElementGainOffsetConfig::RangeType::Print() const {
  ABSL_LOG(INFO) << "range_type: ";
  ABSL_LOG(INFO) << "  default_element_gain_offset= "
                 << default_element_gain_offset;
  ABSL_LOG(INFO) << "  min_element_gain_offset= " << min_element_gain_offset;
  ABSL_LOG(INFO) << "  max_element_gain_offset= " << max_element_gain_offset;
}

void ElementGainOffsetConfig::ExtensionType::Print() const {
  ABSL_LOG(INFO) << "element_gain_offset_config_type: "
                 << element_gain_offset_config_type;
  ABSL_LOG(INFO) << "  element_gain_offset_bytes size: "
                 << element_gain_offset_bytes.size();
  ABSL_LOG(INFO) << "  (element_gain_offset_bytes omitted)";
}

ElementGainOffsetConfig ElementGainOffsetConfig::MakeValueType(
    QFormatOrFloatingPoint element_gain_offset) {
  // Nothing can fail here.
  return ElementGainOffsetConfig(
      ValueType{.element_gain_offset = element_gain_offset});
}

absl::StatusOr<ElementGainOffsetConfig>
ElementGainOffsetConfig::CreateRangeType(
    QFormatOrFloatingPoint default_element_gain_offset,
    QFormatOrFloatingPoint min_element_gain_offset,
    QFormatOrFloatingPoint max_element_gain_offset) {
  // Check that the range is valid, and the default is within the range.
  RETURN_IF_NOT_OK(ValidateInRange(
      default_element_gain_offset.GetQ7_8(),
      {min_element_gain_offset.GetQ7_8(), max_element_gain_offset.GetQ7_8()},
      "default_element_gain_offset"));

  return ElementGainOffsetConfig(RangeType{
      .default_element_gain_offset = default_element_gain_offset,
      .min_element_gain_offset = min_element_gain_offset,
      .max_element_gain_offset = max_element_gain_offset,
  });
}

absl::StatusOr<ElementGainOffsetConfig>
ElementGainOffsetConfig::CreateExtensionType(
    uint8_t element_gain_offset_config_type,
    absl::Span<const uint8_t> element_gain_offset_bytes) {
  if (element_gain_offset_config_type ==
          static_cast<uint8_t>(ElementGainOffsetConfigType::kValueType) ||
      element_gain_offset_config_type ==
          static_cast<uint8_t>(ElementGainOffsetConfigType::kRangeType)) {
    return absl::InvalidArgumentError(
        "Call the specific factory function for value and range types.");
  }

  return ElementGainOffsetConfig(ExtensionType{
      .element_gain_offset_config_type = element_gain_offset_config_type,
      .element_gain_offset_bytes = std::vector<uint8_t>(
          element_gain_offset_bytes.begin(), element_gain_offset_bytes.end()),
  });
}

absl::StatusOr<ElementGainOffsetConfig>
ElementGainOffsetConfig::CreateFromBuffer(ReadBitBuffer& rb) {
  uint8_t element_gain_offset_config_type;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, element_gain_offset_config_type));
  if (element_gain_offset_config_type ==
      static_cast<uint8_t>(ElementGainOffsetConfigType::kValueType)) {
    int16_t element_gain_offset_q78;
    RETURN_IF_NOT_OK(rb.ReadSigned16(element_gain_offset_q78));
    return MakeValueType(
        QFormatOrFloatingPoint::MakeFromQ7_8(element_gain_offset_q78));
  } else if (element_gain_offset_config_type ==
             static_cast<uint8_t>(ElementGainOffsetConfigType::kRangeType)) {
    int16_t default_element_gain_offset_q78;
    int16_t min_element_gain_offset_q78;
    int16_t max_element_gain_offset_q78;
    RETURN_IF_NOT_OK(rb.ReadSigned16(default_element_gain_offset_q78));
    RETURN_IF_NOT_OK(rb.ReadSigned16(min_element_gain_offset_q78));
    RETURN_IF_NOT_OK(rb.ReadSigned16(max_element_gain_offset_q78));
    return CreateRangeType(
        QFormatOrFloatingPoint::MakeFromQ7_8(default_element_gain_offset_q78),
        QFormatOrFloatingPoint::MakeFromQ7_8(min_element_gain_offset_q78),
        QFormatOrFloatingPoint::MakeFromQ7_8(max_element_gain_offset_q78));
  }

  // Else it is an extension type.
  uint32_t element_gain_offset_size;
  RETURN_IF_NOT_OK(rb.ReadULeb128(element_gain_offset_size));
  if (element_gain_offset_size > kEntireObuSizeMaxTwoMegabytes) {
    return absl::InvalidArgumentError(
        absl::StrCat("element_gain_offset_size= ", element_gain_offset_size,
                     " exceeds maximum."));
  }
  std::vector<uint8_t> element_gain_offset_bytes(element_gain_offset_size);
  RETURN_IF_NOT_OK(rb.ReadUint8Span(MakeSpan(element_gain_offset_bytes)));
  return CreateExtensionType(element_gain_offset_config_type,
                             element_gain_offset_bytes);
}

absl::Status ElementGainOffsetConfig::Write(WriteBitBuffer& wb) const {
  return std::visit(
      [&wb](const auto& data) -> absl::Status { return data.Write(wb); },
      element_gain_offset_config_data_);
}

void ElementGainOffsetConfig::Print() const {
  std::visit([](const auto& data) { data.Print(); },
             element_gain_offset_config_data_);
}

}  // namespace iamf_tools
