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

#include "cobalt/media/progressive/mp4_parser.h"

#include <inttypes.h>
#include <limits>
#include <vector>

#include "base/strings/stringprintf.h"
#include "cobalt/media/base/endian_util.h"
#include "cobalt/media/formats/mp4/es_descriptor.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

// how many bytes to skip within an avc1 before the config atoms start?
static const int kSkipBytes_avc1 = 78;

// what's the smallest meaningful mp4 atom size?
static const int kAtomMinSize = 8;

// Full Box has a one byte version and three bytes flags after the header
static const int kFullBoxHeaderAndFlagSize = 4;

// how much to download of an hdlr to get the trak type?
static const int kDesiredBytes_hdlr = 12;
static const uint32 kAudioSubtype_hdlr_soun = 0x736f756e;
static const uint32 kVideoSubtype_hdlr_vide = 0x76696465;

// how much to download of an mp4a to determine version number?
static const int kDesiredBytes_mp4a = 2;
// how big is the mp4a atom before optional extension atoms?
static const int kTotalSize_mp4a_v0 = 28;
static const int kTotalSize_mp4a_v1 = 44;
static const int kTotalSize_mp4a_v2 = 64;

// how many bytes should we download from an mdhd?
static const int kDesiredBytes_mdhd = 20;

// how many bytes should we download from an mvhd?
static const int kDesiredBytes_mvhd = 20;

// how many bytes to skip within an stsd before the config atoms start?
static const int kSkipBytes_stsd = 8;

// use average values of mp4 metadata tables plus 2 standard deviations to
// hold most metadata atoms entirely in memory.
static const int kMapTableAtomCacheEntries_stsz = 260591 / kEntrySize_stsz;
static const int kMapTableAtomCacheEntries_stco = 22859 / kEntrySize_stco;
static const int kMapTableAtomCacheEntries_stss = 3786 / kEntrySize_stss;
static const int kMapTableAtomCacheEntries_stts = 164915 / kEntrySize_stts;
static const int kMapTableAtomCacheEntries_stsc = 32199 / kEntrySize_stsc;
static const int kMapTableAtomCacheEntries_co64 = 740212 / kEntrySize_co64;
static const int kMapTableAtomCacheEntries_ctts = 51543 / kEntrySize_ctts;

// static
PipelineStatus MP4Parser::Construct(scoped_refptr<DataSourceReader> reader,
                                    const uint8* construction_header,
                                    scoped_refptr<ProgressiveParser>* parser,
                                    const scoped_refptr<MediaLog>& media_log) {
  DCHECK(parser);
  DCHECK(media_log);
  *parser = NULL;

  // detect mp4 stream by looking for ftyp atom at top of file
  uint32 ftyp = endian_util::load_uint32_big_endian(construction_header + 4);
  if (ftyp != kAtomType_ftyp) {
    // not an mp4
    return DEMUXER_ERROR_COULD_NOT_PARSE;
  }

  // first 4 bytes will be the size of the ftyp atom
  uint32 ftyp_atom_size =
      endian_util::load_uint32_big_endian(construction_header);
  if (ftyp_atom_size < kAtomMinSize) {
    return DEMUXER_ERROR_COULD_NOT_PARSE;
  }

  // construct new mp4 parser
  *parser = new MP4Parser(reader, ftyp_atom_size, media_log);
  return PIPELINE_OK;
}

MP4Parser::MP4Parser(scoped_refptr<DataSourceReader> reader,
                     uint32 ftyp_atom_size,
                     const scoped_refptr<MediaLog>& media_log)
    : AVCParser(reader, media_log),
      atom_offset_(ftyp_atom_size),  // start at next atom, skipping over ftyp
      current_trak_is_video_(false),
      current_trak_is_audio_(false),
      current_trak_time_scale_(0),
      video_time_scale_hz_(0),
      audio_time_scale_hz_(0),
      audio_map_(new MP4Map(reader)),
      video_map_(new MP4Map(reader)),
      audio_sample_(0),
      video_sample_(0),
      first_audio_hole_ticks_(0),
      first_audio_hole_(base::TimeDelta::FromSeconds(0)) {}

MP4Parser::~MP4Parser() {}

// For MP4 we traverse the file's atom structure attempting to find the audio
// and video configuration information and the locations in the file of the
// various stbl subatoms which detail the position of the audio and video
// NALUs in the file. As some of the stbl subatoms can be quite large we cache
// a fixed maximum quantity of each stbl subatom and update the cache only on
// miss.
bool MP4Parser::ParseConfig() {
  while (!IsConfigComplete() || !audio_map_->IsComplete() ||
         !video_map_->IsComplete()) {
    if (!ParseNextAtom()) {
      return false;
    }
  }
  return true;
}

scoped_refptr<AvcAccessUnit> MP4Parser::GetNextAU(DemuxerStream::Type type) {
  uint32 size = 0;
  uint32 duration_ticks = 0;
  uint64 timestamp_ticks = 0;
  uint64 offset = 0;
  bool is_keyframe = false;
  base::TimeDelta timestamp;
  base::TimeDelta duration;
  if (type == DemuxerStream::AUDIO) {
    if (audio_time_scale_hz_ == 0) {
      DLOG(ERROR) << "|audio_time_scale_hz_| cannot be 0.";
      return NULL;
    }
    if (!audio_map_->GetSize(audio_sample_, &size) ||
        !audio_map_->GetOffset(audio_sample_, &offset) ||
        !audio_map_->GetDuration(audio_sample_, &duration_ticks) ||
        !audio_map_->GetTimestamp(audio_sample_, &timestamp_ticks)) {
      // determine if EOS or error
      if (audio_map_->IsEOS(audio_sample_)) {
        return AvcAccessUnit::CreateEndOfStreamAU(DemuxerStream::AUDIO,
                                                  audio_track_duration_);
      } else {
        DLOG(ERROR) << "parsed bad audio AU";
        return NULL;
      }
    }
    // all aac frames are random-access, so all are keyframes
    is_keyframe = true;
    audio_sample_++;
    timestamp = TicksToTime(timestamp_ticks, audio_time_scale_hz_);
    duration = TicksToTime(duration_ticks, audio_time_scale_hz_);

    // It would be very unusual to encounter non-contiguous audio
    // in an mp4, but you never know. Make sure this timestamp is
    // contiguous in ticks from the last one
    if (first_audio_hole_ticks_ == timestamp_ticks) {
      // Much of the audio stack assumes that audio timestamps are
      // contiguous. While the timestamps coming out of the map are
      // normally continuous, they are on a different time scale. Due
      // to roundoff error in conversion the timestamps produced may
      // be discontinuous. To correct this we correct the timestamp
      // to the one the system is expecting for continuity, then modify
      // the duration by the negative of that same (small) value,
      // so as to not accumulate roundoff error over time.
      base::TimeDelta time_difference = timestamp - first_audio_hole_;
      timestamp = first_audio_hole_;
      duration += time_difference;
      first_audio_hole_ = timestamp + duration;
      first_audio_hole_ticks_ += duration_ticks;
    } else {
      DLOG(WARNING) << "parsed non-contiguous mp4 audio timestamp";
      // reset hole tracking past gap
      first_audio_hole_ticks_ = timestamp_ticks + duration_ticks;
      first_audio_hole_ = timestamp + duration;
    }
  } else if (type == DemuxerStream::VIDEO) {
    if (video_time_scale_hz_ == 0) {
      DLOG(ERROR) << "|video_time_scale_hz_| cannot be 0.";
      return NULL;
    }
    if (!video_map_->GetSize(video_sample_, &size) ||
        !video_map_->GetOffset(video_sample_, &offset) ||
        !video_map_->GetDuration(video_sample_, &duration_ticks) ||
        !video_map_->GetTimestamp(video_sample_, &timestamp_ticks) ||
        !video_map_->GetIsKeyframe(video_sample_, &is_keyframe)) {
      if (video_map_->IsEOS(video_sample_)) {
        return AvcAccessUnit::CreateEndOfStreamAU(DemuxerStream::VIDEO,
                                                  video_track_duration_);
      } else {
        DLOG(ERROR) << "parsed bad video AU";
        return NULL;
      }
    }
    video_sample_++;
    timestamp = TicksToTime(timestamp_ticks, video_time_scale_hz_);
    duration = TicksToTime(duration_ticks, video_time_scale_hz_);
    // due to b-frames it's much more likely we'll encounter discontinuous
    // video buffers. As a result we add a small duration to each video
    // buffer, equal in value to one full tick at the video timescale. The
    // showing of video frames is actually keyed on the audio clock, so this
    // shouldn't create too much jitter in the output.
    duration += one_video_tick_;
  } else {
    NOTREACHED() << "unsupported stream type";
    return NULL;
  }

  size_t prepend_size = CalculatePrependSize(type, is_keyframe);

  if (type == DemuxerStream::AUDIO)
    return AvcAccessUnit::CreateAudioAU(offset, size, prepend_size, is_keyframe,
                                        timestamp, duration, this);
  return AvcAccessUnit::CreateVideoAU(offset, size, prepend_size,
                                      nal_header_size_, is_keyframe, timestamp,
                                      duration, this);
}

bool MP4Parser::SeekTo(base::TimeDelta timestamp) {
  if (audio_time_scale_hz_ == 0 || video_time_scale_hz_ == 0) {
    DLOG_IF(ERROR, audio_time_scale_hz_ == 0)
        << "|audio_time_scale_hz_| cannot be 0.";
    DLOG_IF(ERROR, video_time_scale_hz_ == 0)
        << "|video_time_scale_hz_| cannot be 0.";
    return false;
  }

  // get video timestamp in video time units
  uint64 video_ticks = TimeToTicks(timestamp, video_time_scale_hz_);
  // find nearest keyframe from map, make it our next video sample
  if (!video_map_->GetKeyframe(video_ticks, &video_sample_)) {
    return false;
  }
  // get the timestamp for this video keyframe
  uint64 video_keyframe_time_ticks = 0;
  if (!video_map_->GetTimestamp(video_sample_, &video_keyframe_time_ticks)) {
    return false;
  }
  base::TimeDelta video_keyframe_time =
      TicksToTime(video_keyframe_time_ticks, video_time_scale_hz_);
  // find the closest audio frame that bounds that timestamp
  uint64 audio_ticks = TimeToTicks(video_keyframe_time, audio_time_scale_hz_);
  if (!audio_map_->GetKeyframe(audio_ticks, &audio_sample_)) {
    return false;
  }
  DLOG(INFO) << base::StringPrintf(
      "seeking to timestamp: %" PRId64 ", video sample: %d, audio sample: %d",
      timestamp.InMilliseconds(), video_sample_, audio_sample_);
  // cheat our buffer continuity system
  if (!audio_map_->GetTimestamp(audio_sample_, &first_audio_hole_ticks_)) {
    return false;
  }
  first_audio_hole_ =
      TicksToTime(first_audio_hole_ticks_, audio_time_scale_hz_);
  return true;
}

// parse the atom starting at atom_offset_, update appropriate internal state,
// return false on fatal error. General structure of an MP4 atom is:
// field             | type   | comment
// ------------------+--------+---------
// atom size         | uint32 | if 0 means "rest of file", if 1 means extended
// fourCC code       | ASCII  | four-byte ASCII code we treat as uint32
// extended size     | uint64 | optional size field, only here if atom size is 1
// <--- rest of atom body starts here
bool MP4Parser::ParseNextAtom() {
  uint8 atom[kAtomDownload];
  int bytes_read = reader_->BlockingRead(atom_offset_, kAtomDownload, atom);
  if (bytes_read < kAtomDownload) {
    return false;
  }
  // first 4 bytes are size of atom uint32
  uint64 atom_size =
      static_cast<uint64>(endian_util::load_uint32_big_endian(atom));
  // normally atom body starts just past fourCC code
  uint32 atom_body = kAtomMinSize;
  // if 1 we need to load the extended size which will be appended just past
  // the fourCC code
  if (atom_size == 1) {
    atom_size = endian_util::load_uint64_big_endian(atom + 8);
    // advance atom_body past the 8 bytes of size we just parsed
    atom_body += 8;
  } else if (atom_size == 0) {
    // calculate size of this atom from remainder of file
    DCHECK_LE(atom_offset_,
              static_cast<uint64>(std::numeric_limits<int64>::max()));
    if (reader_->FileSize() > static_cast<int64>(atom_offset_)) {
      atom_size = reader_->FileSize() - atom_offset_;
    }
  }
  // atom sizes also include the size of the start of the atom, so sanity-check
  // the size we just parsed against the number of bytes we needed to parse it
  if (atom_size < atom_body) {
    DLOG(WARNING) << base::StringPrintf("atom size: %" PRId64
                                        " less than min body size %d",
                                        atom_size, atom_body);
    return false;
  }

  // extract fourCC code as big-endian uint32
  uint32 four_cc = endian_util::load_uint32_big_endian(atom + 4);
  DLOG(INFO) << base::StringPrintf("four_cc: %c%c%c%c", atom[4], atom[5],
                                   atom[6], atom[7]);

  // advance read pointer to atom body
  atom_offset_ += atom_body;
  // adjust size of body of atom from size of header
  uint64 atom_data_size = atom_size - atom_body;

  bool atom_parse_success = true;

  // We use 95% certainty intervals for video metadata atom sizes. The map
  // is written to handle larger atom sizes but having to recache metadata
  // increases latencies on things like seeks.
  int map_table_atom_cache_entries = 0;

  // now take appropriate action based on atom type
  switch (four_cc) {
    // avc1 atoms are contained within stsd atoms and carry their own
    // configuration baggage load, which we skip over and parse the atoms
    // within, normally an avcC atom.
    case kAtomType_avc1:
      atom_offset_ += kSkipBytes_avc1;
      break;

    // avcC atoms contain the AVCConfigRecord, our video configuration info
    case kAtomType_avcC:
      atom_parse_success =
          DownloadAndParseAVCConfigRecord(atom_offset_, atom_data_size);
      if (atom_parse_success) atom_offset_ += atom_data_size;
      break;

    // esds atoms contain actually usable audio configuration info for AAC.
    case kAtomType_esds:
      return ParseMP4_esds(atom_data_size);

    // can tell us if mdia and mdhd atoms relate to audio or video metadata
    case kAtomType_hdlr:
      return ParseMP4_hdlr(atom_data_size, atom + atom_body);

    // provides a duration and a timescale unique to a given track
    case kAtomType_mdhd:
      return ParseMP4_mdhd(atom_data_size, atom + atom_body);

    // mp4a atoms contain audio configuration info, but we only want to know
    // which version it is so we can skip to the esds, which we must be present
    // when using AAC
    case kAtomType_mp4a:
      return ParseMP4_mp4a(atom_data_size, atom + atom_body);

    // movie header atom contains track duration and time unit scale, we trust
    // these data as the authoritative duration data for the mp4
    case kAtomType_mvhd:
      return ParseMP4_mvhd(atom_data_size, atom + atom_body);

    // stsd atoms may contain avc1 atoms, which themselves may contain avcC
    // atoms, which contain actually usable configuration information. skip to
    // subatom.
    case kAtomType_stsd:
      atom_offset_ += kSkipBytes_stsd;
      break;

    // We're very much interested in the contents of the trak container atom,
    // blow away state that we may have been keeping about any prior trak
    // atoms we've parsed.
    case kAtomType_trak:
      current_trak_is_video_ = false;
      current_trak_is_audio_ = false;
      break;

    // if one of the stbl subatoms add it to the appropriate audio or video map
    // and then advance past it.
    case kAtomType_co64:
      map_table_atom_cache_entries = kMapTableAtomCacheEntries_co64;
      break;

    case kAtomType_ctts:
      map_table_atom_cache_entries = kMapTableAtomCacheEntries_ctts;
      break;

    case kAtomType_stco:
      map_table_atom_cache_entries = kMapTableAtomCacheEntries_stco;
      break;

    case kAtomType_stts:
      map_table_atom_cache_entries = kMapTableAtomCacheEntries_stts;
      break;

    case kAtomType_stsc:
      map_table_atom_cache_entries = kMapTableAtomCacheEntries_stsc;
      break;

    case kAtomType_stss:
      map_table_atom_cache_entries = kMapTableAtomCacheEntries_stss;
      break;

    case kAtomType_stsz:
      map_table_atom_cache_entries = kMapTableAtomCacheEntries_stsz;
      break;

    // these are container atoms, so we dont want to advance past the header
    // as we are interested in their contents. Parsing them is trivial
    // as all they are is a size header and a fourCC type tag, which we've
    // already parsed and advanced past.
    case kAtomType_mdia:
    case kAtomType_minf:
    case kAtomType_moov:
    case kAtomType_stbl:
      // no-op
      break;

    // known atom types that we wish to just skip past the body without warning
    case kAtomType_dinf:
    case kAtomType_dref:
    case kAtomType_smhd:
    case kAtomType_tkhd:
    case kAtomType_vmhd:
      atom_offset_ += atom_data_size;
      break;

    // parse functions are assumed to advance read_position_ themselves,
    // as we are flattening a tree of atoms so that the atom_size we parsed
    // this time, if it's a container, may not have been entirely consumed
    // in this single call. However for unsupported atoms we just skip them
    // entirely, meaning we will skip their contents too.
    default:
      atom_offset_ += atom_data_size;
      DLOG(INFO) << base::StringPrintf(
          "skipping unsupported MP4 atom: %c%c%c%c", atom[4], atom[5], atom[6],
          atom[7]);
      break;
  }

  if (map_table_atom_cache_entries > 0) {
    if (current_trak_is_video_) {
      atom_parse_success =
          video_map_->SetAtom(four_cc, atom_offset_, atom_data_size,
                              map_table_atom_cache_entries, atom + atom_body);
    } else if (current_trak_is_audio_) {
      atom_parse_success =
          audio_map_->SetAtom(four_cc, atom_offset_, atom_data_size,
                              map_table_atom_cache_entries, atom + atom_body);
    }
    atom_offset_ += atom_data_size;
  }

  if (!atom_parse_success) {
    DLOG(ERROR) << base::StringPrintf("Unable to parse MP4 atom: %c%c%c%c",
                                      atom[4], atom[5], atom[6], atom[7]);
  }

  return atom_parse_success;
}

bool MP4Parser::ParseMP4_esds(uint64 atom_data_size) {
  if (atom_data_size < kFullBoxHeaderAndFlagSize) {
    DLOG(WARNING) << base::StringPrintf(
        "esds box should at least be %d bytes but now it is %" PRId64 " bytes",
        kFullBoxHeaderAndFlagSize, atom_data_size);
    return false;
  }

  uint64 esds_offset = atom_offset_ + kFullBoxHeaderAndFlagSize;
  uint64 esds_size = atom_data_size - kFullBoxHeaderAndFlagSize;

  if (esds_size == 0) {
    return false;
  }
  // we'll need to download entire esds, allocate buffer for it
  std::vector<uint8> esds_storage(esds_size);
  uint8* esds = &esds_storage[0];
  // download esds
  int bytes_read = reader_->BlockingRead(esds_offset, esds_size, esds);
  if (bytes_read < esds_size) {
    DLOG(WARNING) << "failed to download esds";
    return false;
  }
  mp4::ESDescriptor es_descriptor;
  std::vector<uint8> data(esds, esds + esds_size);
  if (es_descriptor.Parse(data)) {
    const std::vector<uint8>& dsi = es_descriptor.decoder_specific_info();
    if (dsi.size() >= 2) {
      ParseAudioSpecificConfig(dsi[0], dsi[1]);
      atom_offset_ += atom_data_size;
      return true;
    }
    DLOG(WARNING) << "esds audio specific config shorter than 2 bytes";
  } else {
    DLOG(WARNING) << "error in parse esds box";
  }

  return false;
}

bool MP4Parser::ParseMP4_hdlr(uint64 atom_data_size, uint8* hdlr) {
  // ensure we're downloading enough of the hdlr to parse
  DCHECK_LE(kDesiredBytes_hdlr + 16, kAtomDownload);
  // sanity-check for minimum size
  if (atom_data_size < kDesiredBytes_hdlr) {
    DLOG(WARNING) << base::StringPrintf("bad size %" PRId64 " on hdlr",
                                        atom_data_size);
    return false;
  }
  // last 4 bytes of the 12 we need are an ascii code for the trak type, we
  // want 'vide' for video or 'soun' for audio. ignore the rest.
  uint32 hdlr_subtype = endian_util::load_uint32_big_endian(hdlr + 8);
  // update state flags
  current_trak_is_video_ = (hdlr_subtype == kVideoSubtype_hdlr_vide);
  current_trak_is_audio_ = (hdlr_subtype == kAudioSubtype_hdlr_soun);
  // save a time scale if pending
  if (current_trak_time_scale_ > 0 && current_trak_is_video_) {
    video_time_scale_hz_ = current_trak_time_scale_;
    current_trak_time_scale_ = 0;
    video_track_duration_ = current_trak_duration_;
    one_video_tick_ =
        base::TimeDelta::FromMicroseconds(1000000 / video_time_scale_hz_);
  }
  if (current_trak_time_scale_ > 0 && current_trak_is_audio_) {
    audio_time_scale_hz_ = current_trak_time_scale_;
    current_trak_time_scale_ = 0;
    audio_track_duration_ = current_trak_duration_;
  }
  // skip rest of atom
  atom_offset_ += atom_data_size;
  return true;
}

bool MP4Parser::ParseMP4_mdhd(uint64 atom_data_size, uint8* mdhd) {
  DCHECK_LE(kDesiredBytes_mdhd + 16, kAtomDownload);
  if (atom_data_size < kDesiredBytes_mdhd) {
    DLOG(WARNING) << base::StringPrintf("bad size %" PRId64 " on mdhd",
                                        atom_data_size);
    return false;
  }
  uint32 time_scale = endian_util::load_uint32_big_endian(mdhd + 12);
  if (time_scale == 0) {
    DLOG(WARNING) << "got 0 time scale for mvhd";
    return false;
  }
  // double-check track duration, it may be different from the movie duration
  uint32 track_duration_ticks = endian_util::load_uint32_big_endian(mdhd + 16);
  base::TimeDelta track_duration =
      TicksToTime(track_duration_ticks, time_scale);
  if (track_duration > duration_) {
    DLOG(WARNING) << base::StringPrintf("mdhd has longer duration: %" PRId64
                                        " ms than old value: %" PRId64 " ms.",
                                        track_duration.InMicroseconds(),
                                        duration_.InMicroseconds());
    duration_ = track_duration;
  }
  if (current_trak_is_video_) {
    video_time_scale_hz_ = time_scale;
    current_trak_time_scale_ = 0;
    video_track_duration_ = track_duration;
    one_video_tick_ =
        base::TimeDelta::FromMicroseconds(1000000 / video_time_scale_hz_);
  } else if (current_trak_is_audio_) {
    audio_time_scale_hz_ = time_scale;
    current_trak_time_scale_ = 0;
    audio_track_duration_ = track_duration;
  } else {
    // it's possible we will encounter the mdhd before we encounter the hdlr,
    // in that event we save the time scale value until we know.
    current_trak_time_scale_ = time_scale;
    current_trak_duration_ = track_duration;
  }
  atom_offset_ += atom_data_size;
  return true;
}

bool MP4Parser::ParseMP4_mp4a(uint64 atom_data_size, uint8* mp4a) {
  DCHECK_LE(kDesiredBytes_mp4a + 16, kAtomDownload);
  // we only need the first two bytes of the header, which details the version
  // number of this atom, which tells us the size of the rest of the header,
  // telling us how much we should skip to get to the extension contents.
  if (atom_data_size < kDesiredBytes_mp4a) {
    DLOG(WARNING) << base::StringPrintf("bad size %" PRId64 " on mp4a",
                                        atom_data_size);
    return false;
  }
  uint16 mp4a_version = endian_util::load_uint16_big_endian(mp4a);
  switch (mp4a_version) {
    case 0:
      atom_offset_ += kTotalSize_mp4a_v0;
      return true;

    case 1:
      atom_offset_ += kTotalSize_mp4a_v1;
      return true;

    case 2:
      atom_offset_ += kTotalSize_mp4a_v2;
      return true;

    default:
      // unknown mp4a atom version, parse failure
      DLOG(ERROR) << base::StringPrintf("parsed bad mp4a version %d",
                                        mp4a_version);
      return false;
  }
}

// partial layout of mvhd header is:
// offset | name              | size in bytes
// -------+-------------------+---------------
// 0      | version           | 1 (ignored)
// 1      | flags             | 3 (ignored)
// 4      | creation time     | 4 (ignored)
// 8      | modification time | 4 (ignored)
// 12     | time scale        | 4
// 16     | duration:         | 4
//
bool MP4Parser::ParseMP4_mvhd(uint64 atom_data_size, uint8* mvhd) {
  DCHECK_LE(kDesiredBytes_mvhd + 16, kAtomDownload);
  // it should be at least long enough for us to extract the parts we want
  if (atom_data_size < kDesiredBytes_mvhd) {
    DLOG(WARNING) << base::StringPrintf("bad size %" PRId64 " on mvhd",
                                        atom_data_size);
    return false;
  }
  uint32 time_scale_hz = endian_util::load_uint32_big_endian(mvhd + 12);
  if (time_scale_hz == 0) {
    DLOG(WARNING) << "got 0 time scale for mvhd";
    return false;
  }
  // duration is in units of the time scale we just extracted
  uint64 duration_ticks = endian_util::load_uint32_big_endian(mvhd + 16);
  // calculate actual duration from that and the time scale
  duration_ = TicksToTime(duration_ticks, time_scale_hz);
  // advance read position
  atom_offset_ += atom_data_size;
  return true;
}

base::TimeDelta MP4Parser::TicksToTime(uint64 ticks, uint32 time_scale_hz) {
  DCHECK_NE(time_scale_hz, 0);

  if (time_scale_hz == 0) {
    return base::TimeDelta::FromSeconds(0);
  }
  return base::TimeDelta::FromMicroseconds((ticks * 1000000ULL) /
                                           time_scale_hz);
}

uint64 MP4Parser::TimeToTicks(base::TimeDelta time, uint32 time_scale_hz) {
  DCHECK_NE(time_scale_hz, 0);

  if (time_scale_hz == 0) {
    return 0;
  }
  return (time.InMicroseconds() * time_scale_hz) / 1000000ULL;
}

}  // namespace media
}  // namespace cobalt
