// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/avc_util.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "starboard/common/check_op.h"

namespace starboard {

namespace {

constexpr uint8_t kAnnexBHeader[] = {0, 0, 0, 1};
constexpr size_t kAnnexBHeaderSizeInBytes = 4;
constexpr uint8_t kIdrStartCode = 0x65;
constexpr uint8_t kSpsStartCode = 0x67;
constexpr uint8_t kPpsStartCode = 0x68;
constexpr uint8_t kAudStartCode = 0x09;

bool StartsWithAnnexBHeader(span<const uint8_t> annex_b_data) {
  static_assert(sizeof(kAnnexBHeader) == kAnnexBHeaderSizeInBytes,
                "sizeof(kAnnexBHeader) doesn't match kAnnexBHeaderSizeInBytes");

  if (annex_b_data.size() < kAnnexBHeaderSizeInBytes) {
    return false;
  }
  return std::equal(std::begin(kAnnexBHeader), std::end(kAnnexBHeader),
                    annex_b_data.begin());
}

// Returns the byte length (including the 4-byte Annex B start code) of the
// first NALU in |annex_b_data|, or std::nullopt if |annex_b_data| does not
// begin with an Annex B header.
std::optional<size_t> GetFirstAnnexBNaluSize(span<const uint8_t> annex_b_data) {
  if (!StartsWithAnnexBHeader(annex_b_data)) {
    return std::nullopt;
  }

  for (size_t offset = kAnnexBHeaderSizeInBytes;
       offset + kAnnexBHeaderSizeInBytes <= annex_b_data.size(); ++offset) {
    if (StartsWithAnnexBHeader(annex_b_data.subspan(offset))) {
      return offset;
    }
  }
  return annex_b_data.size();
}

}  // namespace

// static
std::optional<AvcParameterSets> AvcParameterSets::CreateFromAnnexB(
    span<const uint8_t> annex_b_data) {
  if (!annex_b_data.empty() && !StartsWithAnnexBHeader(annex_b_data)) {
    return std::nullopt;
  }
  if (annex_b_data.empty()) {
    return AvcParameterSets(
        kAnnexB, /*parameter_sets=*/{}, /*first_sps_index=*/-1,
        /*first_pps_index=*/-1, /*combined_size_in_bytes=*/0,
        /*combined_size_with_optionals_in_bytes=*/0);
  }

  std::vector<std::vector<uint8_t>> parameter_sets;
  int first_sps_index = -1;
  int first_pps_index = -1;
  size_t combined_size_in_bytes = 0;
  size_t combined_size_with_optionals_in_bytes = 0;

  while (annex_b_data.size() > kAnnexBHeaderSizeInBytes) {
    std::optional<size_t> nalu_size = GetFirstAnnexBNaluSize(annex_b_data);
    if (!nalu_size) {
      break;
    }
    span<const uint8_t> nalu = annex_b_data.first(*nalu_size);
    annex_b_data = annex_b_data.subspan(*nalu_size);
    if (nalu.size() <= kAnnexBHeaderSizeInBytes) {
      continue;
    }
    const uint8_t nalu_type = nalu[kAnnexBHeaderSizeInBytes];
    if (nalu_type == kSpsStartCode) {
      if (first_sps_index == -1) {
        first_sps_index = static_cast<int>(parameter_sets.size());
      }
      combined_size_in_bytes += nalu.size();
      parameter_sets.emplace_back(nalu.begin(), nalu.end());
    } else if (nalu_type == kPpsStartCode) {
      if (first_pps_index == -1) {
        first_pps_index = static_cast<int>(parameter_sets.size());
      }
      combined_size_in_bytes += nalu.size();
      parameter_sets.emplace_back(nalu.begin(), nalu.end());
    } else if (nalu_type == kIdrStartCode) {
      break;
    } else if (nalu_type == kAudStartCode) {
      combined_size_with_optionals_in_bytes += nalu.size();
    }
  }

  SB_LOG_IF(ERROR, first_sps_index == -1 || first_pps_index == -1)
      << "AVC parameter set NALUs not found.";

  return AvcParameterSets(kAnnexB, std::move(parameter_sets), first_sps_index,
                          first_pps_index, combined_size_in_bytes,
                          combined_size_with_optionals_in_bytes);
}

AvcParameterSets::AvcParameterSets(
    Format format,
    std::vector<std::vector<uint8_t>> parameter_sets,
    int first_sps_index,
    int first_pps_index,
    size_t combined_size_in_bytes,
    size_t combined_size_with_optionals_in_bytes)
    : format_(format),
      first_sps_index_(first_sps_index),
      first_pps_index_(first_pps_index),
      parameter_sets_(std::move(parameter_sets)),
      combined_size_in_bytes_(combined_size_in_bytes),
      combined_size_with_optionals_in_bytes_(
          combined_size_with_optionals_in_bytes) {}

std::vector<uint8_t> AvcParameterSets::GetAllSpses() const {
  std::vector<uint8_t> result;
  for (const auto& parameter_set : parameter_sets_) {
    if (parameter_set[4] == kSpsStartCode) {
      result.insert(result.end(), parameter_set.begin(), parameter_set.end());
    }
  }
  return result;
}

std::vector<uint8_t> AvcParameterSets::GetAllPpses() const {
  std::vector<uint8_t> result;
  for (const auto& parameter_set : parameter_sets_) {
    if (parameter_set[4] == kPpsStartCode) {
      result.insert(result.end(), parameter_set.begin(), parameter_set.end());
    }
  }
  return result;
}

AvcParameterSets AvcParameterSets::ConvertTo(Format new_format) const {
  if (format_ == new_format) {
    return *this;
  }

  SB_DCHECK_EQ(format_, kAnnexB);
  SB_DCHECK_EQ(new_format, kHeadless);

  std::vector<std::vector<uint8_t>> new_parameter_sets = parameter_sets_;
  size_t new_combined_size_in_bytes = combined_size_in_bytes_;
  for (auto& parameter_set : new_parameter_sets) {
    SB_DCHECK_GE(parameter_set.size(), kAnnexBHeaderSizeInBytes);
    parameter_set.erase(parameter_set.begin(),
                        parameter_set.begin() + kAnnexBHeaderSizeInBytes);
    new_combined_size_in_bytes -= kAnnexBHeaderSizeInBytes;
  }

  return AvcParameterSets(new_format, std::move(new_parameter_sets),
                          first_sps_index_, first_pps_index_,
                          new_combined_size_in_bytes,
                          combined_size_with_optionals_in_bytes_);
}

bool AvcParameterSets::operator==(const AvcParameterSets& that) const {
  SB_DCHECK_EQ(format(), that.format());

  if (parameter_sets_ == that.parameter_sets_) {
    SB_DCHECK_EQ(first_sps_index_, that.first_sps_index_);
    SB_DCHECK_EQ(first_pps_index_, that.first_pps_index_);
    return true;
  }
  return false;
}

bool AvcParameterSets::operator!=(const AvcParameterSets& that) const {
  return !(*this == that);
}

bool ConvertAnnexBToAvcc(span<const uint8_t> annex_b_source,
                         span<uint8_t> avcc_destination) {
  if (avcc_destination.size() < annex_b_source.size()) {
    return false;
  }
  if (annex_b_source.empty()) {
    return true;
  }
  if (!StartsWithAnnexBHeader(annex_b_source)) {
    return false;
  }

  constexpr size_t kAvccLengthInBytes = kAnnexBHeaderSizeInBytes;

  while (!annex_b_source.empty()) {
    std::optional<size_t> nalu_size = GetFirstAnnexBNaluSize(annex_b_source);
    if (!nalu_size) {
      return false;
    }

    span<const uint8_t> payload = annex_b_source.subspan(
        kAnnexBHeaderSizeInBytes, *nalu_size - kAnnexBHeaderSizeInBytes);
    const size_t payload_size = payload.size();
    avcc_destination[0] =
        static_cast<uint8_t>((payload_size & 0xff000000) >> 24);
    avcc_destination[1] = static_cast<uint8_t>((payload_size & 0xff0000) >> 16);
    avcc_destination[2] = static_cast<uint8_t>((payload_size & 0xff00) >> 8);
    avcc_destination[3] = static_cast<uint8_t>(payload_size & 0xff);
    std::copy(
        payload.begin(), payload.end(),
        avcc_destination.subspan(kAvccLengthInBytes, payload_size).begin());
    annex_b_source = annex_b_source.subspan(*nalu_size);
    avcc_destination = avcc_destination.subspan(*nalu_size);
  }

  return true;
}

}  // namespace starboard
