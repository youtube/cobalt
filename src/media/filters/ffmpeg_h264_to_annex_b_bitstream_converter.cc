// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_h264_to_annex_b_bitstream_converter.h"

#include "base/logging.h"
#include "media/ffmpeg/ffmpeg_common.h"

namespace media {

FFmpegH264ToAnnexBBitstreamConverter::FFmpegH264ToAnnexBBitstreamConverter(
    AVCodecContext* stream_context)
    : configuration_processed_(false),
      stream_context_(stream_context) {
  CHECK(stream_context_);
}

FFmpegH264ToAnnexBBitstreamConverter::~FFmpegH264ToAnnexBBitstreamConverter() {}

bool FFmpegH264ToAnnexBBitstreamConverter::ConvertPacket(AVPacket* packet) {
  uint32 output_packet_size = 0;
  uint32 configuration_size = 0;
  uint32 io_size = 0;
  if (packet == NULL) {
    return false;
  }

  // Calculate the needed output buffer size.
  if (!configuration_processed_) {
    // FFmpeg's AVCodecContext's extradata field contains the Decoder
    // Specific Information from MP4 headers that contain the H.264 SPS and
    // PPS members. See ISO/IEC 14496-15 Chapter 5.2.4
    // AVCDecoderConfigurationRecord for exact specification.
    // Extradata must be at least 7 bytes long.
    if (stream_context_->extradata == NULL ||
        stream_context_->extradata_size <= 7) {
      return false;  // Can't go on with conversion without configuration.
    }
    configuration_size += converter_.ParseConfigurationAndCalculateSize(
        stream_context_->extradata,
        stream_context_->extradata_size);
    if (configuration_size == 0) {
      return false;  // Not possible to parse the configuration.
    }
  }
  uint32 output_nal_size =
      converter_.CalculateNeededOutputBufferSize(packet->data, packet->size);
  if (output_nal_size == 0) {
    return false;  // Invalid input packet.
  }
  output_packet_size = configuration_size + output_nal_size;

  // Allocate new packet for the output.
  AVPacket dest_packet;
  if (av_new_packet(&dest_packet, output_packet_size) != 0) {
    return false;  // Memory allocation failure.
  }
  // This is a bit tricky: since the interface does not allow us to replace
  // the pointer of the old packet with a new one, we will initially copy the
  // metadata from old packet to new bigger packet.
  dest_packet.pts = packet->pts;
  dest_packet.dts = packet->dts;
  dest_packet.pos = packet->pos;
  dest_packet.duration = packet->duration;
  dest_packet.convergence_duration = packet->convergence_duration;
  dest_packet.flags = packet->flags;
  dest_packet.stream_index = packet->stream_index;

  // Process the configuration if not done earlier.
  if (!configuration_processed_) {
    if (!converter_.ConvertAVCDecoderConfigToByteStream(
            stream_context_->extradata, stream_context_->extradata_size,
            dest_packet.data, &configuration_size)) {
      return false;  // Failed to convert the buffer.
    }
    configuration_processed_ = true;
  }

  // Proceed with the conversion of the actual in-band NAL units, leave room
  // for configuration in the beginning.
  io_size = dest_packet.size - configuration_size;
  if (!converter_.ConvertNalUnitStreamToByteStream(
          packet->data, packet->size,
          dest_packet.data + configuration_size, &io_size)) {
    return false;
  }

  // At the end we must destroy the old packet.
  av_free_packet(packet);
  *packet = dest_packet;  // Finally, replace the values in the input packet.

  return true;
}

}  // namespace media

