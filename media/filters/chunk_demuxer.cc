// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/chunk_demuxer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/data_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/chunk_demuxer_client.h"
#include "media/webm/webm_stream_parser.h"

namespace media {

// Create an "end of stream" buffer.
static Buffer* CreateEOSBuffer() {
  return new DataBuffer(0);
}

class ChunkDemuxerStream : public DemuxerStream {
 public:
  typedef std::deque<scoped_refptr<Buffer> > BufferQueue;
  typedef std::deque<ReadCallback> ReadCBQueue;

  explicit ChunkDemuxerStream(const AudioDecoderConfig& audio_config);
  explicit ChunkDemuxerStream(const VideoDecoderConfig& video_config);
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

ChunkDemuxerStream::ChunkDemuxerStream(const AudioDecoderConfig& audio_config)
    : type_(AUDIO),
      shutdown_called_(false),
      received_end_of_stream_(false),
      last_buffer_timestamp_(kNoTimestamp) {
  audio_config_.CopyFrom(audio_config);
}


ChunkDemuxerStream::ChunkDemuxerStream(const VideoDecoderConfig& video_config)
    : type_(VIDEO),
      shutdown_called_(false),
      received_end_of_stream_(false),
      last_buffer_timestamp_(kNoTimestamp) {
  video_config_.CopyFrom(video_config);
}

ChunkDemuxerStream::~ChunkDemuxerStream() {}

void ChunkDemuxerStream::Flush() {
  DVLOG(1) << "Flush()";
  base::AutoLock auto_lock(lock_);
  buffers_.clear();
  received_end_of_stream_ = false;
  last_buffer_timestamp_ = kNoTimestamp;
}

bool ChunkDemuxerStream::CanAddBuffers(const BufferQueue& buffers) const {
  base::AutoLock auto_lock(lock_);

  // If we haven't seen any buffers yet, then anything can be added.
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
      buffered_bytes_(0),
      seek_waits_for_data_(true) {
  DCHECK(client);
}

ChunkDemuxer::~ChunkDemuxer() {
  DCHECK_NE(state_, INITIALIZED);
}

void ChunkDemuxer::Init(const PipelineStatusCB& cb) {
  DVLOG(1) << "Init()";
  {
    base::AutoLock auto_lock(lock_);
    DCHECK_EQ(state_, WAITING_FOR_INIT);

    ChangeState_Locked(INITIALIZING);
    init_cb_ = cb;
    stream_parser_.reset(new WebMStreamParser());

    stream_parser_->Init(
        base::Bind(&ChunkDemuxer::OnStreamParserInitDone, this),
        this);
  }

  client_->DemuxerOpened(this);
}

void ChunkDemuxer::set_host(DemuxerHost* host) {
  DCHECK_EQ(state_, INITIALIZED);
  Demuxer::set_host(host);
  host->SetDuration(duration_);
  host->SetCurrentReadPosition(0);
}

void ChunkDemuxer::Stop(const base::Closure& callback) {
  DVLOG(1) << "Stop()";
  Shutdown();
  callback.Run();
}

void ChunkDemuxer::Seek(base::TimeDelta time, const PipelineStatusCB& cb) {
  DVLOG(1) << "Seek(" << time.InSecondsF() << ")";

  PipelineStatus status = PIPELINE_ERROR_INVALID_STATE;
  {
    base::AutoLock auto_lock(lock_);

    if (state_ == INITIALIZED || state_ == ENDED) {
      if (seek_waits_for_data_) {
        DVLOG(1) << "Seek() : waiting for more data to arrive.";
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
  DVLOG(1) << "GetStartTime()";
  // TODO(acolwell) : Fix this so it uses the time on the first packet.
  return base::TimeDelta();
}

void ChunkDemuxer::FlushData() {
  DVLOG(1) << "FlushData()";
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == INITIALIZED || state_ == ENDED || state_ == SHUTDOWN);

  if (state_ == SHUTDOWN)
    return;

  if (audio_.get())
    audio_->Flush();

  if (video_.get())
    video_->Flush();

  byte_queue_.Reset();
  stream_parser_->Flush();

  seek_waits_for_data_ = true;
  ChangeState_Locked(INITIALIZED);
}

bool ChunkDemuxer::AppendData(const uint8* data, size_t length) {
  DVLOG(1) << "AppendData(" << length << ")";

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

    // Capture |seek_waits_for_data_| state before we start parsing.
    // Its state can be changed by OnAudioBuffers() or OnVideoBuffers()
    // calls during the parse.
    bool old_seek_waits_for_data = seek_waits_for_data_;

    byte_queue_.Peek(&cur, &cur_size);

    do {
      switch(state_) {
        case INITIALIZING:
          result = stream_parser_->Parse(cur, cur_size);
          if (result < 0) {
            ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
            return true;
          }
          break;

        case INITIALIZED: {
          result = stream_parser_->Parse(cur, cur_size);
          if (result < 0) {
            ReportError_Locked(PIPELINE_ERROR_DECODE);
            return true;
          }
        } break;

        case WAITING_FOR_INIT:
        case ENDED:
        case PARSE_ERROR:
        case SHUTDOWN:
          DVLOG(1) << "AppendData(): called in unexpected state " << state_;
          return false;
      }

      if (result > 0) {
        cur += result;
        cur_size -= result;
        bytes_parsed += result;
      }
    } while (result > 0 && cur_size > 0);

    byte_queue_.Pop(bytes_parsed);

    // Check to see if parsing triggered seek_waits_for_data_ to go from true to
    // false. This indicates we have parsed enough data to complete the seek.
    if (old_seek_waits_for_data && !seek_waits_for_data_ &&
        !seek_cb_.is_null()) {
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
  DVLOG(1) << "EndOfStream(" << status << ")";
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
  DVLOG(1) << "Shutdown()";
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

    stream_parser_.reset();

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
      host()->OnDemuxerError(error);
      return;
    }
    cb.Run(error);
  }
}

void ChunkDemuxer::OnStreamParserInitDone(bool success,
                                          base::TimeDelta duration) {
  lock_.AssertAcquired();
  DCHECK_EQ(state_, INITIALIZING);
  if (!success || (!audio_.get() && !video_.get())) {
    ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  duration_ = duration;

  ChangeState_Locked(INITIALIZED);
  PipelineStatusCB cb;
  std::swap(cb, init_cb_);
  cb.Run(PIPELINE_OK);
}

bool ChunkDemuxer::OnNewAudioConfig(const AudioDecoderConfig& config) {
  lock_.AssertAcquired();
  // Only allow a single audio config for now.
  if (audio_.get())
    return false;

  audio_ = new ChunkDemuxerStream(config);
  return true;
}

bool ChunkDemuxer::OnNewVideoConfig(const VideoDecoderConfig& config) {
  lock_.AssertAcquired();
  // Only allow a single video config for now.
  if (video_.get())
    return false;

  video_ = new ChunkDemuxerStream(config);
  return true;
}


bool ChunkDemuxer::OnAudioBuffers(const BufferQueue& buffers) {
  if (!audio_.get())
    return false;

  if (!audio_->CanAddBuffers(buffers))
    return false;

  audio_->AddBuffers(buffers);
  seek_waits_for_data_ = false;

  return true;
}

bool ChunkDemuxer::OnVideoBuffers(const BufferQueue& buffers) {
  if (!video_.get())
    return false;

  if (!video_->CanAddBuffers(buffers))
    return false;

  video_->AddBuffers(buffers);
  seek_waits_for_data_ = false;

  return true;
}

}  // namespace media
