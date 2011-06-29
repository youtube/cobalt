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
#include "media/base/filter_host.h"
#include "media/base/data_buffer.h"
#include "media/ffmpeg/ffmpeg_common.h"
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
// TODO(acolwell): Remove this once GetAVStream() has been removed from
// the DemuxerStream interface.
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

static Buffer* CreateBuffer(const uint8* data, size_t size) {
  scoped_array<uint8> buf(new uint8[size]);
  memcpy(buf.get(), data, size);
  return new DataBuffer(buf.release(), size);
}

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
  virtual const MediaFormat& media_format();
  virtual void EnableBitstreamConverter();
  virtual AVStream* GetAVStream();

 private:
  static void RunCallback(ReadCallback cb, scoped_refptr<Buffer> buffer);

  Type type_;
  MediaFormat media_format_;
  AVStream* av_stream_;

  mutable base::Lock lock_;
  ReadCBQueue read_cbs_;
  BufferQueue buffers_;
  bool shutdown_called_;

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
      last_buffer_timestamp_(kNoTimestamp) {
}

ChunkDemuxerStream::~ChunkDemuxerStream() {}

void ChunkDemuxerStream::Flush() {
  VLOG(1) << "Flush()";
  base::AutoLock auto_lock(lock_);
  buffers_.clear();
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

      base::TimeDelta current_ts = (*itr)->GetTimestamp();
      if (last_buffer_timestamp_ != kNoTimestamp) {
        DCHECK_GT(current_ts.ToInternalValue(),
                  last_buffer_timestamp_.ToInternalValue());
      }

      last_buffer_timestamp_ = current_ts;

      buffers_.push_back(*itr);
    }

    while (!buffers_.empty() && !read_cbs_.empty()) {
      callbacks.push_back(base::Bind(&ChunkDemuxerStream::RunCallback,
                                     read_cbs_.front(),
                                     buffers_.front()));
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

  // Pass NULL to all callbacks to signify read failure.
  while (!callbacks.empty()) {
    callbacks.front().Run(NULL);
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

// Helper function used to make Closures for ReadCallbacks.
//static
void ChunkDemuxerStream::RunCallback(ReadCallback cb,
                                     scoped_refptr<Buffer> buffer) {
  cb.Run(buffer);
}

// Helper function that makes sure |read_callback| runs on |message_loop|.
static void RunOnMessageLoop(const DemuxerStream::ReadCallback& read_callback,
                             MessageLoop* message_loop,
                             Buffer* buffer) {
  if (MessageLoop::current() != message_loop) {
    message_loop->PostTask(FROM_HERE,
                           NewRunnableFunction(&RunOnMessageLoop,
                                               read_callback,
                                               message_loop,
                                               scoped_refptr<Buffer>(buffer)));
    return;
  }

  read_callback.Run(buffer);
}

// DemuxerStream methods.
void ChunkDemuxerStream::Read(const ReadCallback& read_callback) {
  scoped_refptr<Buffer> buffer;

  {
    base::AutoLock auto_lock(lock_);

    if (!shutdown_called_) {
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

  read_callback.Run(buffer);
}

DemuxerStream::Type ChunkDemuxerStream::type() { return type_; }

const MediaFormat& ChunkDemuxerStream::media_format() { return media_format_; }

void ChunkDemuxerStream::EnableBitstreamConverter() {}

AVStream* ChunkDemuxerStream::GetAVStream() { return av_stream_; }

ChunkDemuxer::ChunkDemuxer()
    : state_(WAITING_FOR_INIT),
      format_context_(NULL),
      buffered_bytes_(0),
      seek_waits_for_data_(true) {
}

ChunkDemuxer::~ChunkDemuxer() {
  DCHECK_NE(state_, INITIALIZED);

  if (!format_context_)
    return;

  DestroyAVFormatContext(format_context_);
  format_context_ = NULL;
}

bool ChunkDemuxer::Init(const uint8* data, int size) {
  DCHECK(data);
  DCHECK_GT(size, 0);

  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, WAITING_FOR_INIT);

  const uint8* cur = data;
  int cur_size = size;
  WebMInfoParser info_parser;
  int res = info_parser.Parse(cur, cur_size);

  if (res <= 0) {
    ChangeState(INIT_ERROR);
    return false;
  }

  cur += res;
  cur_size -= res;

  WebMTracksParser tracks_parser(info_parser.timecode_scale());
  res = tracks_parser.Parse(cur, cur_size);

  if (res <= 0) {
    ChangeState(INIT_ERROR);
    return false;
  }

  double mult = info_parser.timecode_scale() / 1000.0;
  duration_ = base::TimeDelta::FromMicroseconds(info_parser.duration() * mult);

  cluster_parser_.reset(new WebMClusterParser(
      info_parser.timecode_scale(),
      tracks_parser.audio_track_num(),
      tracks_parser.audio_default_duration(),
      tracks_parser.video_track_num(),
      tracks_parser.video_default_duration()));

  format_context_ = CreateFormatContext(data, size);

  if (!format_context_ || !SetupStreams() || !ParsePendingBuffers()) {
    ChangeState(INIT_ERROR);
    return false;
  }

  ChangeState(INITIALIZED);
  return true;
}

// Filter implementation.
void ChunkDemuxer::set_host(FilterHost* filter_host) {
  Demuxer::set_host(filter_host);
  filter_host->SetDuration(duration_);
  filter_host->SetCurrentReadPosition(0);
}

void ChunkDemuxer::Stop(FilterCallback* callback) {
  VLOG(1) << "Stop()";

  callback->Run();
  delete callback;
}

void ChunkDemuxer::Seek(base::TimeDelta time, const FilterStatusCB&  cb) {
  VLOG(1) << "Seek(" << time.InSecondsF() << ")";

  {
    base::AutoLock auto_lock(lock_);

    if (seek_waits_for_data_) {
      seek_cb_ = cb;
      return;
    }
  }

  cb.Run(PIPELINE_OK);
}

void ChunkDemuxer::OnAudioRendererDisabled() {
  base::AutoLock auto_lock(lock_);
  audio_ = NULL;
}

void ChunkDemuxer::SetPreload(Preload preload) {}

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
  base::AutoLock auto_lock(lock_);
  if (audio_.get())
    audio_->Flush();

  if (video_.get())
    video_->Flush();

  pending_buffers_.clear();
  seek_waits_for_data_ = true;
}

bool ChunkDemuxer::AddData(const uint8* data, unsigned length) {
  VLOG(1) << "AddData(" << length << ")";
  DCHECK(data);
  DCHECK_GT(length, 0u);

  int64 buffered_bytes = 0;
  base::TimeDelta buffered_ts = base::TimeDelta::FromSeconds(-1);

  FilterStatusCB cb;
  {
    base::AutoLock auto_lock(lock_);

    switch(state_) {
      case WAITING_FOR_INIT:
        pending_buffers_.push_back(CreateBuffer(data, length));
        return true;
        break;

      case INITIALIZED:
        if (!ParseAndAddData_Locked(data, length)) {
          VLOG(1) << "AddData(): parsing data failed";
          return false;
        }
        break;

      case INIT_ERROR:
      case SHUTDOWN:
        VLOG(1) << "AddData(): called in unexpected state " << state_;
        return false;
        break;
    }

    seek_waits_for_data_ = false;

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

    if (!seek_cb_.is_null())
      std::swap(cb, seek_cb_);
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

void ChunkDemuxer::Shutdown() {
  FilterStatusCB cb;
  {
    base::AutoLock auto_lock(lock_);

    std::swap(cb, seek_cb_);

    if (audio_.get())
      audio_->Shutdown();

    if (video_.get())
      video_->Shutdown();

    ChangeState(SHUTDOWN);
  }

  if (!cb.is_null())
    cb.Run(PIPELINE_ERROR_ABORT);
}

void ChunkDemuxer::ChangeState(State new_state) {
  lock_.AssertAcquired();
  state_ = new_state;
}

AVFormatContext* ChunkDemuxer::CreateFormatContext(const uint8* data,
                                                   int size) const {
  int segment_size = size + sizeof(kEmptyCluster);
  int buf_size = sizeof(kWebMHeader) + segment_size;
  scoped_array<uint8> buf(new uint8[buf_size]);
  memcpy(buf.get(), kWebMHeader, sizeof(kWebMHeader));
  memcpy(buf.get() + sizeof(kWebMHeader), data, size);
  memcpy(buf.get() + sizeof(kWebMHeader) + size, kEmptyCluster,
         sizeof(kEmptyCluster));

  // Update the segment size in the buffer.
  int64 tmp = (segment_size & GG_LONGLONG(0x00FFFFFFFFFFFFFF)) |
      GG_LONGLONG(0x0100000000000000);
  for (int i = 0; i < 8; i++) {
    buf[kSegmentSizeOffset + i] = (tmp >> (8 * (7 - i))) & 0xff;
  }

  InMemoryUrlProtocol imup(buf.get(), buf_size, true);
  std::string key = FFmpegGlue::GetInstance()->AddProtocol(&imup);

  // Open FFmpeg AVFormatContext.
  AVFormatContext* context = NULL;
  int result = av_open_input_file(&context, key.c_str(), NULL, 0, NULL);

  // Remove ourself from protocol list.
  FFmpegGlue::GetInstance()->RemoveProtocol(&imup);

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
    CodecType codec_type = codec_context->codec_type;

    if (codec_type == CODEC_TYPE_AUDIO &&
        stream->codec->codec_id == CODEC_ID_VORBIS &&
        !audio_.get()) {
      audio_ = new ChunkDemuxerStream(DemuxerStream::AUDIO, stream);
      no_supported_streams = false;
      continue;
    }

    if (codec_type == CODEC_TYPE_VIDEO &&
        stream->codec->codec_id == CODEC_ID_VP8 &&
        !video_.get()) {
      video_ = new ChunkDemuxerStream(DemuxerStream::VIDEO, stream);
      no_supported_streams = false;
      continue;
    }
  }

  return !no_supported_streams;
}

bool ChunkDemuxer::ParsePendingBuffers() {
  bool had_pending_buffers = !pending_buffers_.empty();
  // Handle any buffers that came in between the time the pipeline was
  // started and Init() was called.
  while(!pending_buffers_.empty()) {
    scoped_refptr<media::Buffer> buf = pending_buffers_.front();
    pending_buffers_.pop_front();

    if (!ParseAndAddData_Locked(buf->GetData(), buf->GetDataSize())) {
      pending_buffers_.clear();
      ChangeState(INIT_ERROR);
      return false;
    }
  }

  seek_waits_for_data_ = !had_pending_buffers;
  return true;
}

bool ChunkDemuxer::ParseAndAddData_Locked(const uint8* data, int length) {
  if (!cluster_parser_.get())
    return false;

  const uint8* cur = data;
  int cur_size = length;

  while (cur_size > 0) {
    int res = cluster_parser_->Parse(cur, cur_size);

    if (res <= 0) {
      VLOG(1) << "ParseAndAddData_Locked() : cluster parsing failed.";
      return false;
    }

    // Make sure we can add the buffers to both streams before we acutally
    // add them. This allows us to accept all of the data or none of it.
    if ((audio_.get() &&
         !audio_->CanAddBuffers(cluster_parser_->audio_buffers())) ||
        (video_.get() &&
         !video_->CanAddBuffers(cluster_parser_->video_buffers()))) {
      return false;
    }

    if (audio_.get())
      audio_->AddBuffers(cluster_parser_->audio_buffers());

    if (video_.get())
      video_->AddBuffers(cluster_parser_->video_buffers());

    cur += res;
    cur_size -= res;
  }

  // TODO(acolwell) : make this more representative of what is actually
  // buffered.
  buffered_bytes_ += length;

  return true;
}

}  // namespace media
