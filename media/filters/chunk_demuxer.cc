// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/chunk_demuxer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/chunk_demuxer_client.h"
#include "media/webm/webm_stream_parser.h"

namespace media {

struct SupportedTypeInfo {
  const char* type;
  const char** codecs;
};

static const char* kVideoWebMCodecs[] = { "vp8", "vorbis", NULL };
static const char* kAudioWebMCodecs[] = { "vorbis", NULL };

static const SupportedTypeInfo kSupportedTypeInfo[] = {
  { "video/webm", kVideoWebMCodecs },
  { "audio/webm", kAudioWebMCodecs },
};

// Checks to see if the specified |type| and |codecs| list are supported.
// Returns true if |type| and all codecs listed in |codecs| are supported.
// Returns false otherwise.
static bool IsSupported(const std::string& type,
                        std::vector<std::string>& codecs) {
  // Search for the SupportedTypeInfo for |type|
  for (size_t i = 0; i < arraysize(kSupportedTypeInfo); ++i) {
    const SupportedTypeInfo& type_info = kSupportedTypeInfo[i];
    if (type == type_info.type) {
      // Make sure all the codecs specified in |codecs| are
      // in the supported type info.
      for (size_t j = 0; j < codecs.size(); ++j) {
        // Search the type info for a match.
        bool found_codec = false;
        for (int k = 0; type_info.codecs[k] && !found_codec; ++k)
          found_codec = (codecs[j] == type_info.codecs[k]);

        if (!found_codec)
          return false;
      }

      // All codecs were supported by this |type|.
      return true;
    }
  }

  // |type| didn't match any of the supported types.
  return false;
}

class ChunkDemuxerStream : public DemuxerStream {
 public:
  typedef std::deque<scoped_refptr<StreamParserBuffer> > BufferQueue;
  typedef std::deque<ReadCB> ReadCBQueue;
  typedef std::deque<base::Closure> ClosureQueue;

  explicit ChunkDemuxerStream(const AudioDecoderConfig& audio_config);
  explicit ChunkDemuxerStream(const VideoDecoderConfig& video_config);
  virtual ~ChunkDemuxerStream();

  void Flush();
  void Seek(base::TimeDelta time);

  // Checks if it is ok to add the |buffers| to the stream.
  bool CanAddBuffers(const BufferQueue& buffers) const;

  void AddBuffers(const BufferQueue& buffers);
  void Shutdown();

  // Gets the time range buffered by this object.
  // Returns true if there is buffered data. |start_out| & |end_out| are set to
  //    the start and end time of the buffered data respectively.
  // Returns false if no data is buffered.
  bool GetBufferedRange(base::TimeDelta* start_out,
                        base::TimeDelta* end_out) const;

  bool GetLastBufferTimestamp(base::TimeDelta* timestamp) const;

  // DemuxerStream methods.
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual Type type() OVERRIDE;
  virtual void EnableBitstreamConverter() OVERRIDE;
  virtual const AudioDecoderConfig& audio_decoder_config() OVERRIDE;
  virtual const VideoDecoderConfig& video_decoder_config() OVERRIDE;

 private:
  enum State {
    RETURNING_DATA_FOR_READS,
    WAITING_FOR_SEEK,
    RECEIVED_EOS_WHILE_WAITING_FOR_SEEK, // EOS = End of stream.
    RECEIVED_EOS,
    RETURNING_EOS_FOR_READS,
    SHUTDOWN,
  };

  // Assigns |state_| to |state|
  void ChangeState_Locked(State state);

  // Adds the callback to |read_cbs_| so it can be called later when we
  // have data.
  void DeferRead_Locked(const ReadCB& read_cb);

  // Creates closures that bind ReadCBs in |read_cbs_| to data in
  // |buffers_| and pops the callbacks & buffers from the respecive queues.
  void CreateReadDoneClosures_Locked(ClosureQueue* closures);

  Type type_;
  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;

  mutable base::Lock lock_;
  State state_;
  ReadCBQueue read_cbs_;
  BufferQueue buffers_;

  // Keeps track of the timestamp of the last buffer we have
  // added to |buffers_|. This is used to enforce buffers with strictly
  // monotonically increasing timestamps.
  base::TimeDelta last_buffer_timestamp_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChunkDemuxerStream);
};

ChunkDemuxerStream::ChunkDemuxerStream(const AudioDecoderConfig& audio_config)
    : type_(AUDIO),
      state_(RETURNING_DATA_FOR_READS),
      last_buffer_timestamp_(kNoTimestamp()) {
  audio_config_.CopyFrom(audio_config);
}


ChunkDemuxerStream::ChunkDemuxerStream(const VideoDecoderConfig& video_config)
    : type_(VIDEO),
      state_(RETURNING_DATA_FOR_READS),
      last_buffer_timestamp_(kNoTimestamp()) {
  video_config_.CopyFrom(video_config);
}

ChunkDemuxerStream::~ChunkDemuxerStream() {}

void ChunkDemuxerStream::Flush() {
  DVLOG(1) << "Flush()";
  ReadCBQueue read_cbs;
  {
    base::AutoLock auto_lock(lock_);
    buffers_.clear();
    ChangeState_Locked(WAITING_FOR_SEEK);
    last_buffer_timestamp_ = kNoTimestamp();

    std::swap(read_cbs_, read_cbs);
  }

  for (ReadCBQueue::iterator it = read_cbs.begin(); it != read_cbs.end(); ++it)
    it->Run(scoped_refptr<Buffer>());
}

void ChunkDemuxerStream::Seek(base::TimeDelta time) {
  base::AutoLock auto_lock(lock_);

  DCHECK(read_cbs_.empty());

  if (state_ == WAITING_FOR_SEEK) {
    ChangeState_Locked(RETURNING_DATA_FOR_READS);
    return;
  }

  if (state_ == RECEIVED_EOS_WHILE_WAITING_FOR_SEEK) {
    ChangeState_Locked(RECEIVED_EOS);
    return;
  }
}

bool ChunkDemuxerStream::CanAddBuffers(const BufferQueue& buffers) const {
  base::AutoLock auto_lock(lock_);

  // If we haven't seen any buffers yet, then anything can be added.
  if (last_buffer_timestamp_ == kNoTimestamp())
    return true;

  if (buffers.empty())
    return true;

  return (buffers.front()->GetTimestamp() > last_buffer_timestamp_);
}

void ChunkDemuxerStream::AddBuffers(const BufferQueue& buffers) {
  if (buffers.empty())
    return;

  ClosureQueue closures;
  {
    base::AutoLock auto_lock(lock_);

    for (BufferQueue::const_iterator itr = buffers.begin();
         itr != buffers.end(); itr++) {
      // Make sure we aren't trying to add a buffer after we have received and
      // "end of stream" buffer.
      DCHECK_NE(state_, RECEIVED_EOS_WHILE_WAITING_FOR_SEEK);
      DCHECK_NE(state_, RECEIVED_EOS);
      DCHECK_NE(state_, RETURNING_EOS_FOR_READS);

      if ((*itr)->IsEndOfStream()) {
        if (state_ == WAITING_FOR_SEEK)  {
          ChangeState_Locked(RECEIVED_EOS_WHILE_WAITING_FOR_SEEK);
        } else {
          ChangeState_Locked(RECEIVED_EOS);
        }
      } else {
        base::TimeDelta current_ts = (*itr)->GetTimestamp();
        if (last_buffer_timestamp_ != kNoTimestamp()) {
          DCHECK_GT(current_ts.ToInternalValue(),
                    last_buffer_timestamp_.ToInternalValue());
        }

        last_buffer_timestamp_ = current_ts;
        buffers_.push_back(*itr);
      }
    }

    CreateReadDoneClosures_Locked(&closures);
  }

  for (ClosureQueue::iterator it = closures.begin(); it != closures.end(); ++it)
    it->Run();
}

void ChunkDemuxerStream::Shutdown() {
  ReadCBQueue read_cbs;
  {
    base::AutoLock auto_lock(lock_);
    ChangeState_Locked(SHUTDOWN);

    std::swap(read_cbs_, read_cbs);
    buffers_.clear();
  }

  // Pass end of stream buffers to all callbacks to signal that no more data
  // will be sent.
  for (ReadCBQueue::iterator it = read_cbs.begin(); it != read_cbs.end(); ++it)
    it->Run(StreamParserBuffer::CreateEOSBuffer());
}

bool ChunkDemuxerStream::GetBufferedRange(
    base::TimeDelta* start_out, base::TimeDelta* end_out) const {
  base::AutoLock auto_lock(lock_);

  if (buffers_.empty())
    return false;

  *start_out = buffers_.front()->GetTimestamp();
  *end_out = buffers_.back()->GetTimestamp();

  base::TimeDelta end_duration = buffers_.back()->GetDuration();
  if (end_duration != kNoTimestamp())
    *end_out += end_duration;

  return true;
}

bool ChunkDemuxerStream::GetLastBufferTimestamp(
    base::TimeDelta* timestamp) const {
  base::AutoLock auto_lock(lock_);

  if (buffers_.empty())
    return false;

  *timestamp = buffers_.back()->GetTimestamp();
  return true;
}

// Helper function that makes sure |read_cb| runs on |message_loop|.
static void RunOnMessageLoop(const DemuxerStream::ReadCB& read_cb,
                             MessageLoop* message_loop,
                             const scoped_refptr<Buffer>& buffer) {
  if (MessageLoop::current() != message_loop) {
    message_loop->PostTask(FROM_HERE, base::Bind(
        &RunOnMessageLoop, read_cb, message_loop, buffer));
    return;
  }

  read_cb.Run(buffer);
}

// DemuxerStream methods.
void ChunkDemuxerStream::Read(const ReadCB& read_cb) {
  scoped_refptr<Buffer> buffer;

  {
    base::AutoLock auto_lock(lock_);

    switch (state_) {
      case RETURNING_DATA_FOR_READS:
        // If we don't have any buffers ready or already have
        // pending reads, then defer this read.
        if (buffers_.empty() || !read_cbs_.empty()) {
          DeferRead_Locked(read_cb);
          return;
        }

        buffer = buffers_.front();
        buffers_.pop_front();
        break;

      case WAITING_FOR_SEEK:
      case RECEIVED_EOS_WHILE_WAITING_FOR_SEEK:
        // Null buffers should be returned in this state since we are waiting
        // for a seek. Any buffers in |buffers_| should NOT be returned because
        // they are associated with the seek.
        DCHECK(read_cbs_.empty());
        break;
      case RECEIVED_EOS:
        DCHECK(read_cbs_.empty());

        if (buffers_.empty()) {
          ChangeState_Locked(RETURNING_EOS_FOR_READS);
          buffer = StreamParserBuffer::CreateEOSBuffer();
        } else {
          buffer = buffers_.front();
          buffers_.pop_front();
        }
        break;

      case RETURNING_EOS_FOR_READS:
      case SHUTDOWN:
        DCHECK(buffers_.empty());
        DCHECK(read_cbs_.empty());
        buffer = StreamParserBuffer::CreateEOSBuffer();
    }
  }

  read_cb.Run(buffer);
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

void ChunkDemuxerStream::ChangeState_Locked(State state) {
  lock_.AssertAcquired();
  state_ = state;
}

void ChunkDemuxerStream::DeferRead_Locked(const ReadCB& read_cb) {
  lock_.AssertAcquired();
  // Wrap & store |read_cb| so that it will
  // get called on the current MessageLoop.
  read_cbs_.push_back(base::Bind(&RunOnMessageLoop, read_cb,
                                 MessageLoop::current()));
}

void ChunkDemuxerStream::CreateReadDoneClosures_Locked(ClosureQueue* closures) {
  lock_.AssertAcquired();

  if (state_ != RETURNING_DATA_FOR_READS && state_ != RECEIVED_EOS)
    return;

  while (!buffers_.empty() && !read_cbs_.empty()) {
    closures->push_back(base::Bind(read_cbs_.front(), buffers_.front()));
    buffers_.pop_front();
    read_cbs_.pop_front();
  }

  if (state_ != RECEIVED_EOS || !buffers_.empty() || read_cbs_.empty())
    return;

  // Push enough EOS buffers to satisfy outstanding Read() requests.
  scoped_refptr<Buffer> end_of_stream_buffer =
      StreamParserBuffer::CreateEOSBuffer();
  while (!read_cbs_.empty()) {
    closures->push_back(base::Bind(read_cbs_.front(), end_of_stream_buffer));
    read_cbs_.pop_front();
  }

  ChangeState_Locked(RETURNING_EOS_FOR_READS);
}

ChunkDemuxer::ChunkDemuxer(ChunkDemuxerClient* client)
    : state_(WAITING_FOR_INIT),
      host_(NULL),
      client_(client),
      buffered_bytes_(0),
      seek_waits_for_data_(true) {
  DCHECK(client);
}

ChunkDemuxer::~ChunkDemuxer() {
  DCHECK_NE(state_, INITIALIZED);
}

void ChunkDemuxer::Initialize(DemuxerHost* host,
                              const PipelineStatusCB& cb) {
  DVLOG(1) << "Init()";
  {
    base::AutoLock auto_lock(lock_);
    DCHECK_EQ(state_, WAITING_FOR_INIT);
    host_ = host;

    ChangeState_Locked(INITIALIZING);
    init_cb_ = cb;

    source_buffer_.reset(new SourceBuffer());

    source_buffer_->Init(
        base::Bind(&ChunkDemuxer::OnSourceBufferInitDone, this),
        base::Bind(&ChunkDemuxer::OnNewConfigs, base::Unretained(this)),
        base::Bind(&ChunkDemuxer::OnAudioBuffers, base::Unretained(this)),
        base::Bind(&ChunkDemuxer::OnVideoBuffers, base::Unretained(this)),
        base::Bind(&ChunkDemuxer::OnKeyNeeded, base::Unretained(this)));
  }

  client_->DemuxerOpened(this);
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
      if (audio_)
        audio_->Seek(time);

      if (video_)
        video_->Seek(time);

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

int ChunkDemuxer::GetBitrate() {
  // TODO(acolwell): Implement bitrate reporting.
  return 0;
}

bool ChunkDemuxer::IsLocalSource() {
  // TODO(acolwell): Report whether source is local or not.
  return false;
}

bool ChunkDemuxer::IsSeekable() {
  return duration_ != kInfiniteDuration();
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

  source_buffer_->Flush();

  seek_waits_for_data_ = true;
  ChangeState_Locked(INITIALIZED);
}

ChunkDemuxer::Status ChunkDemuxer::AddId(const std::string& id,
                                         const std::string& type,
                                         std::vector<std::string>& codecs) {
  DCHECK_GT(codecs.size(), 0u);

  if (!IsSupported(type, codecs))
    return kNotSupported;

  // TODO(acolwell): Support for more than one ID
  // will be added as part of http://crbug.com/122909
  if (!source_id_.empty())
    return kReachedIdLimit;

  source_id_ = id;
  return kOk;
}

void ChunkDemuxer::RemoveId(const std::string& id) {
  CHECK(!source_id_.empty());
  CHECK_EQ(source_id_, id);
  source_id_ = "";
}

bool ChunkDemuxer::GetBufferedRanges(const std::string& id,
                                     Ranges* ranges_out) const {
  DCHECK(!id.empty());
  DCHECK_EQ(source_id_, id);
  DCHECK(ranges_out);

  base::AutoLock auto_lock(lock_);
  base::TimeDelta start = kNoTimestamp();
  base::TimeDelta end;
  base::TimeDelta tmp_start;
  base::TimeDelta tmp_end;

  if (audio_ && audio_->GetBufferedRange(&tmp_start, &tmp_end)) {
    start = tmp_start;
    end = tmp_end;
  }

  if (video_ && video_->GetBufferedRange(&tmp_start, &tmp_end)) {
    if (start == kNoTimestamp()) {
      start = tmp_start;
      end = tmp_end;
    } else {
      start = std::min(start, tmp_start);
      end = std::max(end, tmp_end);
    }
  }

  if (start == kNoTimestamp())
    return false;

  ranges_out->resize(1);
  (*ranges_out)[0].first = start;
  (*ranges_out)[0].second = end;
  return true;
}

bool ChunkDemuxer::AppendData(const std::string& id,
                              const uint8* data,
                              size_t length) {
  DVLOG(1) << "AppendData(" << id << ", " << length << ")";

  // TODO(acolwell): Remove when http://webk.it/83788 fix lands.
  if (source_id_.empty()) {
    std::vector<std::string> codecs(2);
    codecs[0] = "vp8";
    codecs[1] = "vorbis";
    AddId(id, "video/webm", codecs);
  }

  DCHECK(!source_id_.empty());
  DCHECK_EQ(source_id_, id);
  DCHECK(!id.empty());
  DCHECK(data);
  DCHECK_GT(length, 0u);

  int64 buffered_bytes = 0;
  base::TimeDelta buffered_ts = base::TimeDelta::FromSeconds(-1);

  PipelineStatusCB cb;
  {
    base::AutoLock auto_lock(lock_);

    // Capture |seek_waits_for_data_| state before we start parsing.
    // Its state can be changed by OnAudioBuffers() or OnVideoBuffers()
    // calls during the parse.
    bool old_seek_waits_for_data = seek_waits_for_data_;

    switch (state_) {
      case INITIALIZING:
        if (!source_buffer_->AppendData(data, length)) {
          DCHECK_EQ(state_, INITIALIZING);
          ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
          return true;
        }
        break;

      case INITIALIZED: {
        if (!source_buffer_->AppendData(data, length)) {
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
  host_->SetBufferedBytes(buffered_bytes);

  host_->SetNetworkActivity(true);

  if (!cb.is_null())
    cb.Run(PIPELINE_OK);

  return true;
}

void ChunkDemuxer::Abort(const std::string& id) {
  DCHECK(!id.empty());
  DCHECK_EQ(source_id_, id);

  source_buffer_->Flush();
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
  buffers.push_back(StreamParserBuffer::CreateEOSBuffer());

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

    source_buffer_.reset();

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

  if (!cb.is_null()) {
    base::AutoUnlock auto_unlock(lock_);
    cb.Run(error);
    return;
  }

  base::AutoUnlock auto_unlock(lock_);
  host_->OnDemuxerError(error);
}

void ChunkDemuxer::OnSourceBufferInitDone(bool success,
                                          base::TimeDelta duration) {
  lock_.AssertAcquired();
  DCHECK_EQ(state_, INITIALIZING);
  if (!success || (!audio_.get() && !video_.get())) {
    ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  duration_ = duration;
  host_->SetDuration(duration_);
  host_->SetCurrentReadPosition(0);

  ChangeState_Locked(INITIALIZED);
  PipelineStatusCB cb;
  std::swap(cb, init_cb_);
  cb.Run(PIPELINE_OK);
}

bool ChunkDemuxer::OnNewConfigs(const AudioDecoderConfig& audio_config,
                                const VideoDecoderConfig& video_config) {
  CHECK(audio_config.IsValidConfig() || video_config.IsValidConfig());
  lock_.AssertAcquired();

  // Only allow a single audio config for now.
  if (audio_config.IsValidConfig()) {
    if (audio_.get())
      return false;

    audio_ = new ChunkDemuxerStream(audio_config);
  }

  // Only allow a single video config for now.
  if (video_config.IsValidConfig()) {
    if (video_.get())
      return false;

    video_ = new ChunkDemuxerStream(video_config);
  }

  return true;
}

bool ChunkDemuxer::OnAudioBuffers(const StreamParser::BufferQueue& buffers) {
  if (!audio_.get())
    return false;

  if (!audio_->CanAddBuffers(buffers))
    return false;

  audio_->AddBuffers(buffers);
  seek_waits_for_data_ = false;

  return true;
}

bool ChunkDemuxer::OnVideoBuffers(const StreamParser::BufferQueue& buffers) {
  if (!video_.get())
    return false;

  if (!video_->CanAddBuffers(buffers))
    return false;

  video_->AddBuffers(buffers);
  seek_waits_for_data_ = false;

  return true;
}

bool ChunkDemuxer::OnKeyNeeded(scoped_array<uint8> init_data,
                               int init_data_size) {
  client_->KeyNeeded(init_data.Pass(), init_data_size);
  return true;
}

}  // namespace media
