// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mp4/avc.h"

#include <algorithm>
#include <vector>

#include "media/mp4/box_definitions.h"
#include "media/mp4/box_reader.h"

namespace media {
namespace mp4 {

static const uint8 kAnnexBStartCode[] = {0, 0, 0, 1};
static const int kAnnexBStartCodeSize = 4;

static bool ConvertAVCToAnnexBInPlaceForLengthSize4(std::vector<uint8>* buf) {
  const int kLengthSize = 4;
  size_t pos = 0;
  while (pos + kLengthSize < buf->size()) {
    int nal_size = (*buf)[pos];
    nal_size = (nal_size << 8) + (*buf)[pos+1];
    nal_size = (nal_size << 8) + (*buf)[pos+2];
    nal_size = (nal_size << 8) + (*buf)[pos+3];
    std::copy(kAnnexBStartCode, kAnnexBStartCode + kAnnexBStartCodeSize,
              buf->begin() + pos);
    pos += kLengthSize + nal_size;
  }
  return pos == buf->size();
}

// static
bool AVC::ConvertToAnnexB(int length_size, std::vector<uint8>* buffer) {
  RCHECK(length_size == 1 || length_size == 2 || length_size == 4);

  if (length_size == 4)
    return ConvertAVCToAnnexBInPlaceForLengthSize4(buffer);

  std::vector<uint8> temp;
  temp.swap(*buffer);
  buffer->reserve(temp.size() + 32);

  size_t pos = 0;
  while (pos + length_size < temp.size()) {
    int nal_size = temp[pos];
    if (length_size == 2) nal_size = (nal_size << 8) + temp[pos+1];
    pos += length_size;

    RCHECK(pos + nal_size <= temp.size());
    buffer->insert(buffer->end(), kAnnexBStartCode,
                   kAnnexBStartCode + kAnnexBStartCodeSize);
    buffer->insert(buffer->end(), temp.begin() + pos,
                   temp.begin() + pos + nal_size);
    pos += nal_size;
  }
  return pos == temp.size();
}

// static
bool AVC::InsertParameterSets(const AVCDecoderConfigurationRecord& avc_config,
                         std::vector<uint8>* buffer) {
  int total_size = 0;
  for (size_t i = 0; i < avc_config.sps_list.size(); i++)
    total_size += avc_config.sps_list[i].size() + kAnnexBStartCodeSize;
  for (size_t i = 0; i < avc_config.pps_list.size(); i++)
    total_size += avc_config.pps_list[i].size() + kAnnexBStartCodeSize;

  std::vector<uint8> temp;
  temp.reserve(total_size);

  for (size_t i = 0; i < avc_config.sps_list.size(); i++) {
    temp.insert(temp.end(), kAnnexBStartCode,
                kAnnexBStartCode + kAnnexBStartCodeSize);
    temp.insert(temp.end(), avc_config.sps_list[i].begin(),
                avc_config.sps_list[i].end());
  }

  for (size_t i = 0; i < avc_config.pps_list.size(); i++) {
    temp.insert(temp.end(), kAnnexBStartCode,
                kAnnexBStartCode + kAnnexBStartCodeSize);
    temp.insert(temp.end(), avc_config.pps_list[i].begin(),
                avc_config.pps_list[i].end());
  }

  buffer->insert(buffer->begin(), temp.begin(), temp.end());
  return true;
}

// static
ChannelLayout AVC::ConvertAACChannelCountToChannelLayout(int count) {
  switch (count) {
    case 1:
      return CHANNEL_LAYOUT_MONO;
    case 2:
      return CHANNEL_LAYOUT_STEREO;
    case 3:
      return CHANNEL_LAYOUT_SURROUND;
    case 4:
      return CHANNEL_LAYOUT_4_0;
    case 5:
      return CHANNEL_LAYOUT_5_0;
    case 6:
      return CHANNEL_LAYOUT_5_1;
    case 8:
      return CHANNEL_LAYOUT_7_1;
    default:
      return CHANNEL_LAYOUT_UNSUPPORTED;
  }
}

}  // namespace mp4
}  // namespace media
