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

struct CodecInfo {
  const char* name;
  DemuxerStream::Type type;
};

typedef StreamParser* (*ParserFactoryFunction)();

struct SupportedTypeInfo {
  const char* type;
  const ParserFactoryFunction factory_function;
  const CodecInfo** codecs;
};

static const CodecInfo kVP8CodecInfo = { "vp8", DemuxerStream::VIDEO };
static const CodecInfo kVorbisCodecInfo = { "vorbis", DemuxerStream::AUDIO };

static const CodecInfo* kVideoWebMCodecs[] = {
  &kVP8CodecInfo,
  &kVorbisCodecInfo,
  NULL
};

static const CodecInfo* kAudioWebMCodecs[] = {
  &kVorbisCodecInfo,
  NULL
};

static StreamParser* BuildWebMParser() {
  return new WebMStreamParser();
}

static const SupportedTypeInfo kSupportedTypeInfo[] = {
  { "video/webm", &BuildWebMParser, kVideoWebMCodecs },
  { "audio/webm", &BuildWebMParser, kAudioWebMCodecs },
};

// Checks to see if the specified |type| and |codecs| list are supported.
// Returns true if |type| and all codecs listed in |codecs| are supported.
//         |factory_function| contains a function that can build a StreamParser
//                            for this type.
//         |has_audio| is true if an audio codec was specified.
//         |has_video| is true if a video codec was specified.
// Returns false otherwise. The values of |factory_function|, |has_audio|,
//         and |has_video| are undefined.
static bool IsSupported(const std::string& type,
                        std::vector<std::string>& codecs,
                        ParserFactoryFunction* factory_function,
                        bool* has_audio,
                        bool* has_video) {
  *factory_function = NULL;
  *has_audio = false;
  *has_video = false;

  // Search for the SupportedTypeInfo for |type|
  for (size_t i = 0; i < arraysize(kSupportedTypeInfo); ++i) {
    const SupportedTypeInfo& type_info = kSupportedTypeInfo[i];
    if (type == type_info.type) {
      // Make sure all the codecs specified in |codecs| are
      // in the supported type info.
      for (size_t j = 0; j < codecs.size(); ++j) {
        // Search the type info for a match.
        bool found_codec = false;
        DemuxerStream::Type codec_type = DemuxerStream::UNKNOWN;

        for (int k = 0; type_info.codecs[k]; ++k) {
          if (codecs[j] == type_info.codecs[k]->name) {
            found_codec = true;
            codec_type = type_info.codecs[k]->type;
            break;
          }
        }

        if (!found_codec)
          return false;

        switch (codec_type) {
          case DemuxerStream::AUDIO:
            *has_audio = true;
            break;
          case DemuxerStream::VIDEO:
            *has_video = true;
            break;
          default:
            DVLOG(1) << "Unsupported codec type '"<< codec_type << "' for "
                     << codecs[j];
            return false;
        }
      }

      *factory_function = type_info.factory_function;

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

  ChunkDemuxerStream(Type type, ChunkDemuxer* chunk_demuxer);

  void StartWaitingForSeek();
  void Seek(base::TimeDelta time);

  // Notifies this stream of new buffers.
  void OnBuffersAvailable();

  void Shutdown();

  // DemuxerStream methods.
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual Type type() OVERRIDE;
  virtual void EnableBitstreamConverter() OVERRIDE;
  virtual const AudioDecoderConfig& audio_decoder_config() OVERRIDE;
  virtual const VideoDecoderConfig& video_decoder_config() OVERRIDE;

 protected:
  virtual ~ChunkDemuxerStream();

 private:
  enum State {
    RETURNING_DATA_FOR_READS,
    WAITING_FOR_SEEK,
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

  // Specifies the type of the stream (must be AUDIO or VIDEO for now).
  Type type_;

  // Pointer to the ChunkDemuxer that owns this ChunkDemuxerStream.  Used for
  // read requests to SourceBuffer.
  ChunkDemuxer* chunk_demuxer_;

  mutable base::Lock lock_;
  State state_;
  ReadCBQueue read_cbs_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChunkDemuxerStream);
};

ChunkDemuxerStream::ChunkDemuxerStream(Type type, ChunkDemuxer* chunk_demuxer)
    : type_(type),
      chunk_demuxer_(chunk_demuxer),
      state_(RETURNING_DATA_FOR_READS) {}

void ChunkDemuxerStream::StartWaitingForSeek() {
  DVLOG(1) << "StartWaitingForSeek()";
  ReadCBQueue read_cbs;
  {
    base::AutoLock auto_lock(lock_);
    ChangeState_Locked(WAITING_FOR_SEEK);

    std::swap(read_cbs_, read_cbs);
  }

  for (ReadCBQueue::iterator it = read_cbs.begin(); it != read_cbs.end(); ++it)
    it->Run(scoped_refptr<Buffer>());
}

void ChunkDemuxerStream::Seek(base::TimeDelta time) {
  base::AutoLock auto_lock(lock_);

  DCHECK(read_cbs_.empty());

  if (state_ == WAITING_FOR_SEEK)
    ChangeState_Locked(RETURNING_DATA_FOR_READS);
}

void ChunkDemuxerStream::OnBuffersAvailable() {
  ClosureQueue closures;
  {
    base::AutoLock auto_lock(lock_);
    DCHECK_NE(state_, SHUTDOWN);
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
  }

  // Pass end of stream buffers to all callbacks to signal that no more data
  // will be sent.
  for (ReadCBQueue::iterator it = read_cbs.begin(); it != read_cbs.end(); ++it)
    it->Run(StreamParserBuffer::CreateEOSBuffer());
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
  scoped_refptr<StreamParserBuffer> buffer;
  {
    base::AutoLock auto_lock(lock_);

    switch (state_) {
      case RETURNING_DATA_FOR_READS:
        // If we already have pending reads, then defer this read.
        if (!read_cbs_.empty()) {
          DeferRead_Locked(read_cb);
          return;
        }

        bool read_success;
        {
          // Unlock is necessary to avoid deadlock with OnBuffersAvailable().
          base::AutoUnlock auto_unlock(lock_);
          read_success = chunk_demuxer_->SourceBufferRead(type_, &buffer);
        }

        // Check whether the state may have changed during the unlock.
        switch (state_) {
          case RETURNING_DATA_FOR_READS:
            if (!read_success) {
              DeferRead_Locked(read_cb);
              return;
            }
            break;
          case WAITING_FOR_SEEK:
            DCHECK(read_cbs_.empty());
            buffer = NULL;
            break;
          case SHUTDOWN:
            DCHECK(read_cbs_.empty());
            buffer = StreamParserBuffer::CreateEOSBuffer();
        }

        break;
      case WAITING_FOR_SEEK:
        // Null buffers should be returned in this state since we are waiting
        // for a seek. Any buffers in the SourceBuffer should NOT be returned
        // because they are associated with the seek.
        DCHECK(read_cbs_.empty());
        break;
      case SHUTDOWN:
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
  return chunk_demuxer_->GetCurrentAudioDecoderConfig();
}

const VideoDecoderConfig& ChunkDemuxerStream::video_decoder_config() {
  CHECK_EQ(type_, VIDEO);
  return chunk_demuxer_->GetCurrentVideoDecoderConfig();
}

void ChunkDemuxerStream::ChangeState_Locked(State state) {
  lock_.AssertAcquired();
  state_ = state;
}

ChunkDemuxerStream::~ChunkDemuxerStream() {}

void ChunkDemuxerStream::DeferRead_Locked(const ReadCB& read_cb) {
  lock_.AssertAcquired();
  // Wrap & store |read_cb| so that it will
  // get called on the current MessageLoop.
  read_cbs_.push_back(base::Bind(&RunOnMessageLoop, read_cb,
                                 MessageLoop::current()));
}

void ChunkDemuxerStream::CreateReadDoneClosures_Locked(ClosureQueue* closures) {
  lock_.AssertAcquired();

  if (state_ != RETURNING_DATA_FOR_READS)
    return;

  scoped_refptr<StreamParserBuffer> buffer;
  while (!read_cbs_.empty()) {
    if (!chunk_demuxer_->SourceBufferRead_Locked(type_, &buffer))
      return;
    closures->push_back(base::Bind(read_cbs_.front(), buffer));
    read_cbs_.pop_front();
  }
}

ChunkDemuxer::ChunkDemuxer(ChunkDemuxerClient* client)
    : state_(WAITING_FOR_INIT),
      host_(NULL),
      client_(client),
      buffered_bytes_(0) {
  DCHECK(client);
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

      source_buffer_->Seek(time);

      if (source_buffer_->IsSeekPending()) {
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

void ChunkDemuxer::StartWaitingForSeek() {
  DVLOG(1) << "StartWaitingForSeek()";
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == INITIALIZED || state_ == ENDED || state_ == SHUTDOWN);

  if (state_ == SHUTDOWN)
    return;

  if (audio_.get())
    audio_->StartWaitingForSeek();

  if (video_.get())
    video_->StartWaitingForSeek();

  source_buffer_->ResetParser();

  ChangeState_Locked(INITIALIZED);
}

ChunkDemuxer::Status ChunkDemuxer::AddId(const std::string& id,
                                         const std::string& type,
                                         std::vector<std::string>& codecs) {
  DCHECK_GT(codecs.size(), 0u);

  bool has_audio = false;
  bool has_video = false;
  ParserFactoryFunction factory_function = NULL;
  if (!IsSupported(type, codecs, &factory_function, &has_audio, &has_video))
    return kNotSupported;

  // TODO(acolwell): Support for more than one ID
  // will be added as part of http://crbug.com/122909
  if (!source_id_.empty())
    return kReachedIdLimit;

  source_id_ = id;

  SourceBuffer::NewBuffersCB audio_cb;
  SourceBuffer::NewBuffersCB video_cb;

  if (has_audio) {
    audio_cb = base::Bind(&ChunkDemuxer::OnAudioBuffers,
                          base::Unretained(this));
  }

  if (has_video) {
    video_cb = base::Bind(&ChunkDemuxer::OnVideoBuffers,
                          base::Unretained(this));
  }

  scoped_ptr<StreamParser> stream_parser(factory_function());

  CHECK(stream_parser.get());

  source_buffer_.reset(new SourceBuffer());
  source_buffer_->Init(
      stream_parser.Pass(),
      base::Bind(&ChunkDemuxer::OnSourceBufferInitDone, this),
      base::Bind(&ChunkDemuxer::OnNewConfigs, base::Unretained(this)),
      audio_cb,
      video_cb,
      base::Bind(&ChunkDemuxer::OnKeyNeeded, base::Unretained(this)));

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

  return source_buffer_->GetBufferedRanges(ranges_out);
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

  PipelineStatusCB cb;
  {
    base::AutoLock auto_lock(lock_);

    // Capture if the SourceBuffer has a pending seek before we start parsing.
    bool old_seek_pending = source_buffer_->IsSeekPending();

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

    // Check to see if data was appended at the pending seek point. This
    // indicates we have parsed enough data to complete the seek.
    if (old_seek_pending && !source_buffer_->IsSeekPending() &&
        !seek_cb_.is_null()) {
      std::swap(cb, seek_cb_);
    }

    buffered_bytes_ += length;
    buffered_bytes = buffered_bytes_;
  }

  // Notify the host of 'network activity' because we got data, using a bogus
  // range.
  host_->AddBufferedByteRange(0, buffered_bytes);

  host_->SetNetworkActivity(true);

  if (!cb.is_null())
    cb.Run(PIPELINE_OK);

  return true;
}

void ChunkDemuxer::Abort(const std::string& id) {
  DCHECK(!id.empty());
  DCHECK_EQ(source_id_, id);

  source_buffer_->ResetParser();
}

const AudioDecoderConfig& ChunkDemuxer::GetCurrentAudioDecoderConfig() {
  base::AutoLock auto_lock(lock_);
  return source_buffer_->GetCurrentAudioDecoderConfig();
}

const VideoDecoderConfig& ChunkDemuxer::GetCurrentVideoDecoderConfig() {
  base::AutoLock auto_lock(lock_);
  return source_buffer_->GetCurrentVideoDecoderConfig();
}

bool ChunkDemuxer::SourceBufferRead(DemuxerStream::Type type,
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  base::AutoLock auto_lock(lock_);
  return SourceBufferRead_Locked(type, out_buffer);
}

bool ChunkDemuxer::SourceBufferRead_Locked(DemuxerStream::Type type,
    scoped_refptr<StreamParserBuffer>* out_buffer) {
  lock_.AssertAcquired();
  return source_buffer_->Read(type, out_buffer);
}

bool ChunkDemuxer::EndOfStream(PipelineStatus status) {
  DVLOG(1) << "EndOfStream(" << status << ")";
  base::AutoLock auto_lock(lock_);
  DCHECK_NE(state_, WAITING_FOR_INIT);
  DCHECK_NE(state_, ENDED);

  if (state_ == SHUTDOWN || state_ == PARSE_ERROR)
    return true;

  if (state_ == INITIALIZING) {
    ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
    return true;
  }

  if (!source_buffer_->EndOfStream() && status == PIPELINE_OK)
    return false;

  ChangeState_Locked(ENDED);

  if (status != PIPELINE_OK)
    ReportError_Locked(status);

  return true;
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

ChunkDemuxer::~ChunkDemuxer() {
  DCHECK_NE(state_, INITIALIZED);
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

    audio_ = new ChunkDemuxerStream(DemuxerStream::AUDIO, this);
  }

  // Only allow a single video config for now.
  if (video_config.IsValidConfig()) {
    if (video_.get())
      return false;

    video_ = new ChunkDemuxerStream(DemuxerStream::VIDEO, this);
  }

  return true;
}

bool ChunkDemuxer::OnAudioBuffers() {
  DCHECK_NE(state_, SHUTDOWN);

  if (!audio_.get())
    return false;

  audio_->OnBuffersAvailable();

  return true;
}

bool ChunkDemuxer::OnVideoBuffers() {
  DCHECK_NE(state_, SHUTDOWN);

  if (!video_.get())
    return false;

  video_->OnBuffersAvailable();

  return true;
}

bool ChunkDemuxer::OnKeyNeeded(scoped_array<uint8> init_data,
                               int init_data_size) {
  client_->KeyNeeded(init_data.Pass(), init_data_size);
  return true;
}

}  // namespace media
