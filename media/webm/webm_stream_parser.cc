// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webm/webm_stream_parser.h"

#include "base/callback.h"
#include "base/logging.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/in_memory_url_protocol.h"
#include "media/webm/webm_cluster_parser.h"
#include "media/webm/webm_constants.h"
#include "media/webm/webm_info_parser.h"
#include "media/webm/webm_tracks_parser.h"

namespace media {

// Helper class that uses FFmpeg to create AudioDecoderConfig &
// VideoDecoderConfig objects.
//
// This dependency on FFmpeg can be removed once we update WebMTracksParser
// to parse the necessary data to construct AudioDecoderConfig &
// VideoDecoderConfig objects. http://crbug.com/108756
class FFmpegConfigHelper {
 public:
  FFmpegConfigHelper();
  ~FFmpegConfigHelper();

  bool Parse(const uint8* data, int size);

  const AudioDecoderConfig& audio_config() const;
  const VideoDecoderConfig& video_config() const;

 private:
  AVFormatContext* CreateFormatContext(const uint8* data, int size);
  bool SetupStreamConfigs();

  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;

  // Backing buffer for |url_protocol_|.
  scoped_array<uint8> url_protocol_buffer_;

  // Protocol used by |format_context_|. It must outlive the context object.
  scoped_ptr<FFmpegURLProtocol> url_protocol_;

  // FFmpeg format context for this demuxer. It is created by
  // av_open_input_file() during demuxer initialization and cleaned up with
  // DestroyAVFormatContext() in the destructor.
  AVFormatContext* format_context_;

  static const uint8 kWebMHeader[];
  static const int kSegmentSizeOffset;
  static const uint8 kEmptyCluster[];

  DISALLOW_COPY_AND_ASSIGN(FFmpegConfigHelper);
};

// WebM File Header. This is prepended to the INFO & TRACKS
// data passed to Init() before handing it to FFmpeg. Essentially
// we are making the INFO & TRACKS data look like a small WebM
// file so we can use FFmpeg to initialize the AVFormatContext.
const uint8 FFmpegConfigHelper::kWebMHeader[] = {
  0x1A, 0x45, 0xDF, 0xA3, 0x9F,  // EBML (size = 0x1f)
  0x42, 0x86, 0x81, 0x01,  // EBMLVersion = 1
  0x42, 0xF7, 0x81, 0x01,  // EBMLReadVersion = 1
  0x42, 0xF2, 0x81, 0x04,  // EBMLMaxIDLength = 4
  0x42, 0xF3, 0x81, 0x08,  // EBMLMaxSizeLength = 8
  0x42, 0x82, 0x84, 0x77, 0x65, 0x62, 0x6D,  // DocType = "webm"
  0x42, 0x87, 0x81, 0x02,  // DocTypeVersion = 2
  0x42, 0x85, 0x81, 0x02,  // DocTypeReadVersion = 2
  // EBML end
  0x18, 0x53, 0x80, 0x67,  // Segment
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // segment(size = 0)
  // INFO goes here.
};

// Offset of the segment size field in kWebMHeader. Used to update
// the segment size field before handing the buffer to FFmpeg.
const int FFmpegConfigHelper::kSegmentSizeOffset = sizeof(kWebMHeader) - 8;

const uint8 FFmpegConfigHelper::kEmptyCluster[] = {
  0x1F, 0x43, 0xB6, 0x75, 0x80  // CLUSTER (size = 0)
};

FFmpegConfigHelper::FFmpegConfigHelper() : format_context_(NULL) {}

FFmpegConfigHelper::~FFmpegConfigHelper() {
  if (!format_context_)
    return;

  DestroyAVFormatContext(format_context_);
  format_context_ = NULL;

  if (url_protocol_.get()) {
    FFmpegGlue::GetInstance()->RemoveProtocol(url_protocol_.get());
    url_protocol_.reset();
    url_protocol_buffer_.reset();
  }
}

bool FFmpegConfigHelper::Parse(const uint8* data, int size) {
  format_context_ = CreateFormatContext(data, size);
  return format_context_ && SetupStreamConfigs();
}

const AudioDecoderConfig& FFmpegConfigHelper::audio_config() const {
  return audio_config_;
}

const VideoDecoderConfig& FFmpegConfigHelper::video_config() const {
  return video_config_;
}

AVFormatContext* FFmpegConfigHelper::CreateFormatContext(const uint8* data,
                                                         int size) {
  DCHECK(!url_protocol_.get());
  DCHECK(!url_protocol_buffer_.get());

  int segment_size = size + sizeof(kEmptyCluster);
  int buf_size = sizeof(kWebMHeader) + segment_size;
  url_protocol_buffer_.reset(new uint8[buf_size]);
  uint8* buf = url_protocol_buffer_.get();
  memcpy(buf, kWebMHeader, sizeof(kWebMHeader));
  memcpy(buf + sizeof(kWebMHeader), data, size);
  memcpy(buf + sizeof(kWebMHeader) + size, kEmptyCluster,
         sizeof(kEmptyCluster));

  // Update the segment size in the buffer.
  int64 tmp = (segment_size & GG_LONGLONG(0x00FFFFFFFFFFFFFF)) |
      GG_LONGLONG(0x0100000000000000);
  for (int i = 0; i < 8; i++) {
    buf[kSegmentSizeOffset + i] = (tmp >> (8 * (7 - i))) & 0xff;
  }

  url_protocol_.reset(new InMemoryUrlProtocol(buf, buf_size, true));
  std::string key = FFmpegGlue::GetInstance()->AddProtocol(url_protocol_.get());

  // Open FFmpeg AVFormatContext.
  AVFormatContext* context = NULL;
  int result = av_open_input_file(&context, key.c_str(), NULL, 0, NULL);

  if (result < 0)
    return NULL;

  return context;
}

bool FFmpegConfigHelper::SetupStreamConfigs() {
  int result = av_find_stream_info(format_context_);

  if (result < 0)
    return false;

  bool no_supported_streams = true;
  for (size_t i = 0; i < format_context_->nb_streams; ++i) {
    AVStream* stream = format_context_->streams[i];
    AVCodecContext* codec_context = stream->codec;
    AVMediaType codec_type = codec_context->codec_type;

    if (codec_type == AVMEDIA_TYPE_AUDIO &&
        stream->codec->codec_id == CODEC_ID_VORBIS &&
        !audio_config_.IsValidConfig()) {
      AVCodecContextToAudioDecoderConfig(stream->codec, &audio_config_);
      no_supported_streams = false;
      continue;
    }

    if (codec_type == AVMEDIA_TYPE_VIDEO &&
        stream->codec->codec_id == CODEC_ID_VP8 &&
        !video_config_.IsValidConfig()) {
      AVStreamToVideoDecoderConfig(stream, &video_config_);
      no_supported_streams = false;
      continue;
    }
  }

  return !no_supported_streams;
}

WebMStreamParser::WebMStreamParser()
    : state_(WAITING_FOR_INIT),
      host_(NULL) {
}

WebMStreamParser::~WebMStreamParser() {}

void WebMStreamParser::Init(const InitCB& init_cb, StreamParserHost* host) {
  DCHECK_EQ(state_, WAITING_FOR_INIT);
  DCHECK(init_cb_.is_null());
  DCHECK(!host_);
  DCHECK(!init_cb.is_null());
  DCHECK(host);

  ChangeState(PARSING_HEADERS);
  init_cb_ = init_cb;
  host_ = host;
}

void WebMStreamParser::Flush() {
  DCHECK_NE(state_, WAITING_FOR_INIT);

  if (state_ != PARSING_CLUSTERS)
    return;

  cluster_parser_->Reset();
}

int WebMStreamParser::Parse(const uint8* buf, int size) {
  DCHECK_NE(state_, WAITING_FOR_INIT);

  if (state_ == PARSING_HEADERS)
    return ParseInfoAndTracks(buf, size);

  if (state_ == PARSING_CLUSTERS)
    return ParseCluster(buf, size);

  return -1;
}

void WebMStreamParser::ChangeState(State new_state) {
  state_ = new_state;
}

int WebMStreamParser::ParseInfoAndTracks(const uint8* data, int size) {
  DCHECK(data);
  DCHECK_GT(size, 0);

  const uint8* cur = data;
  int cur_size = size;
  int bytes_parsed = 0;

  int id;
  int64 element_size;
  int result = WebMParseElementHeader(cur, cur_size, &id, &element_size);

  if (result <= 0)
    return result;

  switch (id) {
    case kWebMIdEBMLHeader:
    case kWebMIdSeekHead:
    case kWebMIdVoid:
    case kWebMIdCRC32:
    case kWebMIdCues:
      if (cur_size < (result + element_size)) {
        // We don't have the whole element yet. Signal we need more data.
        return 0;
      }
      // Skip the element.
      return result + element_size;
      break;
    case kWebMIdSegment:
      // Just consume the segment header.
      return result;
      break;
    case kWebMIdInfo:
      // We've found the element we are looking for.
      break;
    default:
      DVLOG(1) << "Unexpected ID 0x" << std::hex << id;
      return -1;
  }

  WebMInfoParser info_parser;
  result = info_parser.Parse(cur, cur_size);

  if (result <= 0)
    return result;

  cur += result;
  cur_size -= result;
  bytes_parsed += result;

  WebMTracksParser tracks_parser(info_parser.timecode_scale());
  result = tracks_parser.Parse(cur, cur_size);

  if (result <= 0)
    return result;

  bytes_parsed += result;

  double mult = info_parser.timecode_scale() / 1000.0;
  base::TimeDelta duration =
      base::TimeDelta::FromMicroseconds(info_parser.duration() * mult);

  FFmpegConfigHelper config_helper;

  if (!config_helper.Parse(data, bytes_parsed))
    return -1;

  if (config_helper.audio_config().IsValidConfig())
    host_->OnNewAudioConfig(config_helper.audio_config());

  if (config_helper.video_config().IsValidConfig())
    host_->OnNewVideoConfig(config_helper.video_config());

  cluster_parser_.reset(new WebMClusterParser(
      info_parser.timecode_scale(),
      tracks_parser.audio_track_num(),
      tracks_parser.audio_default_duration(),
      tracks_parser.video_track_num(),
      tracks_parser.video_default_duration()));

  ChangeState(PARSING_CLUSTERS);
  init_cb_.Run(true, duration);

  return bytes_parsed;
}

int WebMStreamParser::ParseCluster(const uint8* data, int size) {
  if (!cluster_parser_.get())
    return -1;

  int id;
  int64 element_size;
  int result = WebMParseElementHeader(data, size, &id, &element_size);

  if (result <= 0)
    return result;

  if (id == kWebMIdCues) {
    if (size < (result + element_size)) {
      // We don't have the whole element yet. Signal we need more data.
      return 0;
    }
    // Skip the element.
    return result + element_size;
  }

  int bytes_parsed = cluster_parser_->Parse(data, size);

  if (bytes_parsed <= 0)
    return bytes_parsed;

  if (cluster_parser_->audio_buffers().empty() &&
      cluster_parser_->video_buffers().empty())
    return bytes_parsed;

  if (!host_->OnAudioBuffers(cluster_parser_->audio_buffers()))
    return -1;

  if (!host_->OnVideoBuffers(cluster_parser_->video_buffers()))
    return -1;

  return bytes_parsed;
}

}  // namespace media
