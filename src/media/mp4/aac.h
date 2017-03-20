// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP4_AAC_H_
#define MEDIA_MP4_AAC_H_

#include <vector>

#include "base/basictypes.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"

namespace media {

class BitReader;

namespace mp4 {

// This class parses the AAC information from decoder specific information
// embedded in the esds box in an ISO BMFF file.
// Please refer to ISO 14496 Part 3 Table 1.13 - Syntax of AudioSpecificConfig
// for more details.
class MEDIA_EXPORT AAC {
 public:
  // Size in bytes of the ADTS header added by ConvertEsdsToADTS().
  static const size_t kADTSHeaderSize = 7;
  static const size_t kFramesPerAccessUnit = 1024;

  AAC();
  ~AAC();

  // Parse the AAC config from the raw binary data embedded in esds box.
  // The function will parse the data and get the ElementaryStreamDescriptor,
  // then it will parse the ElementaryStreamDescriptor to get audio stream
  // configurations.
  bool Parse(const std::vector<uint8>& data);

  // Gets the output sample rate for the AAC stream.
  // |sbr_in_mimetype| should be set to true if the SBR mode is
  // signalled in the mimetype. (ie mp4a.40.5 in the codecs parameter).
  // Returns the samples_per_second value that should used in an
  // AudioDecoderConfig.
  int GetOutputSamplesPerSecond(bool sbr_in_mimetype) const;
  ChannelLayout channel_layout() const;

  // This function converts a raw AAC frame into an AAC frame with an ADTS
  // header. On success, the function returns true and stores the converted data
  // in the buffer. The function returns false on failure and leaves the buffer
  // unchanged.
  bool ConvertEsdsToADTS(std::vector<uint8>* buffer) const;

  const std::vector<uint8>& raw_data() const { return raw_data_; }

 private:
  bool SkipDecoderGASpecificConfig(BitReader* bit_reader) const;
  bool SkipErrorSpecificConfig() const;
  bool SkipGASpecificConfig(BitReader* bit_reader) const;

  // The following variables store the AAC specific configuration information
  // that are used to generate the ADTS header.
  uint8 profile_;
  uint8 frequency_index_;
  uint8 channel_config_;

  // The following variables store audio configuration information that
  // can be used by Chromium. They are based on the AAC specific
  // configuration but can be overridden by extensions in elementary
  // stream descriptor.
  int frequency_;
  int extension_frequency_;
  ChannelLayout channel_layout_;

  std::vector<uint8> raw_data_;
};

}  // namespace mp4

}  // namespace media

#endif  // MEDIA_MP4_AAC_H_
