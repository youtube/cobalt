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
#include "modules/rtp_rtcp/source/rtp_format_h264.h"
#include "modules/video_coding/codecs/h264/include/h264_globals.h"
#include "test/fuzzers/fuzz_data_helper.h"
#include "test/fuzzers/utils/validate_rtp_packetizer.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  test::FuzzDataHelper fuzz_input(MakeArrayView(data, size));

  RtpPacketizer::PayloadSizeLimits limits = ReadPayloadSizeLimits(fuzz_input);

  const H264PacketizationMode kPacketizationModes[] = {
      H264PacketizationMode::NonInterleaved,
      H264PacketizationMode::SingleNalUnit};

  H264PacketizationMode packetization_mode =
      fuzz_input.SelectOneOf(kPacketizationModes);

  // Main function under test: RtpPacketizerH264's constructor.
  RtpPacketizerH264 packetizer(fuzz_input.ReadByteArray(fuzz_input.BytesLeft()),
                               limits, packetization_mode);

  ValidateRtpPacketizer(limits, packetizer);
}
}  // namespace webrtc
