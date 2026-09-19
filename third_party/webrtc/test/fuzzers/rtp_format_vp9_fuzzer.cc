/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stddef.h>
#include <stdint.h>

#include "api/array_view.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_format_vp9.h"
#include "modules/video_coding/codecs/interface/common_constants.h"
#include "modules/video_coding/codecs/vp9/include/vp9_globals.h"
#include "test/fuzzers/fuzz_data_helper.h"
#include "test/fuzzers/utils/validate_rtp_packetizer.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  test::FuzzDataHelper fuzz_input(MakeArrayView(data, size));

  RtpPacketizer::PayloadSizeLimits limits = ReadPayloadSizeLimits(fuzz_input);

  RTPVideoHeaderVP9 hdr_info;
  hdr_info.InitRTPVideoHeaderVP9();
  uint16_t picture_id = fuzz_input.ReadOrDefaultValue<uint16_t>(0);
  hdr_info.picture_id =
      picture_id >= 0x8000 ? kNoPictureId : picture_id & 0x7fff;

  // Main function under test: RtpPacketizerVp9's constructor.
  RtpPacketizerVp9 packetizer(fuzz_input.ReadByteArray(fuzz_input.BytesLeft()),
                              limits, hdr_info);

  ValidateRtpPacketizer(limits, packetizer);
}
}  // namespace webrtc
