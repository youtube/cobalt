// Copyright 2012 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/progressive/avc_parser.h"

#include <limits>
#include <vector>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/base/endian_util.h"
#include "cobalt/media/base/media_util.h"
#include "cobalt/media/base/video_types.h"
#include "cobalt/media/formats/mp4/aac.h"
#include "cobalt/media/progressive/avc_access_unit.h"
#include "cobalt/media/progressive/rbsp_stream.h"
#include "starboard/memory.h"

namespace cobalt {
namespace media {

// what's the smallest meaningful AVC config we can parse?
static const int kAVCConfigMinSize = 8;
// lower five bits of first byte in SPS should be 7
static const uint8 kSPSNALType = 7;

AVCParser::AVCParser(scoped_refptr<DataSourceReader> reader,
                     const scoped_refptr<MediaLog>& media_log)
    : ProgressiveParser(reader),
      media_log_(media_log),
      nal_header_size_(0),
      video_prepend_size_(0) {
  DCHECK(media_log);
}

AVCParser::~AVCParser() {}

bool AVCParser::Prepend(scoped_refptr<AvcAccessUnit> au,
                        scoped_refptr<DecoderBuffer> buffer) {
  // sanity-check inputs
  if (!au || !buffer) {
    NOTREACHED() << "bad input to Prepend()";
    return false;
  }
  if (au->GetType() == DemuxerStream::VIDEO) {
    if (au->AddPrepend()) {
      if (buffer->data_size() <= video_prepend_size_) {
        NOTREACHED() << "empty/undersized buffer to Prepend()";
        return false;
      }
      buffer->allocations().Write(0, video_prepend_, video_prepend_size_);
    }
  } else if (au->GetType() == DemuxerStream::AUDIO) {
    if (audio_prepend_.size() < 6) {
      // valid ADTS header not available
      return false;
    }
    if (buffer->data_size() <= audio_prepend_.size()) {
      NOTREACHED() << "empty/undersized buffer to Prepend()";
      return false;
    }
    // audio, need to copy ADTS header and then add buffer size
    uint32 buffer_size = au->GetSize() + audio_prepend_.size();
    // we can't express an AU size larger than 13 bits, something's bad here.
    if (buffer_size & 0xffffe000) {
      return false;
    }
    std::vector<uint8_t> audio_prepend(audio_prepend_);
    // OR size into buffer, byte 3 gets 2 MSb of 13-bit size
    audio_prepend[3] |= (uint8)((buffer_size & 0x00001800) >> 11);
    // byte 4 gets bits 10-3 of size
    audio_prepend[4] = (uint8)((buffer_size & 0x000007f8) >> 3);
    // byte 5 gets bits 2-0 of size
    audio_prepend[5] |= (uint8)((buffer_size & 0x00000007) << 5);
    buffer->allocations().Write(0, audio_prepend.data(), audio_prepend.size());
  } else {
    NOTREACHED() << "unsupported demuxer stream type.";
    return false;
  }

  return true;
}

bool AVCParser::DownloadAndParseAVCConfigRecord(uint64 offset, uint32 size) {
  if (size == 0) {
    return false;
  }
  std::vector<uint8> record_buffer(size);
  int bytes_read = reader_->BlockingRead(offset, size, &record_buffer[0]);
  DCHECK_LE(size, static_cast<uint32>(std::numeric_limits<int32>::max()));
  if (bytes_read < static_cast<int>(size)) {
    return false;
  }
  // ok, successfully downloaded the record, parse it
  return ParseAVCConfigRecord(&record_buffer[0], size);
}

// static
bool AVCParser::ParseSPS(const uint8* sps, size_t sps_size,
                         SPSRecord* record_out) {
  DCHECK(sps) << "no sps provided";
  DCHECK(record_out) << "no output structure provided";
  // first byte is NAL type id, check that it is SPS
  if ((*sps & 0x1f) != kSPSNALType) {
    DLOG(ERROR) << "bad NAL type on SPS";
    return false;
  }
  // convert SPS NALU to RBSP stream
  RBSPStream sps_rbsp(sps + 1, sps_size - 1);
  uint8 profile_idc = 0;
  if (!sps_rbsp.ReadByte(&profile_idc)) {
    DLOG(ERROR) << "failure reading profile_idc from sps RBSP";
    return false;
  }
  // skip 3 constraint flags, 5 reserved bits, and level_idc (16 bits)
  sps_rbsp.SkipBytes(2);
  // ReadUEV/ReadSEV require a value to be passed by reference but
  // there are many times in which we ignore this value.
  uint32 disposable_uev = 0;
  int32 disposable_sev = 0;
  // seq_parameter_set_id
  sps_rbsp.ReadUEV(&disposable_uev);
  // skip profile-specific encoding information if there
  if (profile_idc == 100 || profile_idc == 103 || profile_idc == 110 ||
      profile_idc == 122 || profile_idc == 244 || profile_idc == 44 ||
      profile_idc == 83 || profile_idc == 86 || profile_idc == 118) {
    uint32 chroma_format_idc = 0;
    if (!sps_rbsp.ReadUEV(&chroma_format_idc)) {
      DLOG(WARNING) << "failure reading chroma_format_idc from sps RBSP";
      return false;
    }
    if (chroma_format_idc == 3) {
      // separate_color_plane_flag
      sps_rbsp.SkipBits(1);
    }
    // bit_depth_luma_minus8
    sps_rbsp.ReadUEV(&disposable_uev);
    // bit_depth_chroma_minus8
    sps_rbsp.ReadUEV(&disposable_uev);
    // qpprime_y_zero_transform_bypass_flag
    sps_rbsp.SkipBits(1);
    // seq_scaling_matrix_present_flag
    uint8 seq_scaling_matrix_present_flag = 0;
    if (!sps_rbsp.ReadBit(&seq_scaling_matrix_present_flag)) {
      DLOG(ERROR)
          << "failure reading seq_scaling_matrix_present_flag from sps RBSP";
      return false;
    }
    if (seq_scaling_matrix_present_flag) {
      // seq_scaling_list_present_flag[]
      sps_rbsp.SkipBits(chroma_format_idc != 3 ? 8 : 12);
    }
  }
  // log2_max_frame_num_minus4
  sps_rbsp.ReadUEV(&disposable_uev);
  // pic_order_cnt_type
  uint32 pic_order_cnt_type = 0;
  if (!sps_rbsp.ReadUEV(&pic_order_cnt_type)) {
    DLOG(ERROR) << "failure reading pic_order_cnt_type from sps RBSP";
    return false;
  }
  if (pic_order_cnt_type == 0) {
    // log2_max_pic_order_cnt_lsb_minus4
    sps_rbsp.ReadUEV(&disposable_uev);
  } else if (pic_order_cnt_type == 1) {
    // delta_pic_order_always_zero_flag
    sps_rbsp.SkipBits(1);
    // offset_for_non_ref_pic
    sps_rbsp.ReadSEV(&disposable_sev);
    // offset_for_top_to_bottom_field
    sps_rbsp.ReadSEV(&disposable_sev);
    // num_ref_frames_in_pic_order_cnt_cycle
    uint32 num_ref_frames_in_pic_order_cnt_cycle = 0;
    if (!sps_rbsp.ReadUEV(&num_ref_frames_in_pic_order_cnt_cycle)) {
      DLOG(ERROR)
          << "failure reading num_ref_frames_in_pic_order_cnt_cycle from sps";
      return false;
    }
    for (uint32 i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; ++i) {
      sps_rbsp.ReadSEV(&disposable_sev);
    }
  }
  // number of reference frames used to decode
  uint32 num_ref_frames = 0;
  if (!sps_rbsp.ReadUEV(&num_ref_frames)) {
    DLOG(ERROR) << "failure reading number of ref frames from sps RBSP";
    return false;
  }
  // gaps_in_frame_num_value_allowed_flag
  sps_rbsp.SkipBits(1);
  // width is calculated from pic_width_in_mbs_minus1
  uint32 pic_width_in_mbs_minus1 = 0;
  if (!sps_rbsp.ReadUEV(&pic_width_in_mbs_minus1)) {
    DLOG(WARNING) << "failure reading image width from sps RBSP";
    return false;
  }
  // 16 pxs per macroblock
  uint32 width = (pic_width_in_mbs_minus1 + 1) * 16;
  // pic_height_in_map_units_minus1
  uint32 pic_height_in_map_units_minus1 = 0;
  if (!sps_rbsp.ReadUEV(&pic_height_in_map_units_minus1)) {
    DLOG(ERROR)
        << "failure reading pic_height_in_map_uints_minus1 from sps RBSP";
    return false;
  }
  uint8 frame_mbs_only_flag = 0;
  if (!sps_rbsp.ReadBit(&frame_mbs_only_flag)) {
    DLOG(ERROR) << "failure reading frame_mbs_only_flag from sps RBSP";
    return false;
  }
  uint32 height = (2 - static_cast<uint32>(frame_mbs_only_flag)) *
                  (pic_height_in_map_units_minus1 + 1) * 16;
  if (!frame_mbs_only_flag) {
    sps_rbsp.SkipBits(1);
  }
  // direct_8x8_inference_flag
  sps_rbsp.SkipBits(1);
  // frame cropping flag
  uint8 frame_cropping_flag = 0;
  if (!sps_rbsp.ReadBit(&frame_cropping_flag)) {
    DLOG(ERROR) << "failure reading frame_cropping_flag from sps RBSP";
    return false;
  }
  // distance in pixels from the associated edge of the media:
  //
  // <---coded_size---width--------------------->
  //
  // +------------------------------------------+   ^
  // |                 ^                        |   |
  // |                 |                        |   |
  // |              crop_top                    |   |
  // |                 |                        |   |
  // |                 v                        | height
  // |               +---------+                |   |
  // |<--crop_left-->| visible |                |   |
  // |               |   rect  |<--crop_right-->|   |
  // |               +---------+                |   |
  // |                  ^                       |   |
  // |                  |                       |   |
  // |              crop_bottom                 |   |
  // |                  |                       |   |
  // |                  v                       |   |
  // +------------------------------------------+   v
  //
  uint32 crop_left = 0;
  uint32 crop_right = 0;
  uint32 crop_top = 0;
  uint32 crop_bottom = 0;
  // cropping values are stored divided by two
  if (frame_cropping_flag) {
    if (!sps_rbsp.ReadUEV(&crop_left)) {
      DLOG(ERROR) << "failure reading crop_left from sps RBSP";
      return false;
    }
    if (!sps_rbsp.ReadUEV(&crop_right)) {
      DLOG(ERROR) << "failure reading crop_right from sps RBSP";
      return false;
    }
    if (!sps_rbsp.ReadUEV(&crop_top)) {
      DLOG(ERROR) << "failure reading crop_top from sps RBSP";
      return false;
    }
    if (!sps_rbsp.ReadUEV(&crop_bottom)) {
      DLOG(ERROR) << "failure reading crop_bottom from sps RBSP";
      return false;
    }
    crop_left *= 2;
    crop_right *= 2;
    crop_top *= 2;
    crop_bottom *= 2;
  }
  // remainder of SPS are values we can safely ignore, everything
  // checks out, write output structure
  int visible_width = width - (crop_left + crop_right);
  int visible_height = height - (crop_top + crop_bottom);
  record_out->coded_size = math::Size(width, height),
  record_out->visible_rect =
      math::Rect(crop_left, crop_top, visible_width, visible_height),
  record_out->natural_size = math::Size(visible_width, visible_height);
  record_out->num_ref_frames = num_ref_frames;
  return true;
}

bool AVCParser::ParseAVCConfigRecord(uint8* buffer, uint32 size) {
  if (size < kAVCConfigMinSize) {
    DLOG(ERROR) << base::StringPrintf("AVC config record bad size: %d", size);
    return false;
  }

  // get the NALU header size
  nal_header_size_ = (buffer[4] & 0x03) + 1;
  // validate size, needs to be 1, 2 or 4 bytes only
  if (nal_header_size_ != 4 && nal_header_size_ != 2 && nal_header_size_ != 1) {
    return false;
  }
  // AVCConfigRecords contain a variable number of SPS NALU
  // (Sequence Parameter Set) (Network Abstraction Layer Units)
  // from which we can extract width, height, and cropping info.
  // That means we need at least 1 SPS NALU in this stream for extraction.
  uint8 number_of_sps_nalus = buffer[5] & 0x1f;
  if (number_of_sps_nalus == 0) {
    DLOG(WARNING) << "got AVCConfigRecord without any SPS NALUs!";
    return false;
  }
  // iterate through SPS NALUs finding one of valid size for our purposes
  // (this should usually be the first one), but also advancing through
  // the ConfigRecord until we encounter the PPS sets
  bool have_valid_sps = false;
  int record_offset = 6;
  size_t usable_sps_size = 0;
  int usable_sps_offset = 0;
  for (uint8 i = 0; i < number_of_sps_nalus; i++) {
    // make sure we haven't run out of record for the 2-byte size record
    DCHECK_LE(size, static_cast<uint32>(std::numeric_limits<int32>::max()));
    if (record_offset + 2 > static_cast<int>(size)) {
      DLOG(WARNING) << "ran out of AVCConfig record while parsing SPS size.";
      return false;
    }
    // extract 2-byte size of this SPS
    size_t sps_size =
        endian_util::load_uint16_big_endian(buffer + record_offset);
    // advance past the 2-byte size record
    record_offset += 2;
    // see if we jumped over record size
    if (record_offset + sps_size > size) {
      DLOG(WARNING) << "ran out of AVCConfig record while parsing SPS blocks.";
      return false;
    }
    if (!have_valid_sps) {
      have_valid_sps = true;
      // save size and offset for later copying and parsing
      usable_sps_size = sps_size;
      usable_sps_offset = record_offset;
      // continue to iterate through sps records to get to pps which follow
    }
    record_offset += sps_size;
  }
  if (!have_valid_sps) {
    DLOG(WARNING)
        << "unable to parse a suitable SPS. Perhaps increase max size?";
    return false;
  }
  // we don't strictly require a PPS, so we're even willing to accept that
  // this could be the end of the bytestream, but if not the next byte should
  // define the number of PPS objects in the record. Not sure if
  // specific decoders could decode something without a PPS prepend but this
  // doesn't break demuxing so we'll let them complain if that isn't going
  // to work for them :)
  size_t usable_pps_size = 0;
  size_t usable_pps_offset = 0;
  bool have_valid_pps = false;
  DCHECK_LE(size, static_cast<uint32>(std::numeric_limits<int32>::max()));
  if (record_offset + 1 < static_cast<int>(size)) {
    uint8 number_of_pps_nalus = buffer[record_offset];
    record_offset++;
    for (uint8 i = 0; i < number_of_pps_nalus; i++) {
      // make sure we don't run out of room for 2-byte size record
      DCHECK_LE(size, static_cast<uint32>(std::numeric_limits<int32>::max()));
      if (record_offset + 2 >= static_cast<int>(size)) {
        DLOG(WARNING) << "ran out of AVCConfig record while parsing PPS size.";
        return false;
      }
      // extract 2-byte size of this PPS
      size_t pps_size =
          endian_util::load_uint16_big_endian(buffer + record_offset);
      record_offset += 2;
      // see if there's actually room for this record in the buffer
      if (record_offset + pps_size > size) {
        DLOG(WARNING)
            << "ran out of AVCConfig record while scanning PPS blocks.";
        return false;
      }
      if (!have_valid_pps) {
        have_valid_pps = true;
        usable_pps_size = pps_size;
        usable_pps_offset = record_offset;
        break;
      }
    }
  }
  // now we parse the valid SPS we extracted from byte stream earlier.
  SPSRecord sps_record;
  if (!ParseSPS(buffer + usable_sps_offset, usable_sps_size, &sps_record)) {
    DLOG(WARNING) << "error parsing SPS";
    return false;
  }
  // we can now initialize our video decoder config
  video_config_.Initialize(kCodecH264,
                           H264PROFILE_MAIN,  // profile is ignored currently
                           PIXEL_FORMAT_YV12, COLOR_SPACE_HD_REC709,
                           sps_record.coded_size, sps_record.visible_rect,
                           sps_record.natural_size, EmptyExtraData(),
                           Unencrypted());

  return BuildAnnexBPrepend(buffer + usable_sps_offset, usable_sps_size,
                            buffer + usable_pps_offset, usable_pps_size);
}

bool AVCParser::BuildAnnexBPrepend(uint8* sps, uint32 sps_size, uint8* pps,
                                   uint32 pps_size) {
  // We will need to attach the sps and pps (if provided) to each keyframe
  // video packet, with the AnnexB start code in front of each. Start with
  // sps size and start code
  video_prepend_size_ = sps_size + kAnnexBStartCodeSize;
  if (pps_size > 0) {
    // Add pps and pps start code size if needed.
    video_prepend_size_ += pps_size + kAnnexBStartCodeSize;
  }
  // this should be a very rare case for typical videos
  if (video_prepend_size_ > kAnnexBPrependMaxSize) {
    NOTREACHED() << base::StringPrintf("Bad AnnexB prepend size: %d",
                                       video_prepend_size_);
    return false;
  }
  // start code for sps comes first
  memcpy(video_prepend_, kAnnexBStartCode, kAnnexBStartCodeSize);
  // followed by sps body
  memcpy(video_prepend_ + kAnnexBStartCodeSize, sps, sps_size);
  int prepend_offset = kAnnexBStartCodeSize + sps_size;
  if (pps_size > 0) {
    // pps start code comes next
    memcpy(video_prepend_ + prepend_offset, kAnnexBStartCode,
                 kAnnexBStartCodeSize);
    prepend_offset += kAnnexBStartCodeSize;
    // followed by pps
    memcpy(video_prepend_ + prepend_offset, pps, pps_size);
    prepend_offset += pps_size;
  }

  // make sure we haven't wandered off into memory somewhere
  DCHECK_EQ(prepend_offset, video_prepend_size_);
  return true;
}

void AVCParser::ParseAudioSpecificConfig(uint8 b0, uint8 b1) {
  media::mp4::AAC aac;
  std::vector<uint8> aac_config(2);

  aac_config[0] = b0;
  aac_config[1] = b1;
  audio_prepend_.clear();

  if (!aac.Parse(aac_config, media_log_) ||
      !aac.ConvertEsdsToADTS(&audio_prepend_)) {
    DLOG(WARNING) << "Error in parsing AudioSpecificConfig.";
    return;
  }

  // Clear the length, it is 13 bits and stored as ******LL LLLLLLLL LLL*****
  // in bytes 3 to 5.
  audio_prepend_[3] &= 0xfc;
  audio_prepend_[4] = 0;
  audio_prepend_[5] &= 0x1f;

  const bool kSbrInMimetype = false;
  audio_config_.Initialize(
      kCodecAAC, kSampleFormatS16, aac.GetChannelLayout(kSbrInMimetype),
      aac.GetOutputSamplesPerSecond(kSbrInMimetype), aac.codec_specific_data(),
      Unencrypted(), base::TimeDelta(), 0);
}

size_t AVCParser::CalculatePrependSize(DemuxerStream::Type type,
                                       bool is_keyframe) {
  size_t prepend_size = 0;
  if (type == DemuxerStream::VIDEO) {
    bool needs_prepend = is_keyframe;
    if (needs_prepend) prepend_size = video_prepend_size_;
  } else if (type == DemuxerStream::AUDIO) {
    prepend_size = audio_prepend_.size();
  } else {
    NOTREACHED() << "unsupported stream type";
  }
  return prepend_size;
}

}  // namespace media
}  // namespace cobalt
