// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements a Demuxer that can switch among different data sources mid-stream.
// Uses FFmpegDemuxer under the covers, so see the caveats at the top of
// ffmpeg_demuxer.h.

#include "media/filters/chunk_demuxer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/filter_host.h"
#include "media/base/data_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/chunk_demuxer_client.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/in_memory_url_protocol.h"
#include "media/webm/webm_cluster_parser.h"
#include "media/webm/webm_constants.h"
#include "media/webm/webm_info_parser.h"
#include "media/webm/webm_tracks_parser.h"

namespace media {

// WebM File Header. This is prepended to the INFO & TRACKS
// data passed to Init() before handing it to FFmpeg. Essentially
// we are making the INFO & TRACKS data look like a small WebM
// file so we can use FFmpeg to initialize the AVFormatContext.
//
// TODO(acolwell): Remove this when we construct AudioDecoderConfig and
// VideoDecoderConfig without requiring an AVStream object.
static const uint8 kWebMHeader[] = {
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
static const int kSegmentSizeOffset = sizeof(kWebMHeader) - 8;

static const uint8 kEmptyCluster[] = {
  0x1F, 0x43, 0xB6, 0x75, 0x80  // CLUSTER (size = 0)
};

// Create an "end of stream" buffer.
static Buffer* CreateEOSBuffer() { return new DataBuffer(0, 0); }

class ChunkDemuxerStream : public DemuxerStream {
 public:
  typedef std::deque<scoped_refptr<Buffer> > BufferQueue;
  typedef std::deque<ReadCallback> ReadCBQueue;

  ChunkDemuxerStream(Type type, AVStream* stream);
  virtual ~ChunkDemuxerStream();

  void Flush();

  // Checks if it is ok to add the |buffers| to the stream.
  bool CanAddBuffers(const BufferQueue& buffers) const;

  void AddBuffers(const BufferQueue& buffers);
  void Shutdown();

  bool GetLastBufferTimestamp(base::TimeDelta* timestamp) const;

  // DemuxerStream methods.
  virtual void Read(const ReadCallback& read_callback);
  virtual Type type();
  virtual void EnableBitstreamConverter();
  virtual const AudioDecoderConfig& audio_decoder_config();
  virtual const VideoDecoderConfig& video_decoder_config();

 private:
  Type type_;
  AVStream* av_stream_;
  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;

  mutable base::Lock lock_;
  ReadCBQueue read_cbs_;
  BufferQueue buffers_;
  bool shutdown_called_;
  bool received_end_of_stream_;

  // Keeps track of the timestamp of the last buffer we have
  // added to |buffers_|. This is used to enforce buffers with strictly
  // monotonically increasing timestamps.
  base::TimeDelta last_buffer_timestamp_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChunkDemuxerStream);
};

ChunkDemuxerStream::ChunkDemuxerStream(Type type, AVStream* stream)
    : type_(type),
      av_stream_(stream),
      shutdown_called_(false),
      received_end_of_stream_(false),
      last_buffer_timestamp_(kNoTimestamp) {
  if (type_ == AUDIO) {
    AVCodecContextToAudioDecoderConfig(stream->codec, &audio_config_);
  } else if (type_ == VIDEO) {
    AVStreamToVideoDecoderConfig(stream, &video_config_);
  }
}

ChunkDemuxerStream::~ChunkDemuxerStream() {}

void ChunkDemuxerStream::Flush() {
  VLOG(1) << "Flush()";
  base::AutoLock auto_lock(lock_);
  buffers_.clear();
  received_end_of_stream_ = false;
  last_buffer_timestamp_ = kNoTimestamp;
}

bool ChunkDemuxerStream::CanAddBuffers(const BufferQueue& buffers) const {
  base::AutoLock auto_lock(lock_);

  // If we haven't seen any buffers yet than anything can be added.
  if (last_buffer_timestamp_ == kNoTimestamp)
    return true;

  if (buffers.empty())
    return true;

  return (buffers.front()->GetTimestamp() > last_buffer_timestamp_);
}

void ChunkDemuxerStream::AddBuffers(const BufferQueue& buffers) {
  if (buffers.empty())
    return;

  std::deque<base::Closure> callbacks;
  {
    base::AutoLock auto_lock(lock_);

    for (BufferQueue::const_iterator itr = buffers.begin();
         itr != buffers.end(); itr++) {
      // Make sure we aren't trying to add a buffer after we have received and
      // "end of stream" buffer.
      DCHECK(!received_end_of_stream_);

      if ((*itr)->IsEndOfStream()) {
        received_end_of_stream_ = true;

        // Push enough EOS buffers to satisfy outstanding Read() requests.
        if (read_cbs_.size() > buffers_.size()) {
          size_t pending_read_without_data = read_cbs_.size() - buffers_.size();
          for (size_t i = 0; i <= pending_read_without_data; ++i) {
            buffers_.push_back(*itr);
          }
        }
      } else {
        base::TimeDelta current_ts = (*itr)->GetTimestamp();
        if (last_buffer_timestamp_ != kNoTimestamp) {
          DCHECK_GT(current_ts.ToInternalValue(),
                    last_buffer_timestamp_.ToInternalValue());
        }

        last_buffer_timestamp_ = current_ts;
        buffers_.push_back(*itr);
      }
    }

    while (!buffers_.empty() && !read_cbs_.empty()) {
      callbacks.push_back(base::Bind(read_cbs_.front(), buffers_.front()));
      buffers_.pop_front();
      read_cbs_.pop_front();
    }
  }

  while (!callbacks.empty()) {
    callbacks.front().Run();
    callbacks.pop_front();
  }
}

void ChunkDemuxerStream::Shutdown() {
  std::deque<ReadCallback> callbacks;
  {
    base::AutoLock auto_lock(lock_);
    shutdown_called_ = true;

    // Collect all the pending Read() callbacks.
    while (!read_cbs_.empty()) {
      callbacks.push_back(read_cbs_.front());
      read_cbs_.pop_front();
    }
  }

  // Pass end of stream buffers to all callbacks to signal that no more data
  // will be sent.
  while (!callbacks.empty()) {
    callbacks.front().Run(CreateEOSBuffer());
    callbacks.pop_front();
  }
}

bool ChunkDemuxerStream::GetLastBufferTimestamp(
    base::TimeDelta* timestamp) const {
  base::AutoLock auto_lock(lock_);

  if (buffers_.empty())
    return false;

  *timestamp = buffers_.back()->GetTimestamp();
  return true;
}

// Helper function that makes sure |read_callback| runs on |message_loop|.
static void RunOnMessageLoop(const DemuxerStream::ReadCallback& read_callback,
                             MessageLoop* message_loop,
                             const scoped_refptr<Buffer>& buffer) {
  if (MessageLoop::current() != message_loop) {
    message_loop->PostTask(FROM_HERE, base::Bind(
        &RunOnMessageLoop, read_callback, message_loop, buffer));
    return;
  }

  read_callback.Run(buffer);
}

// DemuxerStream methods.
void ChunkDemuxerStream::Read(const ReadCallback& read_callback) {
  scoped_refptr<Buffer> buffer;

  {
    base::AutoLock auto_lock(lock_);

    if (shutdown_called_ || (received_end_of_stream_ && buffers_.empty())) {
      buffer = CreateEOSBuffer();
    } else {
      if (buffers_.empty()) {
        // Wrap & store |read_callback| so that it will
        // get called on the current MessageLoop.
        read_cbs_.push_back(base::Bind(&RunOnMessageLoop,
                                       read_callback,
                                       MessageLoop::current()));
        return;
      }

      if (!read_cbs_.empty()) {
        // Wrap & store |read_callback| so that it will
        // get called on the current MessageLoop.
        read_cbs_.push_back(base::Bind(&RunOnMessageLoop,
                                       read_callback,
                                       MessageLoop::current()));
        return;
      }

      buffer = buffers_.front();
      buffers_.pop_front();
    }
  }

  DCHECK(buffer.get());
  read_callback.Run(buffer);
}

DemuxerStream::Type ChunkDemuxerStream::type() { return type_; }

void ChunkDemuxerStream::EnableBitstreamConverter() {}

const AudioDecoderConfig& ChunkDemuxerStream::audio_decoder_config() {
  CHECK_EQ(type_, AUDIO);
  return audio_config_;
}

const VideoDecoderConfig& ChunkDemuxerStream::video_decoder_config() {
  CHECK_EQ(type_, VIDEO);
  return video_config_;
}

ChunkDemuxer::ChunkDemuxer(ChunkDemuxerClient* client)
    : state_(WAITING_FOR_INIT),
      client_(client),
      format_context_(NULL),
      buffered_bytes_(0),
      seek_waits_for_data_(true) {
  DCHECK(client);
}

ChunkDemuxer::~ChunkDemuxer() {
  DCHECK_NE(state_, INITIALIZED);

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

void ChunkDemuxer::Init(const PipelineStatusCB& cb) {
  VLOG(1) << "Init()";
  {
    base::AutoLock auto_lock(lock_);
    DCHECK_EQ(state_, WAITING_FOR_INIT);

    ChangeState_Locked(INITIALIZING);
    init_cb_ = cb;
  }

  client_->DemuxerOpened(this);
}

void ChunkDemuxer::set_host(FilterHost* filter_host) {
  DCHECK_EQ(state_, INITIALIZED);
  Demuxer::set_host(filter_host);
  filter_host->SetDuration(duration_);
  filter_host->SetCurrentReadPosition(0);
}

void ChunkDemuxer::Stop(const base::Closure& callback) {
  VLOG(1) << "Stop()";
  Shutdown();
  callback.Run();
}

void ChunkDemuxer::Seek(base::TimeDelta time, const PipelineStatusCB& cb) {
  VLOG(1) << "Seek(" << time.InSecondsF() << ")";

  PipelineStatus status = PIPELINE_ERROR_INVALID_STATE;
  {
    base::AutoLock auto_lock(lock_);

    if (state_ == INITIALIZED || state_ == ENDED) {
      if (seek_waits_for_data_) {
        VLOG(1) << "Seek() : waiting for more data to arrive.";
        seek_cb_ = cb;
        return;
      }

      status = PIPELINE_OK;
    }
  }

  cb.Run(status);
}

void ChunkDemuxer::OnAudioRendererDisabled() {
  base::AutoLock auto_lock(lock_);
  audio_ = NULL;
}

void ChunkDemuxer::SetPreload(Preload preload) {}

int ChunkDemuxer::GetBitrate() {
  // TODO(acolwell): Implement bitrate reporting.
  return 0;
}

bool ChunkDemuxer::IsLocalSource() {
  // TODO(acolwell): Report whether source is local or not.
  return false;
}

bool ChunkDemuxer::IsSeekable() {
  // TODO(acolwell): Report whether source is seekable or not.
  return true;
}

// Demuxer implementation.
scoped_refptr<DemuxerStream> ChunkDemuxer::GetStream(
    DemuxerStream::Type type) {
  if (type == DemuxerStream::VIDEO)
    return video_;

  if (type == DemuxerStream::AUDIO)
    return audio_;

  return NULL;
}

base::TimeDelta ChunkDemuxer::GetStartTime() const {
  VLOG(1) << "GetStartTime()";
  // TODO(acolwell) : Fix this so it uses the time on the first packet.
  return base::TimeDelta();
}

void ChunkDemuxer::FlushData() {
  VLOG(1) << "FlushData()";
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == INITIALIZED || state_ == ENDED || state_ == SHUTDOWN);

  if (state_ == SHUTDOWN)
    return;

  if (audio_.get())
    audio_->Flush();

  if (video_.get())
    video_->Flush();

  seek_waits_for_data_ = true;
  ChangeState_Locked(INITIALIZED);
}

bool ChunkDemuxer::AppendData(const uint8* data, size_t length) {
  VLOG(1) << "AppendData(" << length << ")";

  if (!data || length == 0u)
    return false;

  int64 buffered_bytes = 0;
  base::TimeDelta buffered_ts = base::TimeDelta::FromSeconds(-1);

  PipelineStatusCB cb;
  {
    base::AutoLock auto_lock(lock_);

    byte_queue_.Push(data, length);

    const uint8* cur = NULL;
    int cur_size = 0;
    int bytes_parsed = 0;
    int result = -1;
    bool parsed_a_cluster = false;

    byte_queue_.Peek(&cur, &cur_size);

    do {
      switch(state_) {
        case INITIALIZING:
          result = ParseInfoAndTracks_Locked(cur, cur_size);
          if (result < 0) {
            VLOG(1) << "AppendData(): parsing info & tracks failed";
            ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
            return true;
          }
          break;

        case INITIALIZED:
          result = ParseCluster_Locked(cur, cur_size);
          if (result < 0) {
            VLOG(1) << "AppendData(): parsing data failed";
            ReportError_Locked(PIPELINE_ERROR_DECODE);
            return true;
          }

          parsed_a_cluster = (result > 0);
          break;

        case WAITING_FOR_INIT:
        case ENDED:
        case PARSE_ERROR:
        case SHUTDOWN:
          VLOG(1) << "AppendData(): called in unexpected state " << state_;
          return false;
      }

      if (result > 0) {
        cur += result;
        cur_size -= result;
        bytes_parsed += result;
      }
    } while (result > 0 && cur_size > 0);

    byte_queue_.Pop(bytes_parsed);

    if (parsed_a_cluster && seek_waits_for_data_) {
      seek_waits_for_data_ = false;

      if (!seek_cb_.is_null())
        std::swap(cb, seek_cb_);
    }

    base::TimeDelta tmp;
    if (audio_.get() && audio_->GetLastBufferTimestamp(&tmp) &&
        tmp > buffered_ts) {
      buffered_ts = tmp;
    }

    if (video_.get() && video_->GetLastBufferTimestamp(&tmp) &&
        tmp > buffered_ts) {
      buffered_ts = tmp;
    }

    buffered_bytes = buffered_bytes_;
  }

  // Notify the host of 'network activity' because we got data.
  if (host()) {
    host()->SetBufferedBytes(buffered_bytes);

    if (buffered_ts.InSeconds() >= 0) {
      host()->SetBufferedTime(buffered_ts);
    }

    host()->SetNetworkActivity(true);
  }

  if (!cb.is_null())
    cb.Run(PIPELINE_OK);

  return true;
}

void ChunkDemuxer::EndOfStream(PipelineStatus status) {
  VLOG(1) << "EndOfStream(" << status << ")";
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, WAITING_FOR_INIT);
  DCHECK_NE(state_, ENDED);

  if (state_ == SHUTDOWN || state_ == PARSE_ERROR)
    return;

  if (state_ == INITIALIZING) {
    ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  ChangeState_Locked(ENDED);

  if (status != PIPELINE_OK) {
    ReportError_Locked(status);
    return;
  }

  // Create an end of stream buffer.
  ChunkDemuxerStream::BufferQueue buffers;
  buffers.push_back(CreateEOSBuffer());

  if (audio_.get())
    audio_->AddBuffers(buffers);

  if (video_.get())
    video_->AddBuffers(buffers);
}

bool ChunkDemuxer::HasEnded() {
  base::AutoLock auto_lock(lock_);
  return (state_ == ENDED);
}

void ChunkDemuxer::Shutdown() {
  VLOG(1) << "Shutdown()";
  PipelineStatusCB cb;
  {
    base::AutoLock auto_lock(lock_);

    if (state_ == SHUTDOWN)
      return;

    std::swap(cb, seek_cb_);

    if (audio_.get())
      audio_->Shutdown();

    if (video_.get())
      video_->Shutdown();

    ChangeState_Locked(SHUTDOWN);
  }

  if (!cb.is_null())
    cb.Run(PIPELINE_ERROR_ABORT);

  client_->DemuxerClosed();
}

void ChunkDemuxer::ChangeState_Locked(State new_state) {
  lock_.AssertAcquired();
  state_ = new_state;
}

int ChunkDemuxer::ParseInfoAndTracks_Locked(const uint8* data, int size) {
  lock_.AssertAcquired();
  DCHECK(data);
  DCHECK_GT(size, 0);

  DCHECK_EQ(state_, INITIALIZING);

  const uint8* cur = data;
  int cur_size = size;
  int bytes_parsed = 0;

  int id;
  int64 element_size;
  int result = WebMParseElementHeader(cur, cur_size, &id, &element_size);

  if (result <= 0)
    return result;

  switch (id) {
    case kWebMIdEBML :
    case kWebMIdSeekHead :
    case kWebMIdVoid :
    case kWebMIdCRC32 :
    case kWebMIdCues :
      if (cur_size < (result + element_size)) {
        // We don't have the whole element yet. Signal we need more data.
        return 0;
      }
      // Skip the element.
      return result + element_size;
      break;
    case kWebMIdSegment :
      // Just consume the segment header.
      return result;
      break;
    case kWebMIdInfo :
      // We've found the element we are looking for.
      break;
    default:
      VLOG(1) << "Unexpected ID 0x" << std::hex << id;
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
  duration_ = base::TimeDelta::FromMicroseconds(info_parser.duration() * mult);

  cluster_parser_.reset(new WebMClusterParser(
      info_parser.timecode_scale(),
      tracks_parser.audio_track_num(),
      tracks_parser.audio_default_duration(),
      tracks_parser.video_track_num(),
      tracks_parser.video_default_duration()));

  format_context_ = CreateFormatContext(data, bytes_parsed);
  if (!format_context_ || !SetupStreams())
    return -1;

  ChangeState_Locked(INITIALIZED);
  init_cb_.Run(PIPELINE_OK);
  init_cb_.Reset();
  return bytes_parsed;
}

AVFormatContext* ChunkDemuxer::CreateFormatContext(const uint8* data,
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

bool ChunkDemuxer::SetupStreams() {
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
        !audio_.get()) {
      audio_ = new ChunkDemuxerStream(DemuxerStream::AUDIO, stream);
      no_supported_streams = false;
      continue;
    }

    if (codec_type == AVMEDIA_TYPE_VIDEO &&
        stream->codec->codec_id == CODEC_ID_VP8 &&
        !video_.get()) {
      video_ = new ChunkDemuxerStream(DemuxerStream::VIDEO, stream);
      no_supported_streams = false;
      continue;
    }
  }

  return !no_supported_streams;
}

int ChunkDemuxer::ParseCluster_Locked(const uint8* data, int size) {
  lock_.AssertAcquired();
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
  } else if (id != kWebMIdCluster) {
    VLOG(1) << "Unexpected ID 0x" << std::hex << id;
    return -1;
  }

  int bytes_parsed = cluster_parser_->Parse(data, size);

  if (bytes_parsed <= 0)
    return bytes_parsed;

  // Make sure we can add the buffers to both streams before we actutally
  // add them. This allows us to accept all of the data or none of it.
  if ((audio_.get() &&
       !audio_->CanAddBuffers(cluster_parser_->audio_buffers())) ||
      (video_.get() &&
       !video_->CanAddBuffers(cluster_parser_->video_buffers()))) {
    return -1;
  }

  if (audio_.get())
    audio_->AddBuffers(cluster_parser_->audio_buffers());

  if (video_.get())
    video_->AddBuffers(cluster_parser_->video_buffers());

  // TODO(acolwell) : make this more representative of what is actually
  // buffered.
  buffered_bytes_ += bytes_parsed;

  return bytes_parsed;
}

void ChunkDemuxer::ReportError_Locked(PipelineStatus error) {
  lock_.AssertAcquired();
  DCHECK_NE(error, PIPELINE_OK);

  ChangeState_Locked(PARSE_ERROR);

  PipelineStatusCB cb;

  if (!init_cb_.is_null()) {
    std::swap(cb, init_cb_);
  } else {
    if (!seek_cb_.is_null())
      std::swap(cb, seek_cb_);

    if (audio_.get())
      audio_->Shutdown();

    if (video_.get())
      video_->Shutdown();
  }

  {
    base::AutoUnlock auto_unlock(lock_);
    if (cb.is_null()) {
      host()->SetError(error);
      return;
    }
    cb.Run(error);
  }
}

}  // namespace media
