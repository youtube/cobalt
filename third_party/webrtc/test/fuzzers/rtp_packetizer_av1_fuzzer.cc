/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stddef.h>
#include <stdint.h>

#include "api/video/video_frame_type.h"
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "modules/rtp_rtcp/source/rtp_packetizer_av1.h"
#include "test/fuzzers/fuzz_data_helper.h"
#include "test/fuzzers/utils/validate_rtp_packetizer.h"

namespace webrtc {
void FuzzOneInput(FuzzDataHelper fuzz_data) {
  RtpPacketizer::PayloadSizeLimits limits = ReadPayloadSizeLimits(fuzz_data);

  const VideoFrameType kFrameTypes[] = {VideoFrameType::kVideoFrameKey,
                                        VideoFrameType::kVideoFrameDelta};
  VideoFrameType frame_type = fuzz_data.SelectOneOf(kFrameTypes);

  // Main function under test: RtpPacketizerAv1's constructor.
  RtpPacketizerAv1 packetizer(fuzz_data.ReadRemaining(), limits, frame_type,
                              /*is_last_frame_in_picture=*/true);

  ValidateRtpPacketizer(limits, packetizer);
}
}  // namespace webrtc
