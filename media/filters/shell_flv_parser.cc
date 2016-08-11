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

#include "media/filters/shell_flv_parser.h"

#include <inttypes.h>
#include <limits>

#include "base/stringprintf.h"
#include "media/base/endian_util.h"

namespace media {

// "FLV" as hex ASCII codes
static const uint32 kFLV = 0x00464c56;

// FLV configuration, such as the AVCConfigRecord and the AudioSpecificConfig,
// should proceed any actual encoded data, and should be in the top of the file.
// This constant describes how far into the file we're willing to traverse
// without encountering metadata or encoded video keyframe data before giving
// up.
static const uint64 kMetadataMaxBytes = 4 * 1024 * 1024;

static const uint8 kAudioTagType = 8;
static const uint8 kVideoTagType = 9;
static const uint8 kScriptDataObjectTagType = 18;

// size of standard FLV tag
static const int kTagSize = 11;
// To limit reads we download a bit of extra data in flash headers to make
// sure we get all of the tag information we might need, allowing us
// to download the actual encoded data directly into a decoder buffer with
// proper alignment on a subsequent read. This value is calculated as the sum of
// the FLV tag size, the VIDEODATA tag size, and the AVCVIDEOPACKET tag size,
// which is the maximum amount of tag data that we will need to parse before
// download encoded A/V data.
static const int kTagDownloadSize = kTagSize + 1 + 4;
// these constants describe total tag sizes for AAC/AVC addendums to tags
static const int kAudioTagSize = 2;
static const int kVideoTagSize = 5;

// FLV AUDIODATA tag constants
static const uint8 kSoundFormatTypeAAC = 10;
// FLV AACAUDIODATA tag constants
static const uint8 kAACPacketTypeSequence = 0;
static const uint8 kAACPacketTypeRaw = 1;
// FLV VIDEODATA tag constants
static const uint8 kCodecIDAVC = 7;
// FLV AVCVIDEODATA tag constants
static const uint8 kAVCPacketTypeSequenceHeader = 0;
static const uint8 kAVCPacketTypeNALU = 1;
// Unused:
// static const uint8 kAVCPacketTypeEndOfSequence = 2;

// SCRIPTDATA parsing constants
static const uint8 kAMF0NumberType = 0x00;
static const int kAMF0NumberLength = 9;

// static
scoped_refptr<ShellParser> ShellFLVParser::Construct(
    scoped_refptr<ShellDataSourceReader> reader,
    const uint8* construction_header,
    const PipelineStatusCB& status_cb) {
  // look for "FLV" string at top of file, mask off LSB
  uint32 FLV = endian_util::load_uint32_big_endian(construction_header) >> 8;
  if (FLV != kFLV) {
    return NULL;
  }
  // Check for availability of both an audio and video stream. Audio stream is
  // third bit, video stream is first bit in 5th byte of file header
  if ((construction_header[4] & 0x05) != 0x05) {
    status_cb.Run(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
    return NULL;
  }
  // offset of first data tag in stream is next 4 bytes
  uint32 data_offset =
      endian_util::load_uint32_big_endian(construction_header + 5);
  // add four bytes to skip over PreviousTagSize0
  data_offset += 4;
  // construct and return an FLV parser
  return scoped_refptr<ShellParser>(new ShellFLVParser(reader, data_offset));
}

ShellFLVParser::ShellFLVParser(scoped_refptr<ShellDataSourceReader> reader,
                               uint32 tag_start_offset)
    : ShellAVCParser(reader),
      tag_offset_(tag_start_offset),
      at_end_of_file_(false) {}

ShellFLVParser::~ShellFLVParser() {}

bool ShellFLVParser::ParseConfig() {
  // traverse file until we either reach the limit of bytes we're willing to
  // parse of config info or we've encountered actual keyframe video data.
  while (tag_offset_ < kMetadataMaxBytes && time_to_byte_map_.size() == 0) {
    if (!ParseNextTag()) {
      return false;
    }
  }

  if (duration_.InMilliseconds() == 0) {
    return false;
  }

  // We may have a valid duration by now and the reader may know the
  // length of the file in bytes, see if we can extrapolate a bitrate from
  // this.
  if (duration_ != kInfiniteDuration() && reader_->FileSize() > 0) {
    bits_per_second_ = (uint32)(
        ((reader_->FileSize() * 8000ULL) / (duration_.InMilliseconds())));
  }

  return true;
}

scoped_refptr<ShellAU> ShellFLVParser::GetNextAU(DemuxerStream::Type type) {
  if (type == DemuxerStream::AUDIO) {
    return GetNextAudioAU();
  } else if (type == DemuxerStream::VIDEO) {
    return GetNextVideoAU();
  } else {
    NOTREACHED();
  }
  return NULL;
}

// seeking an flv:
// 1) finding nearest video keyframe before timestamp:
//  a) If we are seeking in to an area we have already parsed then we
//      will find the bounding keyframe.
//  b) If not, we parse the FLV until a) is true.
// 2) set tag_offset_ to the byte offset of the keyframe found in 1)
bool ShellFLVParser::SeekTo(base::TimeDelta timestamp) {
  // convert timestamp to millisecond FLV timestamp
  uint32 timestamp_flv = (uint32)timestamp.InMilliseconds();
  bool found_upper_bound = false;
  uint64 seek_byte_offset = tag_offset_;
  uint32 seek_timestamp = 0;
  // upper_bound returns iterator of first element in container with key > arg
  TimeToByteMap::iterator keyframe_in_map =
      time_to_byte_map_.upper_bound(timestamp_flv);
  // this is case 1a), or keyframe is last keyframe before EOS,
  // or map is empty (error state)
  if (keyframe_in_map == time_to_byte_map_.end()) {
    // is map empty? This is an error case, we should always have found a
    // keyframe during ParseConfig()
    if (time_to_byte_map_.size() == 0) {
      NOTREACHED() << "empty time to byte map on FLV seek";
      return false;
    } else {
      // start at last keyframe in the map and parse from there
      seek_byte_offset = time_to_byte_map_.rbegin()->second;
    }
  } else {
    found_upper_bound = true;
    // it's possible timestamp <= first keyframe in map, in which case we
    // use the first keyframe in map.
    if (keyframe_in_map != time_to_byte_map_.begin()) {
      keyframe_in_map--;
    }
    seek_byte_offset = keyframe_in_map->second;
    seek_timestamp = keyframe_in_map->first;
  }
  // if seek has changed our position in the file jump there now
  if (seek_byte_offset != tag_offset_) {
    JumpParserTo(seek_byte_offset);
  }
  // if found_upper_bound is still false we are in case 1b), parse ahead until
  // we encounter an upper bound or an eof
  while (!found_upper_bound && !at_end_of_file_) {
    // save highest keyframe in file in case it becomes the one we want
    seek_byte_offset = time_to_byte_map_.rbegin()->second;
    seek_timestamp = time_to_byte_map_.rbegin()->first;
    // parse next tag in the file
    if (!ParseNextTag()) {
      return false;
    }
    // check last keyframe timestamp, if it's greater than our target timestamp
    // we can stop
    found_upper_bound = (time_to_byte_map_.rbegin()->first > timestamp_flv);
  }
  // make sure we have done step 2), jump parser to new keyframe
  if (seek_byte_offset != tag_offset_) {
    JumpParserTo(seek_byte_offset);
  }
  DLOG(INFO) << base::StringPrintf("flv parser seeking to timestamp: %" PRId64
                                   " chose keyframe at %d",
                                   timestamp.InMilliseconds(), seek_timestamp);
  return true;
}

scoped_refptr<ShellAU> ShellFLVParser::GetNextAudioAU() {
  // As audio timestamps are supposed to increase monotonically we need
  // only 2 to calculate a duration.
  while (next_audio_aus_.size() < 2) {
    if (!ParseNextTag()) {
      return NULL;
    }
  }
  // There should always be 2 AUs in the queue now, even if they are both EOS.
  DCHECK_GE(next_audio_aus_.size(), 2);

  // Extract first AU in queue
  scoped_refptr<ShellAU> au(next_audio_aus_.front());
  next_audio_aus_.pop_front();
  // Next timestamp should be greater than ours, if not something is very funny
  // with this FLV and we won't be able to calculate duration.
  if (next_audio_aus_.front()->GetTimestamp() >= au->GetTimestamp()) {
    au->SetDuration(next_audio_aus_.front()->GetTimestamp() -
                    au->GetTimestamp());
  } else {
    DLOG(ERROR) << "out of order audio timestamps encountered on FLV parsing.";
  }
  return au;
}

scoped_refptr<ShellAU> ShellFLVParser::GetNextVideoAU() {
  while (next_video_aus_.empty()) {
    if (!ParseNextTag()) {
      return NULL;
    }
  }
  // extract next video AU
  scoped_refptr<ShellAU> au(next_video_aus_.front());
  next_video_aus_.pop_front();

  return au;
}

//
// byte layout of an FLVTAG is:
// field             | type   | comment
// ------------------+--------+---------
// previous tag size | uint32 | we skip past these when parsing last tag
// tag type          | uint8  | parsing starts here. describes tag type
// tag data size     | uint24 | size of tag data payload (everything after this)
// timestamp         | uint24 | lower 24 bits of timestamp in milliseconds
// timestamp ext     | uint8  | upper 8 bits of timestamp in milliseconds
// stream id         | uint24 | always 0
//
bool ShellFLVParser::ParseNextTag() {
  uint8 tag_buffer[kTagDownloadSize];

  if (at_end_of_file_) {
    return false;
  }

  // get previous tag size and header for next one
  int bytes_read =
      reader_->BlockingRead(tag_offset_, kTagDownloadSize, tag_buffer);

  // if that was the last tag in the stream detect the EOS and return. This
  // is where normal termination of an FLV stream will occur.
  if (bytes_read < kTagDownloadSize) {
    at_end_of_file_ = true;
    // Normal termination of an FLV. Enqueue EOS AUs in both streams.
    next_video_aus_.push_back(ShellAU::CreateEndOfStreamAU(
        DemuxerStream::VIDEO, video_track_duration_));
    next_audio_aus_.push_back(ShellAU::CreateEndOfStreamAU(
        DemuxerStream::AUDIO, audio_track_duration_));
    return true;
  }

  // extract the tag data size from the tag header as uint24
  // this is size of attached data field only not including this header
  // but including the audio and video sub-headers
  uint32 tag_data_size =
      endian_util::load_uint32_big_endian(tag_buffer + 1) >> 8;

  // extract timestamp, wonky byte order comes from the standard
  int32 timestamp = tag_buffer[4] << 16 | tag_buffer[5] << 8 | tag_buffer[6] |
                    tag_buffer[7] << 24;

  // choose which tag type to parse
  bool parse_result = true;
  uint8* tag_body = tag_buffer + kTagSize;
  switch (tag_buffer[0]) {
    case kAudioTagType:
      parse_result = ParseAudioDataTag(tag_body, tag_data_size, timestamp);
      break;

    case kVideoTagType:
      parse_result = ParseVideoDataTag(tag_body, tag_data_size, timestamp);
      break;

    case kScriptDataObjectTagType:
      parse_result =
          ParseScriptDataObjectTag(tag_body, tag_data_size, timestamp);
      break;

    default:
      DLOG(WARNING) << base::StringPrintf("unsupported FLV TagType %d",
                                          tag_buffer[0]);
      break;
  }

  // advance read pointer to next tag header
  tag_offset_ += kTagSize + tag_data_size + 4;
  return parse_result;
}

// FLV AUDIODATA tags are packed into a single byte, bit layout is:
//    aaaabbcd
// aaaa: 4 bits format enum, AAC is 10 decimal
// bb: 2 bits sample rate enum, AAC is always 3 decimal (44 KHz)
// c:  1 bit sound size, 0 means 8 bit, 1 means 16 bit, AAC is always 1
// d:  1 bit sound type, 0 means mono, 1 means stereo, AAC is always 1
// if this is an AACAUDIODATA tag the next byte in the sequence 0 if
// this is an AudioSpecificConfig tag or 1 if it is raw AAC frame data.
//
// * NOTE that FLV standard defines fixed values for sample rate, bit
// width, and channel count for AAC samples but the AudioSpecificConfig may
// define those values differently and is authoritative, so we ignore the
// FLV-provided config values.
bool ShellFLVParser::ParseAudioDataTag(uint8* tag,
                                       uint32 size,
                                       uint32 timestamp) {
  // Smallest meaningful size for an audio data tag is 4 bytes, one for the
  // AUDIODATA tag, one for the AACAUDIODATA tag. and minimum 2 bytes of data.
  if (size < kAudioTagSize + 2) {
    return false;
  }
  // we only support parsing AAC audio data tags
  if (((tag[0] >> 4) & 0x0f) != kSoundFormatTypeAAC) {
    return false;
  }
  // now see if this is a config packet or a data packet
  if (tag[1] == kAACPacketTypeSequence) {  // audio config info
    // AudioSpecificConfig records can be longer than two bytes but we extract
    // everything we need from the first two bytes, positioned here at index 2
    // and 3 in the tag buffer
    ParseAudioSpecificConfig(tag[2], tag[3]);
  } else if (tag[1] == kAACPacketTypeRaw) {  // raw AAC audio
    // this is audio data, check timestamp
    base::TimeDelta ts = base::TimeDelta::FromMilliseconds(timestamp);
    if (ts > audio_track_duration_) {
      audio_track_duration_ = ts;
    }
    // build the AU
    size_t prepend_size = CalculatePrependSize(DemuxerStream::AUDIO, true);
    scoped_refptr<ShellAU> au = ShellAU::CreateAudioAU(
        tag_offset_ + kTagSize + kAudioTagSize, size - kAudioTagSize,
        prepend_size, true, ts, kInfiniteDuration(), this);
    next_audio_aus_.push_back(au);
  }

  return true;
}

// FLV VIDEODATA tags are packed into a single byte, bit layout is:
// aaaabbbb
// aaaa: 4 bits frame type enum, 1 is AVC keyframe, 2 is AVC inter-frame
// bbbb: 4 bits codecID, 7 is AVC
// if this is an AVCVIDEOPACKET tag the next 4 bytes comprise the AVCVIDEOPACKET
// tag header:
// field             | type   | comment
// ------------------+--------+---------
// AVCPacketType     | uint8  | 0 is config, 1 is data, 2 is EOS (ignored)
// CompositionTime   | int24  | signed time offset, add to FLV timestamp for pts
//
// NOTE that FLV video data is always presented in decode order and
// CompositionTime is not entirely reliable for determining pts, as some
// encoders always set it to zero.
bool ShellFLVParser::ParseVideoDataTag(uint8* tag,
                                       uint32 size,
                                       uint32 timestamp) {
  // need at least 5 bytes of tag data, one for the VIDEODATA tag and 4 for
  // the AVCVIDEODATA tag that should always follow it
  if (size < kVideoTagSize) {
    return false;
  }
  // check for AVC format
  if ((tag[0] & 0x0f) != kCodecIDAVC) {
    return false;
  }
  // determine packet type
  if (tag[1] == kAVCPacketTypeSequenceHeader) {  // video config info
    // AVC config record, download and parse
    return DownloadAndParseAVCConfigRecord(
        tag_offset_ + kTagSize + kVideoTagSize, size);
  } else if (tag[1] == kAVCPacketTypeNALU) {  // raw AVC data
    // should we add this to our keyframe map?
    bool is_keyframe = (tag[0] & 0xf0) == 0x10;
    // TODO: when we add support for seeking, make sure these numbers are
    // consistent with the numbers provided by the time-to-byte manifest.
    if (is_keyframe) {
      time_to_byte_map_[timestamp] = tag_offset_;
    }
    // extract 24-bit composition time offset in big-endian for this frame
    int32 composition_time_offset = tag[2] * 65536 + tag[3] * 256 + tag[4];
    // calculate pts from flv timestamp and cts
    uint32 pts = timestamp + composition_time_offset;
    // FLV standard says that there can be multiple AVC NALUs packed here, so
    // we iterate through the tag data payload and enqueue byte offsets for
    // each NALU we encounter. The NALUs are packed by size counter that is
    // nal_header_size_ bytes long followed by the NALU of that size.
    uint32 avc_data_size = size - kVideoTagSize;
    uint32 avc_tag_offset = 0;
    base::TimeDelta ts = base::TimeDelta::FromMilliseconds(pts);
    if (ts > video_track_duration_) {
      video_track_duration_ = ts;
    }

    size_t prepend_size =
        CalculatePrependSize(DemuxerStream::VIDEO, is_keyframe);
    scoped_refptr<ShellAU> au = ShellAU::CreateVideoAU(
        tag_offset_ + kTagSize + kVideoTagSize + avc_tag_offset, avc_data_size,
        prepend_size, nal_header_size_, is_keyframe, ts, kInfiniteDuration(),
        this);
    // enqueue data tag
    next_video_aus_.push_back(au);
  }
  return true;
}

// FLV SCRIPTDATA tags are in serialized typically in Action Message Format 0 as
// a collection of key/value pairs terminated by a special code. We only wish to
// parse the duration and byterate from the scriptdata so we use a very light
// weight parser in ExtractAMF0Number();
bool ShellFLVParser::ParseScriptDataObjectTag(uint8* tag,
                                              uint32 size,
                                              uint32 timestamp) {
  scoped_refptr<ShellScopedArray> script_buffer =
      ShellBufferFactory::Instance()->AllocateArray(size);
  if (!script_buffer || !script_buffer->Get()) {
    return false;
  }
  int bytes_read =
      reader_->BlockingRead(tag_offset_ + kTagSize, size, script_buffer->Get());
  DCHECK_LE(size, static_cast<uint32>(std::numeric_limits<int32>::max()));
  if (bytes_read < static_cast<int>(size)) {
    return false;
  }
  // Attempt to extract the duration from the FLV metadata.
  double duration_seconds = 0;
  if (!ExtractAMF0Number(script_buffer, "duration", &duration_seconds)) {
    // might be worth trying to parse this as AMF3?
    return false;
  }
  duration_ = base::TimeDelta::FromMicroseconds(
      duration_seconds * base::Time::kMicrosecondsPerSecond);

  // Try for the byterate too, but this is nonfatal if we can't get it.
  double byterate = 0;
  if (ExtractAMF0Number(script_buffer, "totaldatarate", &byterate)) {
    bits_per_second_ = (uint32)(byterate * 8.0);
  }

  return true;
}

// The SCRIPTDATA tag contains a list of ordered pairs of AMF0 strings followed
// by an arbitrary AMF0 object. Typically there's one object of interest with
// string name 'onMetaData' followed by an anonymous object. In any event we
// will scan the buffer looking only for the provided string, verify the next
// bytes in the stream describe an AMF0 number, extract and return it.
// TODO: Replace this (brittle) code with a proper AMF0 parser.
bool ShellFLVParser::ExtractAMF0Number(scoped_refptr<ShellScopedArray> amf0,
                                       const char* name,
                                       double* number_out) {
  DCHECK(number_out);
  // the string will be proceeded by a u16 big-endian string length
  uint16 name_length = strlen(name);
  // there's lots of nonprinting characters and zeros in amf0, so we'll need
  // to search for the string using our own method
  int match_offset = 0;
  int name_offset = 0;
  // the last index in the buffer we could extract a string followed by Number
  int search_length = amf0->Size() - (name_length + kAMF0NumberLength);
  uint8* search_buffer = amf0->Get();
  while (match_offset <= search_length && name_offset < name_length) {
    if (search_buffer[match_offset] == name[name_offset]) {
      name_offset++;  // advance our substring pointer in the event of a match
    } else {
      name_offset = 0;  // reset our substring pointer on a miss
    }
    match_offset++;  // always advance our larger string pointer
  }
  // If we got a match name_offset will be pointing past the end of the search
  // string and match_offset will be pointing to valid memory with room to
  // extract a Number
  if ((name_offset == name_length) &&
      (match_offset <= amf0->Size() - kAMF0NumberLength)) {
    // make sure the first byte matches the number type code
    if (search_buffer[match_offset] != kAMF0NumberType) {
      return false;
    }
    // advance pointer past the number type to the number itself
    match_offset++;
    // load big-endian double as uint, then cast to correct type
    uint64 num_as_uint =
        endian_util::load_uint64_big_endian(search_buffer + match_offset);
    *number_out = *((double*)(&num_as_uint));
    return true;
  }
  return false;
}

void ShellFLVParser::JumpParserTo(uint64 byte_offset) {
  next_video_aus_.clear();
  next_audio_aus_.clear();
  at_end_of_file_ = false;
  tag_offset_ = byte_offset;
}

}  // namespace media
