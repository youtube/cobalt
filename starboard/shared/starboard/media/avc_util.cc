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

#include <type_traits>

#include "starboard/memory.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

namespace {

const uint8_t kAnnexBHeader[] = {0, 0, 0, 1};
const auto kAnnexBHeaderSizeInBytes =
    AvcParameterSets::kAnnexBHeaderSizeInBytes;

bool StartsWithAnnexBHeader(const uint8_t* annex_b_data,
                            size_t annex_b_data_size) {
  static_assert(
      sizeof(kAnnexBHeader) == AvcParameterSets::kAnnexBHeaderSizeInBytes,
      "sizeof(kAnnexBHeader) doesn't match kAnnexBHeaderSizeInBytes");

  if (annex_b_data_size < sizeof(kAnnexBHeader)) {
    return false;
  }
  return memcmp(annex_b_data, kAnnexBHeader, sizeof(kAnnexBHeader)) ==
         0;
}

// UInt8Type can be "uint8_t", or "const uint8_t".
template <typename UInt8Type>
bool AdvanceToNextAnnexBHeader(UInt8Type** annex_b_data,
                               size_t* annex_b_data_size) {
  SB_DCHECK(annex_b_data);
  SB_DCHECK(annex_b_data_size);

  if (!StartsWithAnnexBHeader(*annex_b_data, *annex_b_data_size)) {
    return false;
  }

  *annex_b_data += kAnnexBHeaderSizeInBytes;
  *annex_b_data_size -= kAnnexBHeaderSizeInBytes;

  while (*annex_b_data_size > 0) {
    if (StartsWithAnnexBHeader(*annex_b_data, *annex_b_data_size)) {
      return true;
    }
    ++*annex_b_data;
    --*annex_b_data_size;
  }
  return true;
}

bool ExtractAnnexBNalu(const uint8_t** annex_b_data,
                       size_t* annex_b_data_size,
                       std::vector<uint8_t>* annex_b_nalu) {
  SB_DCHECK(annex_b_data);
  SB_DCHECK(annex_b_data_size);
  SB_DCHECK(annex_b_nalu);

  const uint8_t* saved_data = *annex_b_data;

  if (!AdvanceToNextAnnexBHeader(annex_b_data, annex_b_data_size)) {
    return false;
  }

  annex_b_nalu->assign(saved_data, *annex_b_data);
  return true;
}

}  // namespace

AvcParameterSets::AvcParameterSets(Format format,
                                   const uint8_t* data,
                                   size_t size)
    : format_(format) {
  SB_DCHECK(format == kAnnexB);

  is_valid_ =
      format == kAnnexB && (size == 0 || StartsWithAnnexBHeader(data, size));

  if (!is_valid_) {
    return;
  }

  if (size == 0) {
    return;
  }

  std::vector<uint8_t> nalu;
  while (size > kAnnexBHeaderSizeInBytes &&
         ExtractAnnexBNalu(&data, &size, &nalu)) {
    if (nalu[kAnnexBHeaderSizeInBytes] == kSpsStartCode) {
      if (first_sps_index_ == -1) {
        first_sps_index_ = static_cast<int>(parameter_sets_.size());
      }
      parameter_sets_.push_back(nalu);
      combined_size_in_bytes_ += nalu.size();
    } else if (nalu[kAnnexBHeaderSizeInBytes] == kPpsStartCode) {
      if (first_pps_index_ == -1) {
        first_pps_index_ = static_cast<int>(parameter_sets_.size());
      }
      parameter_sets_.push_back(nalu);
      combined_size_in_bytes_ += nalu.size();
    } else if (nalu[kAnnexBHeaderSizeInBytes] == kIdrStartCode) {
      break;
    }
  }
  SB_LOG_IF(ERROR, first_sps_index_ == -1 || first_pps_index_ == -1)
      << "AVC parameter set NALUs not found.";
}

AvcParameterSets AvcParameterSets::ConvertTo(Format new_format) const {
  if (format_ == new_format) {
    return *this;
  }

  SB_DCHECK(format_ == kAnnexB);
  SB_DCHECK(new_format == kHeadless);

  AvcParameterSets new_parameter_sets(*this);
  new_parameter_sets.format_ = new_format;
  if (!new_parameter_sets.is_valid()) {
    return new_parameter_sets;
  }
  for (auto& parameter_set : new_parameter_sets.parameter_sets_) {
    SB_DCHECK(parameter_set.size() >= kAnnexBHeaderSizeInBytes);
    parameter_set.erase(parameter_set.begin(),
                        parameter_set.begin() + kAnnexBHeaderSizeInBytes);
    new_parameter_sets.combined_size_in_bytes_ -= kAnnexBHeaderSizeInBytes;
  }

  return new_parameter_sets;
}

bool AvcParameterSets::operator==(const AvcParameterSets& that) const {
  if (is_valid() != that.is_valid()) {
    return false;
  }
  if (!is_valid()) {
    return true;
  }

  SB_DCHECK(format() == that.format());

  if (parameter_sets_ == that.parameter_sets_) {
    SB_DCHECK(first_sps_index_ == that.first_sps_index_);
    SB_DCHECK(first_pps_index_ == that.first_pps_index_);
    return true;
  }
  return false;
}

bool AvcParameterSets::operator!=(const AvcParameterSets& that) const {
  return !(*this == that);
}

bool ConvertAnnexBToAvcc(const uint8_t* annex_b_source,
                         size_t size,
                         uint8_t* avcc_destination) {
  if (size == 0) {
    return true;
  }

  SB_DCHECK(annex_b_source);
  SB_DCHECK(avcc_destination);

  if (!StartsWithAnnexBHeader(annex_b_source, size)) {
    return false;
  }

  auto annex_b_source_size = size;
  // |avcc_destination_end| exists only for the purpose of validation.
  const auto avcc_destination_end = avcc_destination + size;

  const uint8_t* last_source = annex_b_source;

  const auto kAvccLengthInBytes = kAnnexBHeaderSizeInBytes;

  while (AdvanceToNextAnnexBHeader(&annex_b_source, &annex_b_source_size)) {
    SB_DCHECK(annex_b_source - last_source >= kAnnexBHeaderSizeInBytes);
    SB_DCHECK(avcc_destination < avcc_destination_end);

    size_t payload_size =
        annex_b_source - last_source - kAnnexBHeaderSizeInBytes;
    avcc_destination[0] =
        static_cast<uint8_t>((payload_size & 0xff000000) >> 24);
    avcc_destination[1] = static_cast<uint8_t>((payload_size & 0xff0000) >> 16);
    avcc_destination[2] = static_cast<uint8_t>((payload_size & 0xff00) >> 8);
    avcc_destination[3] = static_cast<uint8_t>(payload_size & 0xff);
    memcpy(avcc_destination + kAvccLengthInBytes,
                 last_source + kAnnexBHeaderSizeInBytes, payload_size);
    avcc_destination += annex_b_source - last_source;
    last_source = annex_b_source;
  }

  SB_DCHECK(annex_b_source_size == 0);
  SB_DCHECK(avcc_destination == avcc_destination_end);

  return true;
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
