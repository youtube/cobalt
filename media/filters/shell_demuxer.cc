/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "media/filters/shell_demuxer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "media/base/decoder_buffer.h"
#include "media/base/shell_buffer_factory.h"
#include "media/filters/shell_audio_decoder.h"
#include "media/filters/shell_video_decoder.h"
#include "media/mp4/avc.h"

namespace media {

// We extract our audio and video configuration from the downloaded stream
// on Initialization of the ShellDemuxer object. Most YouTube streams are
// well-formed and the configuration information is available at the start
// of the stream. This parsing takes place at pipeline setup and so adds
// to the startup latency of the video and also increases memory consumption
// as everything we parse must be buffered to be decoded. Therefore we define
// a maximum number of bytes we'll download to find this information before
// aborting this stream as an error.
static const uint64 kUnconfigStreamMaxSizeBytes = 2 * 1024 * 1024;

ShellDemuxerStream::ShellDemuxerStream(ShellDemuxer* demuxer,
                                       Type type)
    : demuxer_(demuxer)
    , stopped_(false)
    , type_(type) {
  DCHECK(demuxer_);
}


void ShellDemuxerStream::Read(const ReadCB& read_cb) {
  DCHECK(!read_cb.is_null());

  base::AutoLock auto_lock(lock_);

  // Don't accept any additional reads if we've been told to stop.
  // The demuxer_ may have been destroyed in the pipleine thread.
  if (stopped_) {
    read_cb.Run(DemuxerStream::kOk,
                scoped_refptr<DecoderBuffer>(DecoderBuffer::CreateEOSBuffer()));
    return;
  }

  // Buffers are only queued when there are no pending reads.
  DCHECK(buffer_queue_.empty() || read_queue_.empty());

  // if no enqueued buffer post a read task on DemuxerThread
  if (buffer_queue_.empty()) {
    demuxer_->message_loop()->PostTask(FROM_HERE, base::Bind(
        &ShellDemuxerStream::ReadTask, this, read_cb));
    return;
  }

  // Send the oldest buffer back.
  scoped_refptr<DecoderBuffer> buffer = buffer_queue_.front();
  buffer_queue_.pop_front();
  read_cb.Run(DemuxerStream::kOk, buffer);
}

void ShellDemuxerStream::ReadTask(const ReadCB &read_cb) {
  DCHECK_EQ(MessageLoop::current(), demuxer_->message_loop());
  base::AutoLock auto_lock(lock_);
  // Don't accept any additional reads if we've been told to stop.
  //
  // TODO(scherkus): it would be cleaner if we replied with an error message.
  if (stopped_) {
    read_cb.Run(DemuxerStream::kOk,
                scoped_refptr<DecoderBuffer>(DecoderBuffer::CreateEOSBuffer()));
    return;
  }

  // Enqueue the callback and attempt to satisfy it immediately.
  read_queue_.push_back(read_cb);
  FulfillPendingReadLocked();

  // Check if there are still pending reads, demux some more.
  if (!read_queue_.empty()) {
    demuxer_->PostDemuxTask();
  }
}

const AudioDecoderConfig& ShellDemuxerStream::audio_decoder_config() {
  return demuxer_->audio_decoder_config();
}

const VideoDecoderConfig& ShellDemuxerStream::video_decoder_config() {
  return demuxer_->video_decoder_config();
}

Ranges<base::TimeDelta> ShellDemuxerStream::GetBufferedRanges() {
  NOTIMPLEMENTED();
  return Ranges<base::TimeDelta>();
}

DemuxerStream::Type ShellDemuxerStream::type() {
  return type_;
}

void ShellDemuxerStream::EnableBitstreamConverter() {
  NOTIMPLEMENTED();
}

bool ShellDemuxerStream::HasPendingReads() {
  // ensure running on ShellDemuxer thread
  DCHECK_EQ(MessageLoop::current(), demuxer_->message_loop());
  base::AutoLock auto_lock(lock_);
  DCHECK(!stopped_ || read_queue_.empty())
      << "Read queue should have been emptied if demuxing stream is stopped";
  return !read_queue_.empty();
}

void ShellDemuxerStream::EnqueueBuffer(scoped_refptr<DecoderBuffer> buffer) {
  // ensure running on ShellDemuxer thread
  DCHECK_EQ(MessageLoop::current(), demuxer_->message_loop());

  base::AutoLock auto_lock(lock_);
  if (stopped_) {
    NOTREACHED() << "Attempted to enqueue packet on a stopped stream";
    return;
  }

  // timestamp?
  buffer_queue_.push_back(buffer);

  FulfillPendingReadLocked();
}

void ShellDemuxerStream::FulfillPendingReadLocked() {
  DCHECK_EQ(MessageLoop::current(), demuxer_->message_loop());
  lock_.AssertAcquired();
  if (buffer_queue_.empty() || read_queue_.empty()) {
    return;
  }

  // Dequeue a buffer and pending read pair.
  scoped_refptr<DecoderBuffer> buffer = buffer_queue_.front();
  ReadCB read_cb(read_queue_.front());
  buffer_queue_.pop_front();
  read_queue_.pop_front();

  // Execute the callback.
  read_cb.Run(DemuxerStream::kOk, buffer);
}

void ShellDemuxerStream::ClearBufferQueueLocked() {
  DCHECK_EQ(MessageLoop::current(), demuxer_->message_loop());
  lock_.AssertAcquired();
  while (buffer_queue_.size()) {
    scoped_refptr<DecoderBuffer> buffer = buffer_queue_.front();
    buffer_queue_.pop_front();
    // EOS buffers are not recyclable
    if (buffer->IsEndOfStream()) continue;
    // return to correct pool
    if (type_ == AUDIO) {
      ShellBufferFactory::ReturnAudioDecoderBuffer(buffer);
    } else if (type_ == VIDEO) {
      ShellBufferFactory::ReturnVideoDecoderBuffer(buffer);
    } else {
      NOTREACHED();
    }
  }
}

void ShellDemuxerStream::FlushBuffers() {
  DCHECK_EQ(MessageLoop::current(), demuxer_->message_loop());
  base::AutoLock auto_lock(lock_);
  DCHECK(read_queue_.empty()) << "Read requests should be empty";
  ClearBufferQueueLocked();
}

void ShellDemuxerStream::Stop() {
  DCHECK_EQ(MessageLoop::current(), demuxer_->message_loop());
  base::AutoLock auto_lock(lock_);
  ClearBufferQueueLocked();
  for (ReadQueue::iterator it = read_queue_.begin();
       it != read_queue_.end(); ++it) {
    it->Run(DemuxerStream::kOk,
            scoped_refptr<DecoderBuffer>(DecoderBuffer::CreateEOSBuffer()));
  }
  read_queue_.clear();
  stopped_ = true;
}

//
// ShellDemuxer
//
ShellDemuxer::ShellDemuxer(MessageLoop* message_loop,
                           const scoped_refptr<DataSource> &data_source)
    : audio_decoder_config_known_(false)
    , blocking_read_event_(false, false)
    , data_source_(data_source)
    , flv_tag_size_(0)
    , nal_header_size_(0)
    , is_flv_(false)
    , is_mp4_(false)
    , last_read_bytes_(0)
    , message_loop_(message_loop)
    , metadata_known_(false)
    , read_has_failed_(false)
    , read_position_(0)
    , start_time_(kNoTimestamp())
    , video_decoder_config_known_(false)
    , video_buffer_padding_(0) {
  DCHECK(data_source_);
  DCHECK(message_loop_);
}

void ShellDemuxer::PostDemuxTask() {
  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&ShellDemuxer::DemuxTask, this));
}

bool ShellDemuxer::StreamsHavePendingReads() {
  if (audio_demuxer_stream_ && audio_demuxer_stream_->HasPendingReads()) {
    return true;
  }
  if (video_demuxer_stream_ && video_demuxer_stream_->HasPendingReads()) {
    return true;
  }
  return false;
}

void ShellDemuxer::DemuxTask() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  // if neither stream is waiting on data we don't need to demux anymore, return
  if (!StreamsHavePendingReads()) {
    return;
  }

  // demux a single item from the input and enqueue it. If error/EOS we send
  // EOS to both DemuxerStreams
  if (!DemuxSingleItem()) {
    if (audio_demuxer_stream_) {
      audio_demuxer_stream_->EnqueueBuffer(DecoderBuffer::CreateEOSBuffer());
    }
    if (video_demuxer_stream_) {
      video_demuxer_stream_->EnqueueBuffer(DecoderBuffer::CreateEOSBuffer());
    }
  }

  // if an additional request for data has come in to the demux streams
  // automatically post another DemuxTask
  if (StreamsHavePendingReads()) {
    PostDemuxTask();
  }
}

bool ShellDemuxer::DemuxSingleItem() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  if (is_flv_) {
    return DemuxFLVTag();
  } else if (is_mp4_) {
    // TODO: demux mp4
  } else {
    NOTREACHED() << "attempting to demux unknown format!";
  }
  return false;
}

// ===== FLV demuxing constants

// FLV tag constants
static const uint8 kFlashAudioTagType = 8;
static const uint8 kFlashVideoTagType = 9;
static const uint8 kFlashScriptTagType = 18;
// Flash AUDIODATA tag constants
static const uint8 kFlashSoundFormatTypeAAC = 10;
// Flash AACAUDIODATA tag constants
static const uint8 kFlashAACPacketTypeSequence = 0;
static const uint8 kFlashAACPacketTypeRaw = 1;
// Flash VIDEODATA tag constants
static const uint8 kFlashCodecIDAVC = 7;
// Flash AVCVIDEODATA tag constants
static const uint8 kFlashAVCPacketTypeSequenceHeader = 0;
static const uint8 kFlashAVCPacketTypeNALU = 1;
static const uint8 kFlashAVCPacketTypeEndOfSequence = 2;

// TODO: I think mp4 standard defines actual sizes for the configuration
// NALU, replace these estimated values with real ones
static const int kFlashVideoConfigPayloadMaxSize = 1024;
static const int kFlashVideoConfigPayloadMinSize = 8;

// size of generic FLV tag, followed by either AUDIODATA, VIDEODATA, or
// SCRIPTDATA tag. All units are in bytes.
static const int kFlashFLVTagSize = 11;
static const int kFlashAudioDataTagSize = 1;
static const int kFlashAACAudioDataTagSize = 1;
static const int kFlashVideoDataTagSize = 1;
static const int kFlashAVCVideoPacketTagSize = 4;
// To limit reads we download a bit of extra data in flash headers to make
// sure we get all of the tag information we might need, allowing us
// to download the actual encoded data directly into a decoder buffer with
// proper alignment on a subsequent read. This value is calculated as the sum of
// 4 bytes for the previous tag size UI32, the FLV tag size, the VIDEODATA tag
// size, and the AVCVIDEOPACKET tag size, which is the maximum amount of tag
// data that we will need to parse before download encoded A/V data.
static const int kFlashTagDownloadSize =
    4 + kFlashFLVTagSize + kFlashVideoDataTagSize + kFlashAVCVideoPacketTagSize;

// Sampling frequency index according to subclause 1.6.3.4 in ISO14496-3.
static const int kAACSamplingFrequencyIndex[] = {
  96000,
  88200,
  64000,
  48000,
  44100,
  32000,
  24000,
  22050,
  16000,
  12000,
  11025,
  8000,
  7350,
  -1,
  -1,
  -1
};

// ISO 14496-10 describes a byte encoding format for NAL (network abstraction
// layer) and rules to convert it into a RBSP (raw byte stream p??) stream,
// which is the format that many other atoms are defined. This class takes
// a non-owning reference to a buffer and returns various types from the
// stream while silently consuming the extra encoding bytes and advancing
// a bit stream pointer.
class NALUToRBSPStream {
 public:
  // NON-OWNING pointer to buffer. It is assumed the client will dispose of
  // this buffer.
  NALUToRBSPStream(const uint8* nalu_buffer, size_t nalu_buffer_size)
     : nalu_buffer_(nalu_buffer)
     , nalu_buffer_size_(nalu_buffer_size)
     , nalu_buffer_byte_offset_(1)  // skip first byte, it's a NALU header
     , current_nalu_byte_(0)
     , at_first_coded_zero_(false)
     , rbsp_bit_offset_(0) { }

  // read unsigned Exp-Golomb coded integer, ISO 14496-10 Section 9.1
  uint32 ReadUEV() {
    int leading_zero_bits = -1;
    for (uint8 b = 0; b == 0; leading_zero_bits++) {
      b = ReadRBSPBit();
    }
    // we can only fit 16 bits of Exp-Golomb coded data into a 32-bit number
    DCHECK_LE(leading_zero_bits, 16);
    uint32 result = (1 << leading_zero_bits) - 1;
    result += ReadBits(leading_zero_bits);
    return result;
  }

  // read signed Exp-Golomb coded integer, ISO 14496-10 Section 9.1
  int32 ReadSEV() {
    // we start off by reading an unsigned Exp-Golomb coded number
    uint32 uev = ReadUEV();
    // the LSb in this number is treated as the sign bit
    bool is_negative = uev & 1;
    int32 result = (int32)(uev >> 1);
    if (is_negative) result = result * -1;
    return result;
  }

  // read and return up to 32 bits, filling from the right, meaning that
  // ReadBits(17) on a stream of all 1s would return 0x01ff
  uint32 ReadBits(size_t bits) {
    DCHECK_LE(bits, 32);
    uint32 result = 0;
    size_t bytes = bits >> 3;
    // read bytes first
    for (int i = 0; i < bytes; i++) {
      result = result << 8;
      result = result | (uint32)ReadRBSPByte();
    }
    // scoot any leftover bits in
    bits = bits % 8;
    for (int i = 0; i < bits; i++) {
      result = result << 1;
      result = result | (uint32)ReadRBSPBit();
    }
    return result;
  }

  uint8 ReadByte() {
    return ReadRBSPByte();
  }

  uint8 ReadBit() {
    return ReadRBSPBit();
  }

  // jump over bytes in the RBSP stream
  void SkipBytes(size_t bytes) {
    for (int i = 0; i < bytes; ++i) {
      ConsumeNALUByte();
    }
  }

  // jump over bits in the RBSP stream
  void SkipBits(size_t bits) {
    // skip bytes first
    size_t bytes = bits >> 3;
    if (bytes > 0) {
      SkipBytes(bytes);
    }
    // mask off byte skips
    bits = bits & 7;
    // if no bits left to skip just return
    if (bits == 0) {
      return;
    }
    // obey the convention that if our bit offset is 0 we haven't loaded the
    // current byte, extract it from NALU stream as we are going to advance
    // the bit cursor in to it (or potentially past it)
    if (rbsp_bit_offset_ == 0) {
      ConsumeNALUByte();
    }
    // add to our bit offset
    rbsp_bit_offset_ += bits;
    // if we jumped in to the next byte advance the NALU stream, respecting the
    // convention that if we're at 8 bits stay on the current byte
    if (rbsp_bit_offset_ >= 9) {
      ConsumeNALUByte();
    }
    rbsp_bit_offset_ = rbsp_bit_offset_ % 8;
  }

 private:
  // advance by one byte through the NALU buffer, respecting the encoding of
  // 00 00 03 => 00 00. Updates the state of current_nalu_byte_ to the new value.
  void ConsumeNALUByte() {
    DCHECK_LT(nalu_buffer_byte_offset_, nalu_buffer_size_);
    if (at_first_coded_zero_) {
      current_nalu_byte_ = 0;
      at_first_coded_zero_ = false;
    } else if (nalu_buffer_byte_offset_ + 2 < nalu_buffer_size_   &&
               nalu_buffer_[nalu_buffer_byte_offset_]     == 0x00 &&
               nalu_buffer_[nalu_buffer_byte_offset_ + 1] == 0x00 &&
               nalu_buffer_[nalu_buffer_byte_offset_ + 2] == 0x03) {
      nalu_buffer_byte_offset_ += 3;
      current_nalu_byte_ = 0;
      at_first_coded_zero_ = true;
    } else {
      current_nalu_byte_ = nalu_buffer_[nalu_buffer_byte_offset_];
      nalu_buffer_byte_offset_++;
    }
  }

  // return single bit in the LSb from the RBSP stream. Bits are read from MSb
  // to LSb in the stream.
  uint8 ReadRBSPBit() {
    // check to see if we need to consume a fresh byte
    if (rbsp_bit_offset_ == 0) {
      ConsumeNALUByte();
    }
    // since we read from MSb to LSb in stream we shift right
    uint8 bit = (current_nalu_byte_ >> (7 - rbsp_bit_offset_)) & 1;
    // increment bit offset
    rbsp_bit_offset_ = (rbsp_bit_offset_ + 1) % 8;
    return bit;
  }

  uint8 ReadRBSPByte() {
    // fast path for byte-aligned access
    if (rbsp_bit_offset_ == 0) {
      ConsumeNALUByte();
      return current_nalu_byte_;
    }
    // at least some of the bits in the current byte will be included in this
    // next byte, absorb them
    uint8 upper_part = current_nalu_byte_;
    // read next byte from stream
    ConsumeNALUByte();
    // form the byte from the two bytes
    return (upper_part << rbsp_bit_offset_) |
           (current_nalu_byte_ >> (7 - rbsp_bit_offset_));
  }

  const uint8* nalu_buffer_;
  size_t nalu_buffer_size_;
  size_t nalu_buffer_byte_offset_;

  uint8 current_nalu_byte_;
  bool at_first_coded_zero_;

  // location of rbsp bit cursor within current_nalu_byte_
  size_t rbsp_bit_offset_;
};


void ShellDemuxer::ParseAVCConfigRecord(uint8 *avc_config,
                                        size_t record_size) {
  // anything less than 8 bytes just really doesn't make sense
  if (record_size < 8) {
    DLOG(WARNING) << "AVCConfigRecord less than minimum size.";
    return;
  }
  // get the NALU header size
  nal_header_size_ = (avc_config[4] & 0x03) + 1;
  // AVCConfigRecords contain a variable number of SPS NALU
  // (Sequence Parameter Set) (Network Abstraction Layer Units)
  // from which we can extract width, height, and cropping info.
  // That means we need at least 1 SPS NALU in this stream for extraction.
  uint8 number_of_sps_nalus = avc_config[5] & 0x1f;
  if (number_of_sps_nalus == 0) {
    DLOG(WARNING) << "got AVCConfigRecord without any SPS NALUs!";
    return;
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
    if (record_offset + 2 > record_size) {
      DLOG(WARNING) << "ran out of AVCConfig record while parsing SPS size.";
      return;
    }
    // extract 2-byte size of this SPS
    size_t sps_size = (avc_config[record_offset] << 8) |
                      (avc_config[record_offset+1]);
    // advance past the 2-byte size record
    record_offset += 2;
    // see if we jumped over record size
    if (record_offset + sps_size > record_size) {
      DLOG(WARNING) << "ran out of AVCConfig record while parsing SPS blocks.";
      return;
    }
    if (!have_valid_sps && sps_size <= kShellMaxSPSSize) {
      have_valid_sps = true;
      // save size and offset for later copying and parsing
      usable_sps_size = sps_size;
      usable_sps_offset = record_offset;
    }
    record_offset += sps_size;
  }
  if (!have_valid_sps) {
    DLOG(WARNING) <<
        "unable to parse a suitable SPS. Perhaps increase max size?";
    return;
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
  if (record_offset + 1 < record_size) {
    uint8 number_of_pps_nalus = avc_config[record_offset];
    record_offset++;
    for (uint8 i = 0; i < number_of_pps_nalus; i++) {
      // make sure we don't run out of room for 2-byte size record
      if (record_offset + 2 >= record_size) {
        DLOG(WARNING) << "ran out of AVCConfig record while parsing PPS size.";
        return;
      }
      // extract 2-byte size of this PPS
      size_t pps_size = (avc_config[record_offset] << 8) |
                        (avc_config[record_offset + 1]);
      record_offset += 2;
      // see if there's actually room for this record in the buffer
      if (record_offset + pps_size > record_size) {
        DLOG(WARNING) <<
            "ran out of AVCConfig record while scanning PPS blocks.";
        return;
      }
      if (!have_valid_pps && pps_size <= kShellMaxPPSSize) {
        have_valid_pps = true;
        usable_pps_size = pps_size;
        usable_pps_offset = record_offset;
        break;
      }
    }
  }
  // now we parse the valid SPS we extracted from byt stream earlier.
  // first convert SPS NALU to RBSP stream
  NALUToRBSPStream s(avc_config + usable_sps_offset, usable_sps_size);
  uint8 profile_idc = s.ReadByte();
  // skip 3 constraint flags, 5 reserved bits, and level_idc (16 bits)
  s.SkipBytes(2);
  // seq_parameter_set_id
  s.ReadUEV();
/*
  // TODO: figure out why Max has this in his code but it's not
  // in the standard
  if (profile_idc == 100 || profile_idc == 103 ||
      profile_idc == 110 || profile_idc == 122 ||
      profile_idc == 244 || profile_idc == 44 ||
      profile_idc == 83 || profile_idc == 86 ||
      profile_idc == 118) {
    uint32 chroma_format_idc = s.ReadUEV();
    if (chroma_format_idc == 3) {
      // separate_color_plane_flag
      s.SkipBits(1);
    }
    // bit_depth_luma_minus8
    s.ReadUEV();
    // bit_depth_chroma_minus8
    s.ReadUEV();
    // qpprime_y_zero_transform_bypass_flag
    s.SkipBits(1);
    // seq_scaling_matrix_present_flag
    uint8 seq_scaling_matrix_present_flag = s.ReadBit();
    if (seq_scaling_matrix_present_flag) {
      // seq_scaling_list_present_flag[]
      s.SkipBits(chroma_format_idc != 3 ? 8 : 12);
    }
  }
*/
  // log2_max_frame_num_minus4
  s.ReadUEV();
  // pic_order_cnt_type
  uint32 pic_order_cnt_type  = s.ReadUEV();
  if (pic_order_cnt_type == 0) {
    // log2_max_pic_order_cnt_lsb_minus4
    s.ReadUEV();
  } else if (pic_order_cnt_type == 1) {
    // delta_pic_order_always_zero_flag
    s.SkipBits(1);
    // offset_for_non_ref_pic
    s.ReadSEV();
    // offset_for_top_to_bottom_field
    s.ReadSEV();
    // num_ref_frames_in_pic_order_cnt_cycle
    uint32 num_ref_frames_in_pic_order_cnt_cycle = s.ReadUEV();
    for (uint32 i = 0;
         i < num_ref_frames_in_pic_order_cnt_cycle; ++i) {
      s.ReadSEV();
    }
  }
  uint32 num_ref_frames = s.ReadUEV();
  // gaps_in_frame_num_value_allowed_flag
  s.SkipBits(1);
  // width is calculated from pic_width_in_mbs_minus1
  uint32 width = (s.ReadUEV() + 1) * 16;  // 16 pxs per macroblock
  // pic_height_in_map_units_minus1
  uint32 pic_height_in_map_units_minus1 = s.ReadUEV();
  uint32 frame_mbs_only_flag = (uint32)s.ReadBit();
  uint32 height = (2 - frame_mbs_only_flag)
                  * (pic_height_in_map_units_minus1 + 1)
                  * 16;
  if (!frame_mbs_only_flag) {
    s.SkipBits(1);
  }
  // direct_8x8_inference_flag
  s.SkipBits(1);
  // frame cropping flag
  uint8 frame_cropping_flag = s.ReadBit();
  uint32 crop_left, crop_right, crop_top, crop_bottom;
  if (frame_cropping_flag) {
    crop_left = s.ReadUEV() * 2;
    crop_right = s.ReadUEV() * 2;
    crop_top = s.ReadUEV() * 2;
    crop_bottom = s.ReadUEV() * 2;
  } else {
    crop_left = 0;
    crop_right = 0;
    crop_top = 0;
    crop_bottom = 0;
  }
  // remainder of SPS are values we can safely ignore

  // populate extra data structure for copying into config
  VideoConfigExtraData extra_data;
  extra_data.num_ref_frames = num_ref_frames;
  memcpy(&extra_data.sps, avc_config + usable_sps_offset, usable_sps_size);
  extra_data.sps_size = usable_sps_size;
  if (have_valid_pps) {
    memcpy(extra_data.pps, avc_config + usable_pps_offset, usable_pps_size);
  }
  // can be 0 if we didn't find one
  extra_data.pps_size = usable_pps_size;
  extra_data.nal_header_size = nal_header_size_;
  // we can now initialize our video decoder config
  video_decoder_config_.Initialize(
      kCodecH264,
      H264PROFILE_MAIN,  // profile is ignored currently
      VideoFrame::NATIVE_TEXTURE,  // we always decode directly to texture
      gfx::Size(width, height),
      gfx::Rect(crop_left, crop_top, width - crop_right, height - crop_bottom),
      // framerate of zero is provided to signal that the decoder
      // should trust demuxer timestamps
      0, 1,
      // for now assume square pixels
      1, 1,
      // attach our extra data structure
      (const uint8*)&extra_data, sizeof(VideoConfigExtraData),
      // ignore stats for now
      false);
  video_buffer_padding_ =
      ShellVideoDecoder::GetHeaderSizeBytes(video_decoder_config_);
  if (video_buffer_padding_ >= 0) {
    video_decoder_config_known_ = true;
  }
}

// Download individual video NALUs from an FLV AVCVIDEOPACKET tag and enqueue
// them into our video DemuxerStream. Returns false on download error.
// stream_position should be the byte offset within the FLV stream where the
// Data member of the AVCVIDEOPACKET starts. tag_avc_data_size should be the
// size of that Data member. timestamp will be applied to all DecoderBuffers
// extracted and should have been extracted from the FLV timestamp and
// composition time offset tags.
bool ShellDemuxer::ExtractAndEnqueueVideoNALUs(uint64_t stream_position,
                                               size_t tag_avc_data_size,
                                               base::TimeDelta timestamp) {
  DCHECK(video_decoder_config_known_);
  int stream_offset = 0;
  while (stream_offset + nal_header_size_ < tag_avc_data_size) {
    uint8 size_buffer[4];
    // download size of next NALU
    int bytes_read = BlockingRead(
        stream_position + stream_offset,
        nal_header_size_,
        size_buffer);
    if (bytes_read != nal_header_size_) {
      DLOG(WARNING) << "error downloading FLV Video NALU size.";
      return false;
    }
    // extract size
    uint32_t nal_size = 0;
    switch(nal_header_size_) {
      case 1:
        nal_size = size_buffer[0];
        break;
      case 2:
        nal_size = (size_buffer[0] << 8) |
                   (size_buffer[1]);
        break;
      case 4:
        nal_size = (size_buffer[0] << 24) |
                   (size_buffer[1] << 16) |
                   (size_buffer[2] << 8)  |
                   (size_buffer[3]);
        break;
      default:
        NOTREACHED();
        break;
    }
    stream_offset += nal_header_size_;
    // extract NAL into buffer with padding
    if (stream_offset + nal_size > tag_avc_data_size) {
      DLOG(WARNING) << "ran out of video buffer while trying to extract NALU.";
      return false;
    }
    if (nal_size > 0 && nal_size < ShellVideoDecoderBufferMaxSize) {
      // create a decoder buffer, download the bytes, and enqueue
      scoped_refptr<DecoderBuffer> buffer =
        ShellBufferFactory::GetVideoDecoderBuffer();
        bytes_read = BlockingRead(
          stream_position + stream_offset,
          nal_size,
          buffer->GetWritableData() + video_buffer_padding_);
      if (bytes_read != nal_size) {
        // recycle buffer
        ShellBufferFactory::ReturnVideoDecoderBuffer(buffer);
        // report error
        DLOG(WARNING) << base::StringPrintf(
            "failed to download FLV video download %d bytes, got %d bytes.",
            nal_size, bytes_read);
        return false;
      }
      buffer->SetDataSize(nal_size + video_buffer_padding_);
      buffer->SetTimestamp(timestamp);
      video_demuxer_stream_->EnqueueBuffer(buffer);
    } else {
      DLOG(WARNING) << base::StringPrintf(
          "bad size on FLV video NALU of %d bytes, skipped.",
          nal_size);
    }
    // move buffer_offset forward past the NALU
    stream_offset += nal_size;
  }
  // normal termination means no error
  return true;
}

bool ShellDemuxer::DemuxFLVTag() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  uint8 tag_buffer[kFlashTagDownloadSize];

  // get previous tag size and header for next one
  int bytes_read = BlockingRead(read_position_, kFlashTagDownloadSize,
      (uint8*)&tag_buffer);

  // if that was the last tag in the stream detect the EOS and return. This
  // is where normal termination of an FLV stream will occur.
  if (bytes_read < kFlashTagDownloadSize)
    return false;

  // move our read pointer past the previous size (4 bytes) and the FLV tag
  read_position_ += 4 + kFlashFLVTagSize;

  // first four bytes of tag_buffer are going to be the size of the previous
  // tag, including the length of the tag header. Compare with previous tag
  // size
  uint32 previous_tag_size = tag_buffer[0] << 24
                           | tag_buffer[1] << 16
                           | tag_buffer[2] << 8
                           | tag_buffer[3];

  // if there's a tag size mismatch we've lost sync, end the stream
  if (previous_tag_size != flv_tag_size_) {
    DLOG(WARNING) << base::StringPrintf(
        "FLV tag size mistmatch, last tag size: %d, parsed tag size: %d",
        flv_tag_size_, previous_tag_size);
    return false;
  }

  // extract the tag data size from the tag header as u24
  // this is size of attached data field only not including this header
  // but including the audio and video sub-headers
  uint32 tag_data_size = tag_buffer[5] << 16
                       | tag_buffer[6] << 8
                       | tag_buffer[7];

  // extract timestamp, byte order comes from the standard
  int32 timestamp = tag_buffer[8] << 16
                  | tag_buffer[9] << 8
                  | tag_buffer[10]
                  | tag_buffer[11] << 24;

  // now check which type of tag this is
  if (tag_buffer[4] == kFlashAudioTagType) {
    // read single-byte AUDIODATA tag
    uint8 audio_tag = tag_buffer[15];
    // 4 MSb of audio_tag indicate format, we support only AAC
    if (((audio_tag >> 4) & 0x0f) == kFlashSoundFormatTypeAAC) {
      // According to FLV standard AAC audio is always 44kHz, 16 bit stereo.
      // The subsequent byte in the header is the AACAUDIODATA tag, which
      // identifies if these data are an AudioSpecificConfig or raw AAC data
      uint8 aac_audio_tag = tag_buffer[16];
      if (aac_audio_tag == kFlashAACPacketTypeSequence) {
        // AudioSpecificConfig records can be longer than two bytes but we
        // extract everything we need from the first two bytes, positioned
        // here at index 17 and 18 in the tag_buffer.
        if (tag_data_size >= 2) {
          // see http://wiki.multimedia.cx/index.php?title=Understanding_AAC for
          // best description of bit packing within these two bytes, basically
          // aaaaabbb bccccdef
          // aaaaa: 5 bits of aac object type
          // bbbb: 4 bits of frequency table index
          // cccc: 4 bits of channel config index
          uint8 aac_object_type = (tag_buffer[17] >> 3) & 0x1f;
          uint8 sample_freq_index = ((tag_buffer[17] & 0x07) << 1) |
                                    ((tag_buffer[18] & 0x80) >> 7) & 0x0f;
          int frequency = kAACSamplingFrequencyIndex[sample_freq_index];
          uint8 channel_count = ((tag_buffer[18] & 0x78) >> 3) & 0x0f;
          ChannelLayout channel_layout =
              mp4::AVC::ConvertAACChannelCountToChannelLayout(channel_count);
          AudioConfigExtraData extra_data;
          extra_data.channel_count = channel_count;
          extra_data.aac_object_type = aac_object_type;
          extra_data.aac_sampling_frequency_index = sample_freq_index;
          audio_decoder_config_.Initialize(kCodecAAC,
                                           16,   // FLV AAC is always 16 bit
                                           channel_layout,
                                           frequency,
                                           (uint8*)(&extra_data),
                                           sizeof(AudioConfigExtraData),
                                           false);
          audio_decoder_config_known_ = true;
        } else {
          DLOG(WARNING) << "AACAUDIODATA packet shorter than two bytes.";
        }
      } else if (aac_audio_tag == kFlashAACPacketTypeRaw &&
                 audio_decoder_config_known_) {
        // actual payload size is size of FLV tag data minus size of
        // audio and AAC tags
        int download_size = tag_data_size -
            (kFlashAudioDataTagSize + kFlashAACAudioDataTagSize);
        if (download_size < ShellAudioDecoderBufferMaxSize) {
          // rather then have to recopy this buffer again to frame it in
          // a platform-specific decoder envelope we will simply offset
          // these data into the decoder buffer leaving room at the top
          // for those data.
          int audio_buffer_padding =
              ShellAudioDecoder::GetHeaderSizeBytes(audio_decoder_config_);
          //  fill audio decoderbuffer
          scoped_refptr<DecoderBuffer> buffer =
            ShellBufferFactory::GetAudioDecoderBuffer();
          // set data size
          buffer->SetDataSize(download_size + audio_buffer_padding);
          int bytes_read = BlockingRead(
              read_position_ +
                  kFlashAudioDataTagSize + kFlashAACAudioDataTagSize,
              download_size,
              buffer->GetWritableData() + audio_buffer_padding);

          if (bytes_read < download_size) {
            // return the audio buffer to the pool
            ShellBufferFactory::ReturnAudioDecoderBuffer(buffer);
            DLOG(WARNING) << base::StringPrintf(
                "error downloading FLV audio tag, expected %d bytes, got %d.",
                download_size, bytes_read);
            // return error
            return false;
          }

          // set timestamp on audio buffer
          buffer->SetTimestamp(base::TimeDelta::FromMilliseconds(timestamp));
          // add the buffer to the audio demux stream
          audio_demuxer_stream_->EnqueueBuffer(buffer);
        } else {
          DLOG(WARNING) << base::StringPrintf(
              "FLV AAC audio payload too large, ignoring: %d bytes", download_size);
        }
      } else {
        DLOG(WARNING) << base::StringPrintf(
          "unknown FLV AACAUDIODATA AACPacketType: %d", aac_audio_tag);
      }
    } else {
      DLOG(WARNING) << base::StringPrintf(
          "unsupported FLV AUDIODATA sound format type: %d",
          (audio_tag >> 4) & 0x0f);
    }
  } else if (tag_buffer[4] == kFlashVideoTagType) {
    // read single-byte VIDEODATA tag
    uint8 video_tag = tag_buffer[15];
    bool is_keyframe = (video_tag & 0xf0) == 0x10;
    // check for AVC format
    if ((video_tag & 0x0f) == kFlashCodecIDAVC) {
      // determine packet type
      uint8 avc_packet_type = tag_buffer[16];
      if (avc_packet_type == kFlashAVCPacketTypeSequenceHeader) {
        int download_size = tag_data_size -
            (kFlashVideoDataTagSize + kFlashAVCVideoPacketTagSize);
        if (download_size <= kFlashVideoConfigPayloadMaxSize &&
            download_size >= kFlashVideoConfigPayloadMinSize) {
          // this has the same structure as the contents of a avcC atom in mp4.
          // request entire tag for parsing.
          // TODO: re-use this buffer!
          uint8* avc_config = (uint8*)malloc(download_size);
          int bytes_read = BlockingRead(
              read_position_ +
                  kFlashVideoDataTagSize + kFlashAVCVideoPacketTagSize,
              download_size,
              avc_config);
          // make sure we downloaded the whole config block successfully
          if (bytes_read < download_size) {
            free(avc_config);
            DLOG(WARNING) << base::StringPrintf(
                "error downloading FLV video config tag of %d bytes, got %d",
                download_size, bytes_read);
            return false;
          }
          ParseAVCConfigRecord(avc_config, bytes_read);
          // dispose of the avc_config buffer
          free(avc_config);
        } else {
          DLOG(WARNING) << base::StringPrintf(
              "FLV AVCVIDEODATA configuration packet bad size: %d",
              download_size);
        }
      } else if (avc_packet_type == kFlashAVCPacketTypeNALU &&
                 video_decoder_config_known_) {
        // extract composition time offset for this frame
        int32 composition_time_offset = tag_buffer[17] << 16
                                      | tag_buffer[18] << 8
                                      | tag_buffer[19];
        // sign extend the signed 24-bit integer to 32 bits
        composition_time_offset = (composition_time_offset << 8) >> 8;
        // calculate size of video tag payload
        int tag_avc_data_size = tag_data_size -
            (kFlashVideoDataTagSize + kFlashAVCVideoPacketTagSize);
        // extract and enqueue video nalus into DecoderBuffers. Propagate any
        // download error back to calling function.
        if (!ExtractAndEnqueueVideoNALUs(
                read_position_ +
                    kFlashVideoDataTagSize + kFlashAVCVideoPacketTagSize,
                tag_avc_data_size,
                base::TimeDelta::FromMilliseconds(
                timestamp + composition_time_offset))) {
          return false;
        }
      } else if (avc_packet_type == kFlashAVCPacketTypeEndOfSequence) {
        // construct end of stream or something? - there's no data payload here
      } else {
        DLOG(WARNING) << "unrecognized FLV AVCAUDIODATA AVCPacketType.";
      }
    } else {
      DLOG(WARNING) << base::StringPrintf(
          "unsupported FLV VIDEODATA CodecID: %d", video_tag & 0x0f);
    }
  } else if (tag_buffer[4] == kFlashScriptTagType) {
    // SCRIPT tag - ignore
  } else {
    DLOG(WARNING) << base::StringPrintf("unsupported FLV TagType %d",
        tag_buffer[4]);
  }
  // advance read pointer to next tag header
  read_position_ += tag_data_size;
  // update the previous tag size to our size
  flv_tag_size_ = tag_data_size + kFlashFLVTagSize;
  // normal operation, return true!
  return true;
}

void ShellDemuxer::Initialize(DemuxerHost* host,
                              const PipelineStatusCB& status_cb) {
  message_loop_->PostTask(FROM_HERE, base::Bind(&ShellDemuxer::InitializeTask,
                                                this,
                                                host,
                                                status_cb));
}

void ShellDemuxer::InitializeTask(DemuxerHost *host,
                                  const PipelineStatusCB &status_cb) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  // safe to call multiple times
  ShellBufferFactory::Initialize();

  host_ = host;
  data_source_->set_host(host);

  // read first 16 bytes of stream to determine if mp4 format or flv
  uint8 header[16];
  int bytes_read = BlockingRead(0, 16, (uint8*)&header);

  // if we can't even read the first 16 bytes there's a serious problem
  if (bytes_read < 16) {
    status_cb.Run(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  // detect flv
  is_flv_ = (header[0] == 'F' &&
             header[1] == 'L' &&
             header[2] == 'V');
  // detect mp4
  is_mp4_ = (header[4] == 'f' &&
             header[5] == 't' &&
             header[6] == 'y' &&
             header[7] == 'p');
  // if it's both we have problems :)
  DCHECK(!(is_flv_ && is_mp4_));

  if (is_mp4_) {
    // yaya
    NOTREACHED() << "MP4 demux not implemented!";
  } else if (is_flv_) {
    // ensure availability of both audio and video stream
    uint8_t type_flags = header[4];
    bool has_audio = (((type_flags >> 2) & 1) == 1);
    bool has_video = ((type_flags & 1) == 1);
    if (!has_audio && !has_video) {
      status_cb.Run(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
      return;
    }
    // Read the data offset as big endian u32. This is the position in the
    // stream of the PreviousTagSize0 u32.
    uint32_t data_offset = header[5] << 24
                         | header[6] << 16
                         | header[7] << 8
                         | header[8];
    read_position_ = static_cast<uint64_t>(data_offset);

    // start_time_ for FLV is always 0 as all timestamps are relative to 0
    start_time_ = base::TimeDelta();

    // TODO: kick off FLV metadata request, callback will post a task asking
    // for metadata to be parsed

  } else {
    // if it's neither an flv or an mp4 container than we don't support it
    status_cb.Run(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
    return;
  }

  // create audio and video demuxer stream objects
  audio_demuxer_stream_ = new ShellDemuxerStream(this, DemuxerStream::AUDIO);
  video_demuxer_stream_ = new ShellDemuxerStream(this, DemuxerStream::VIDEO);

  // demux packets to the streams until we have found a valid audio and video
  // configuration or we've downloaded too many bytes
  while (!(audio_decoder_config_known_ && video_decoder_config_known_) &&
      read_position_ < kUnconfigStreamMaxSizeBytes) {
    DemuxSingleItem();
  }

  // if we didn't find valid audio and video config this is an error, abort
  if (!audio_decoder_config_known_ || !video_decoder_config_known_) {
    DLOG(WARNING) <<
        "downloaded more than kUnconfigStreamMaxBytes without finding config.";
    status_cb.Run(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  // shell demuxer supports deferred loading of FLV metadata loading from a
  // separate URL. MP4 also does not require that the MOOV atom be the first
  // atom in the stream. Although all YouTube videos are well-behaved in this
  // sense some ad videos aren't as consistent. As a result we support deferred
  // loading of movie metadata. However we can't accurately know the bitrate
  // or duration of a video until that metatdata arrives. If we haven't received
  // accurate metadata set bitrate to a reasonable average for 1080p H.264 and
  // set duration to infinite.
  if (!metadata_known_) {
    // could possibly extract approximate (and maybe wrong) values from
    // the SCRIPT object in flash?
    host_->SetDuration(kInfiniteDuration());
  }

  status_cb.Run(PIPELINE_OK);
}

int ShellDemuxer::BlockingRead(int64 position, int size, uint8* data) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  DCHECK(host_);
  DCHECK(data_source_);

  // don't even try to read once a read has failed
  if (read_has_failed_)
    return -1;

  // check for reads at or past EOF
  int64 file_size;
  if (data_source_->GetSize(&file_size) && position >= file_size) {
    return 0;
  }

  // have data_source_ perform the read, calling us back to signal our event
  data_source_->Read(position,
                     size,
                     data,
                     base::Bind(&ShellDemuxer::SignalReadCompleted, this));

  int bytes_read = WaitForRead();

  // check for error, set appropriate flags and return -1 if needed
  if (bytes_read == DataSource::kReadError) {
    host_->OnDemuxerError(PIPELINE_ERROR_READ);
    read_has_failed_ = true;
    return -1;
  }

  return bytes_read;
}

int ShellDemuxer::WaitForRead() {
  blocking_read_event_.Wait();
  return last_read_bytes_;
}

void ShellDemuxer::SignalReadCompleted(int size) {
  last_read_bytes_ = size;
  blocking_read_event_.Signal();
}

void ShellDemuxer::Stop(const base::Closure &callback) {
  // Post a task to notify the streams to stop as well.
  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&ShellDemuxer::StopTask, this, callback));

  // Then wakes up the thread from reading.
  SignalReadCompleted(DataSource::kReadError);
}

void ShellDemuxer::StopTask(const base::Closure& callback) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  // tell downstream we've stopped
  if (audio_demuxer_stream_) audio_demuxer_stream_->Stop();
  if (video_demuxer_stream_) video_demuxer_stream_->Stop();
  // tell upstream we've stopped and let them callback when finished
  if (data_source_) {
    data_source_->Stop(callback);
  } else {
    // otherwise we're just done, callback ourselves
    callback.Run();
  }
}

void ShellDemuxer::Seek(base::TimeDelta time, const PipelineStatusCB& cb) {
  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&ShellDemuxer::SeekTask, this, time, cb));
}

void ShellDemuxer::SeekTask(base::TimeDelta time, const PipelineStatusCB& cb) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  // flush audio and video buffers
  if (audio_demuxer_stream_) audio_demuxer_stream_->FlushBuffers();
  if (video_demuxer_stream_) video_demuxer_stream_->FlushBuffers();

  // seek the demuxer itself
  NOTIMPLEMENTED();

  // notify seek complete
  cb.Run(PIPELINE_OK);
}

void ShellDemuxer::OnAudioRendererDisabled() {
  NOTIMPLEMENTED();
}

void ShellDemuxer::SetPlaybackRate(float playback_rate) {
  DCHECK(data_source_.get());
  data_source_->SetPlaybackRate(playback_rate);
}

scoped_refptr<DemuxerStream> ShellDemuxer::GetStream(
    media::DemuxerStream::Type type) {
  if (type == DemuxerStream::AUDIO) {
    return audio_demuxer_stream_;
  } else if (type == DemuxerStream::VIDEO) {
    return video_demuxer_stream_;
  } else {
    DLOG(WARNING) << "unsupported stream type requested";
  }
  return NULL;
}

base::TimeDelta ShellDemuxer::GetStartTime() const {
  return start_time_;
}

int ShellDemuxer::GetBitrate() {
  NOTIMPLEMENTED();
  return 0;
}

const AudioDecoderConfig& ShellDemuxer::audio_decoder_config() {
  DCHECK(audio_decoder_config_known_);
  return audio_decoder_config_;
}

const VideoDecoderConfig& ShellDemuxer::video_decoder_config() {
  DCHECK(video_decoder_config_known_);
  return video_decoder_config_;
}

}  // namespace media
