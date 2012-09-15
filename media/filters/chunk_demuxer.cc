// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/chunk_demuxer.h"

#include <algorithm>
#include <deque>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/string_util.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/video_decoder_config.h"
#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
#include "media/mp4/mp4_stream_parser.h"
#endif
#include "media/webm/webm_stream_parser.h"

using base::TimeDelta;

namespace media {

struct CodecInfo {
  const char* pattern;
  DemuxerStream::Type type;
};

typedef StreamParser* (*ParserFactoryFunction)(
    const std::vector<std::string>& codecs);

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

static StreamParser* BuildWebMParser(const std::vector<std::string>& codecs) {
  return new WebMStreamParser();
}

#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
static const CodecInfo kH264CodecInfo = { "avc1.*", DemuxerStream::VIDEO };
static const CodecInfo kAACCodecInfo = { "mp4a.40.*", DemuxerStream::AUDIO };

static const CodecInfo* kVideoMP4Codecs[] = {
  &kH264CodecInfo,
  &kAACCodecInfo,
  NULL
};

static const CodecInfo* kAudioMP4Codecs[] = {
  &kAACCodecInfo,
  NULL
};

// Mimetype codec string that indicates the content contains AAC SBR frames.
static const char* kSBRCodecId = "mp4a.40.5";

static StreamParser* BuildMP4Parser(const std::vector<std::string>& codecs) {
  bool has_sbr = false;
  for (size_t i = 0; i < codecs.size(); ++i) {
    if (codecs[i] == kSBRCodecId) {
      has_sbr = true;
      break;
    }
  }

  return new mp4::MP4StreamParser(has_sbr);
}
#endif

static const SupportedTypeInfo kSupportedTypeInfo[] = {
  { "video/webm", &BuildWebMParser, kVideoWebMCodecs },
  { "audio/webm", &BuildWebMParser, kAudioWebMCodecs },
#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
  { "video/mp4", &BuildMP4Parser, kVideoMP4Codecs },
  { "audio/mp4", &BuildMP4Parser, kAudioMP4Codecs },
#endif
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

  // Search for the SupportedTypeInfo for |type|.
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
          if (MatchPattern(codecs[j], type_info.codecs[k]->pattern)) {
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

  explicit ChunkDemuxerStream(const AudioDecoderConfig& audio_config);
  explicit ChunkDemuxerStream(const VideoDecoderConfig& video_config);

  void StartWaitingForSeek();
  void Seek(TimeDelta time);
  void CancelPendingSeek();
  bool IsSeekPending() const;

  // Add buffers to this stream.  Buffers are stored in SourceBufferStreams,
  // which handle ordering and overlap resolution.
  // Returns true if buffers were successfully added.
  bool Append(const StreamParser::BufferQueue& buffers);

  // Signal to the stream that duration has changed to |duration|.
  void OnSetDuration(base::TimeDelta duration);

  // Returns the range of buffered data in this stream, capped at |duration|.
  Ranges<TimeDelta> GetBufferedRanges(base::TimeDelta duration) const;

  // Signal to the stream that buffers handed in through subsequent calls to
  // Append() belong to a media segment that starts at |start_timestamp|.
  void OnNewMediaSegment(TimeDelta start_timestamp);

  // Called when mid-stream config updates occur.
  // Returns true if the new config is accepted.
  // Returns false if the new config should trigger an error.
  bool UpdateAudioConfig(const AudioDecoderConfig& config);
  bool UpdateVideoConfig(const VideoDecoderConfig& config);

  void EndOfStream();
  bool CanEndOfStream() const;
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
    CANCELED,
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

  // Gets the value to pass to the next Read() callback. Returns true if
  // |status| and |buffer| should be passed to the callback. False indicates
  // that |status| and |buffer| were not set and more data is needed.
  bool GetNextBuffer_Locked(DemuxerStream::Status* status,
                            scoped_refptr<StreamParserBuffer>* buffer);

  // Specifies the type of the stream (must be AUDIO or VIDEO for now).
  Type type_;

  scoped_ptr<SourceBufferStream> stream_;

  mutable base::Lock lock_;
  State state_;
  ReadCBQueue read_cbs_;
  bool end_of_stream_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChunkDemuxerStream);
};

ChunkDemuxerStream::ChunkDemuxerStream(const AudioDecoderConfig& audio_config)
    : type_(AUDIO),
      state_(RETURNING_DATA_FOR_READS),
      end_of_stream_(false) {
  stream_.reset(new SourceBufferStream(audio_config));
}

ChunkDemuxerStream::ChunkDemuxerStream(const VideoDecoderConfig& video_config)
    : type_(VIDEO),
      state_(RETURNING_DATA_FOR_READS),
      end_of_stream_(false) {
  stream_.reset(new SourceBufferStream(video_config));
}

void ChunkDemuxerStream::StartWaitingForSeek() {
  DVLOG(1) << "StartWaitingForSeek()";
  ReadCBQueue read_cbs;
  {
    base::AutoLock auto_lock(lock_);
    ChangeState_Locked(WAITING_FOR_SEEK);
    end_of_stream_ = false;
    std::swap(read_cbs_, read_cbs);
  }

  for (ReadCBQueue::iterator it = read_cbs.begin(); it != read_cbs.end(); ++it)
    it->Run(kAborted, NULL);
}

void ChunkDemuxerStream::Seek(TimeDelta time) {
  base::AutoLock auto_lock(lock_);

  DCHECK(read_cbs_.empty());

  // Ignore seek requests when canceled.
  if (state_ == CANCELED)
    return;

  stream_->Seek(time);

  if (state_ == WAITING_FOR_SEEK)
    ChangeState_Locked(RETURNING_DATA_FOR_READS);
}

void ChunkDemuxerStream::CancelPendingSeek() {
  DVLOG(1) << "CancelPendingSeek()";
  ReadCBQueue read_cbs;
  {
    base::AutoLock auto_lock(lock_);
    ChangeState_Locked(CANCELED);
    std::swap(read_cbs_, read_cbs);
  }

  for (ReadCBQueue::iterator it = read_cbs.begin(); it != read_cbs.end(); ++it)
    it->Run(kAborted, NULL);
}

bool ChunkDemuxerStream::IsSeekPending() const {
  base::AutoLock auto_lock(lock_);
  return stream_->IsSeekPending();
}

void ChunkDemuxerStream::OnNewMediaSegment(TimeDelta start_timestamp) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!end_of_stream_);
  stream_->OnNewMediaSegment(start_timestamp);
}

bool ChunkDemuxerStream::Append(const StreamParser::BufferQueue& buffers) {
  if (buffers.empty())
    return false;

  ClosureQueue closures;
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(!end_of_stream_);
    DCHECK_NE(state_, SHUTDOWN);
    if (!stream_->Append(buffers)) {
      DVLOG(1) << "ChunkDemuxerStream::Append() : stream append failed";
      return false;
    }
    CreateReadDoneClosures_Locked(&closures);
  }

  for (ClosureQueue::iterator it = closures.begin(); it != closures.end(); ++it)
    it->Run();

  return true;
}

void ChunkDemuxerStream::OnSetDuration(base::TimeDelta duration) {
  base::AutoLock auto_lock(lock_);
  stream_->OnSetDuration(duration);
}

Ranges<TimeDelta> ChunkDemuxerStream::GetBufferedRanges(
    base::TimeDelta duration) const {
  base::AutoLock auto_lock(lock_);
  Ranges<TimeDelta> range = stream_->GetBufferedTime();

  if (range.size() == 0u)
    return range;

  // Clamp the end of the stream's buffered ranges to fit within the duration.
  // This can be done by intersecting the stream's range with the valid time
  // range.
  Ranges<TimeDelta> valid_time_range;
  valid_time_range.Add(range.start(0), duration);
  return range.IntersectionWith(valid_time_range);
}

bool ChunkDemuxerStream::UpdateAudioConfig(const AudioDecoderConfig& config) {
  DCHECK(config.IsValidConfig());
  DCHECK_EQ(type_, AUDIO);
  base::AutoLock auto_lock(lock_);
  return stream_->UpdateAudioConfig(config);
}

bool ChunkDemuxerStream::UpdateVideoConfig(const VideoDecoderConfig& config) {
  DCHECK(config.IsValidConfig());
  DCHECK_EQ(type_, VIDEO);
  base::AutoLock auto_lock(lock_);
  return stream_->UpdateVideoConfig(config);
}

void ChunkDemuxerStream::EndOfStream() {
  ClosureQueue closures;
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(!end_of_stream_);
    DCHECK(stream_->IsEndSelected());
    end_of_stream_ = true;
    CreateReadDoneClosures_Locked(&closures);
  }

  for (ClosureQueue::iterator it = closures.begin(); it != closures.end(); ++it)
    it->Run();
}

bool ChunkDemuxerStream::CanEndOfStream() const {
  base::AutoLock auto_lock(lock_);
  return stream_->IsEndSelected();
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
    it->Run(DemuxerStream::kOk, StreamParserBuffer::CreateEOSBuffer());
}

// Helper function that makes sure |read_cb| runs on |message_loop_proxy|.
static void RunOnMessageLoop(
    const DemuxerStream::ReadCB& read_cb,
    const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  if (!message_loop_proxy->BelongsToCurrentThread()) {
    message_loop_proxy->PostTask(FROM_HERE, base::Bind(
        &RunOnMessageLoop, read_cb, message_loop_proxy, status, buffer));
    return;
  }

  read_cb.Run(status, buffer);
}

// DemuxerStream methods.
void ChunkDemuxerStream::Read(const ReadCB& read_cb) {
  DemuxerStream::Status status = kOk;
  scoped_refptr<StreamParserBuffer> buffer;
  {
    base::AutoLock auto_lock(lock_);
    if (!read_cbs_.empty() || !GetNextBuffer_Locked(&status, &buffer)) {
      DeferRead_Locked(read_cb);
      return;
    }
  }

  read_cb.Run(status, buffer);
}

DemuxerStream::Type ChunkDemuxerStream::type() { return type_; }

void ChunkDemuxerStream::EnableBitstreamConverter() {}

const AudioDecoderConfig& ChunkDemuxerStream::audio_decoder_config() {
  CHECK_EQ(type_, AUDIO);
  base::AutoLock auto_lock(lock_);
  return stream_->GetCurrentAudioDecoderConfig();
}

const VideoDecoderConfig& ChunkDemuxerStream::video_decoder_config() {
  CHECK_EQ(type_, VIDEO);
  base::AutoLock auto_lock(lock_);
  return stream_->GetCurrentVideoDecoderConfig();
}

void ChunkDemuxerStream::ChangeState_Locked(State state) {
  lock_.AssertAcquired();
  DVLOG(1) << "ChunkDemuxerStream::ChangeState_Locked() : "
           << "type " << type_
           << " - " << state_ << " -> " << state;
  state_ = state;
}

ChunkDemuxerStream::~ChunkDemuxerStream() {}

void ChunkDemuxerStream::DeferRead_Locked(const ReadCB& read_cb) {
  lock_.AssertAcquired();
  // Wrap & store |read_cb| so that it will
  // get called on the current MessageLoop.
  read_cbs_.push_back(base::Bind(&RunOnMessageLoop, read_cb,
                                 base::MessageLoopProxy::current()));
}

void ChunkDemuxerStream::CreateReadDoneClosures_Locked(ClosureQueue* closures) {
  lock_.AssertAcquired();

  if (state_ != RETURNING_DATA_FOR_READS)
    return;

  DemuxerStream::Status status;
  scoped_refptr<StreamParserBuffer> buffer;
  while (!read_cbs_.empty()) {
    if (!GetNextBuffer_Locked(&status, &buffer))
      return;
    closures->push_back(base::Bind(read_cbs_.front(),
                                   status, buffer));
    read_cbs_.pop_front();
  }
}

bool ChunkDemuxerStream::GetNextBuffer_Locked(
    DemuxerStream::Status* status,
    scoped_refptr<StreamParserBuffer>* buffer) {
  lock_.AssertAcquired();

  switch (state_) {
    case RETURNING_DATA_FOR_READS:
      switch (stream_->GetNextBuffer(buffer)) {
        case SourceBufferStream::kSuccess:
          *status = DemuxerStream::kOk;
          return true;
        case SourceBufferStream::kNeedBuffer:
          if (end_of_stream_) {
            *status = DemuxerStream::kOk;
            *buffer = StreamParserBuffer::CreateEOSBuffer();
            return true;
          }
          return false;
        case SourceBufferStream::kConfigChange:
          *status = kConfigChanged;
          *buffer = NULL;
          return true;
      }
      break;
    case CANCELED:
    case WAITING_FOR_SEEK:
      // Null buffers should be returned in this state since we are waiting
      // for a seek. Any buffers in the SourceBuffer should NOT be returned
      // because they are associated with the seek.
      DCHECK(read_cbs_.empty());
      *status = DemuxerStream::kAborted;
      *buffer = NULL;
      return true;
    case SHUTDOWN:
      DCHECK(read_cbs_.empty());
      *status = DemuxerStream::kOk;
      *buffer = StreamParserBuffer::CreateEOSBuffer();
      return true;
  }

  NOTREACHED();
  return false;
}

ChunkDemuxer::ChunkDemuxer(const base::Closure& open_cb,
                           const NeedKeyCB& need_key_cb)
    : state_(WAITING_FOR_INIT),
      host_(NULL),
      open_cb_(open_cb),
      need_key_cb_(need_key_cb) {
  DCHECK(!open_cb_.is_null());
  DCHECK(!need_key_cb_.is_null());
}

void ChunkDemuxer::Initialize(DemuxerHost* host, const PipelineStatusCB& cb) {
  DVLOG(1) << "Init()";

  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, WAITING_FOR_INIT);
  host_ = host;

  ChangeState_Locked(INITIALIZING);
  init_cb_ = cb;

  base::ResetAndReturn(&open_cb_).Run();
}

void ChunkDemuxer::Stop(const base::Closure& callback) {
  DVLOG(1) << "Stop()";
  Shutdown();
  callback.Run();
}

void ChunkDemuxer::Seek(TimeDelta time, const PipelineStatusCB& cb) {
  DVLOG(1) << "Seek(" << time.InSecondsF() << ")";
  DCHECK(time >= TimeDelta());
  DCHECK(seek_cb_.is_null());

  PipelineStatus status = PIPELINE_ERROR_INVALID_STATE;
  {
    base::AutoLock auto_lock(lock_);

    if (state_ == INITIALIZED || state_ == ENDED) {
      if (audio_)
        audio_->Seek(time);

      if (video_)
        video_->Seek(time);

      if (IsSeekPending_Locked()) {
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

// Demuxer implementation.
scoped_refptr<DemuxerStream> ChunkDemuxer::GetStream(
    DemuxerStream::Type type) {
  if (type == DemuxerStream::VIDEO)
    return video_;

  if (type == DemuxerStream::AUDIO)
    return audio_;

  return NULL;
}

TimeDelta ChunkDemuxer::GetStartTime() const {
  return TimeDelta();
}

void ChunkDemuxer::StartWaitingForSeek() {
  DVLOG(1) << "StartWaitingForSeek()";
  base::AutoLock auto_lock(lock_);
  DCHECK(state_ == INITIALIZED || state_ == ENDED || state_ == SHUTDOWN);

  if (state_ == SHUTDOWN)
    return;

  if (audio_)
    audio_->StartWaitingForSeek();

  if (video_)
    video_->StartWaitingForSeek();

  ChangeState_Locked(INITIALIZED);
}

void ChunkDemuxer::CancelPendingSeek() {
  PipelineStatusCB cb;
  {
    base::AutoLock auto_lock(lock_);
    if (IsSeekPending_Locked() && !seek_cb_.is_null()) {
      std::swap(cb, seek_cb_);
    }
    if (audio_)
      audio_->CancelPendingSeek();

    if (video_)
      video_->CancelPendingSeek();
  }

  if (!cb.is_null())
    cb.Run(PIPELINE_OK);
}

ChunkDemuxer::Status ChunkDemuxer::AddId(const std::string& id,
                                         const std::string& type,
                                         std::vector<std::string>& codecs) {
  DCHECK_GT(codecs.size(), 0u);
  base::AutoLock auto_lock(lock_);

  if ((state_ != WAITING_FOR_INIT && state_ != INITIALIZING) ||
      stream_parser_map_.count(id) > 0u)
    return kReachedIdLimit;

  bool has_audio = false;
  bool has_video = false;
  ParserFactoryFunction factory_function = NULL;
  if (!IsSupported(type, codecs, &factory_function, &has_audio, &has_video))
    return kNotSupported;

  if ((has_audio && !source_id_audio_.empty()) ||
      (has_video && !source_id_video_.empty()))
    return kReachedIdLimit;

  StreamParser::NewBuffersCB audio_cb;
  StreamParser::NewBuffersCB video_cb;

  if (has_audio) {
    source_id_audio_ = id;
    audio_cb = base::Bind(&ChunkDemuxer::OnAudioBuffers,
                          base::Unretained(this));
  }

  if (has_video) {
    source_id_video_ = id;
    video_cb = base::Bind(&ChunkDemuxer::OnVideoBuffers,
                          base::Unretained(this));
  }

  scoped_ptr<StreamParser> stream_parser(factory_function(codecs));
  CHECK(stream_parser.get());

  stream_parser->Init(
      base::Bind(&ChunkDemuxer::OnStreamParserInitDone, base::Unretained(this)),
      base::Bind(&ChunkDemuxer::OnNewConfigs, base::Unretained(this),
                 has_audio, has_video),
      audio_cb,
      video_cb,
      base::Bind(&ChunkDemuxer::OnNeedKey, base::Unretained(this)),
      base::Bind(&ChunkDemuxer::OnNewMediaSegment, base::Unretained(this), id),
      base::Bind(&ChunkDemuxer::OnEndOfMediaSegment,
                 base::Unretained(this), id));

  stream_parser_map_[id] = stream_parser.release();
  SourceInfo info = { base::TimeDelta(), true };
  source_info_map_[id] = info;

  return kOk;
}

void ChunkDemuxer::RemoveId(const std::string& id) {
  CHECK(IsValidId(id));
  base::AutoLock auto_lock(lock_);

  delete stream_parser_map_[id];
  stream_parser_map_.erase(id);
  source_info_map_.erase(id);

  if (source_id_audio_ == id && audio_)
    audio_->Shutdown();

  if (source_id_video_ == id && video_)
    video_->Shutdown();
}

Ranges<TimeDelta> ChunkDemuxer::GetBufferedRanges(const std::string& id) const {
  DCHECK(!id.empty());
  DCHECK(IsValidId(id));
  DCHECK(id == source_id_audio_ || id == source_id_video_);

  base::AutoLock auto_lock(lock_);

  if (id == source_id_audio_ && id != source_id_video_) {
    // Only include ranges that have been buffered in |audio_|
    return audio_ ? audio_->GetBufferedRanges(duration_) : Ranges<TimeDelta>();
  }

  if (id != source_id_audio_ && id == source_id_video_) {
    // Only include ranges that have been buffered in |video_|
    return video_ ? video_->GetBufferedRanges(duration_) : Ranges<TimeDelta>();
  }

  return ComputeIntersection();
}

Ranges<TimeDelta> ChunkDemuxer::ComputeIntersection() const {
  lock_.AssertAcquired();

  if (!audio_ || !video_)
    return Ranges<TimeDelta>();

  // Include ranges that have been buffered in both |audio_| and |video_|.
  Ranges<TimeDelta> audio_ranges = audio_->GetBufferedRanges(duration_);
  Ranges<TimeDelta> video_ranges = video_->GetBufferedRanges(duration_);
  Ranges<TimeDelta> result = audio_ranges.IntersectionWith(video_ranges);

  if (state_ == ENDED && result.size() > 0) {
    // If appending has ended, extend the last intersection range to include the
    // max end time of the last audio/video range. This allows the buffered
    // information to match the actual time range that will get played out if
    // the streams have slightly different lengths.
    TimeDelta audio_start = audio_ranges.start(audio_ranges.size() - 1);
    TimeDelta audio_end = audio_ranges.end(audio_ranges.size() - 1);
    TimeDelta video_start = video_ranges.start(video_ranges.size() - 1);
    TimeDelta video_end = video_ranges.end(video_ranges.size() - 1);

    // Verify the last audio range overlaps with the last video range.
    // This is enforced by the logic that controls the transition to ENDED.
    DCHECK((audio_start <= video_start && video_start <= audio_end) ||
           (video_start <= audio_start && audio_start <= video_end));
    result.Add(result.end(result.size()-1), std::max(audio_end, video_end));
  }

  return result;
}

bool ChunkDemuxer::AppendData(const std::string& id,
                              const uint8* data,
                              size_t length) {
  DVLOG(1) << "AppendData(" << id << ", " << length << ")";

  DCHECK(!id.empty());
  DCHECK(data);
  DCHECK_GT(length, 0u);

  Ranges<TimeDelta> ranges;

  PipelineStatusCB cb;
  {
    base::AutoLock auto_lock(lock_);

    // Capture if the SourceBuffer has a pending seek before we start parsing.
    bool old_seek_pending = IsSeekPending_Locked();

    switch (state_) {
      case INITIALIZING:
        DCHECK(IsValidId(id));
        if (!stream_parser_map_[id]->Parse(data, length)) {
          ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
          return true;
        }
        break;

      case INITIALIZED: {
        DCHECK(IsValidId(id));
        if (!stream_parser_map_[id]->Parse(data, length)) {
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
    if (old_seek_pending && !IsSeekPending_Locked() && !seek_cb_.is_null()) {
      std::swap(cb, seek_cb_);
    }

    ranges = GetBufferedRanges();
  }

  for (size_t i = 0; i < ranges.size(); ++i)
    host_->AddBufferedTimeRange(ranges.start(i), ranges.end(i));

  if (!cb.is_null())
    cb.Run(PIPELINE_OK);

  return true;
}

void ChunkDemuxer::Abort(const std::string& id) {
  DVLOG(1) << "Abort(" << id << ")";
  DCHECK(!id.empty());
  CHECK(IsValidId(id));

  stream_parser_map_[id]->Flush();
  source_info_map_[id].can_update_offset = true;
}

void ChunkDemuxer::SetDuration(base::TimeDelta duration) {
  DVLOG(1) << "SetDuration(" << duration.InSecondsF() << ")";

  if (duration == duration_)
    return;

  UpdateDuration(duration);

  if (audio_)
    audio_->OnSetDuration(duration);

  if (video_)
    video_->OnSetDuration(duration);
}

bool ChunkDemuxer::SetTimestampOffset(const std::string& id, TimeDelta offset) {
  DVLOG(1) << "SetTimestampOffset(" << id << ", " << offset.InSecondsF() << ")";
  CHECK(IsValidId(id));

  if (!source_info_map_[id].can_update_offset)
    return false;

  source_info_map_[id].timestamp_offset = offset;
  return true;
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

  if (!CanEndOfStream_Locked() && status == PIPELINE_OK)
    return false;

  if (audio_)
    audio_->EndOfStream();

  if (video_)
    video_->EndOfStream();

  if (status != PIPELINE_OK) {
    ReportError_Locked(status);
  } else {
    ChangeState_Locked(ENDED);
    DecreaseDurationIfNecessary();
  }

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

    if (audio_)
      audio_->Shutdown();

    if (video_)
      video_->Shutdown();

    ChangeState_Locked(SHUTDOWN);
  }

  if (!cb.is_null())
    cb.Run(PIPELINE_ERROR_ABORT);
}

void ChunkDemuxer::ChangeState_Locked(State new_state) {
  lock_.AssertAcquired();
  DVLOG(1) << "ChunkDemuxer::ChangeState_Locked() : "
           << state_ << " -> " << new_state;
  state_ = new_state;
}

ChunkDemuxer::~ChunkDemuxer() {
  DCHECK_NE(state_, INITIALIZED);
  for (StreamParserMap::iterator it = stream_parser_map_.begin();
       it != stream_parser_map_.end(); ++it) {
    delete it->second;
  }
  stream_parser_map_.clear();
}

void ChunkDemuxer::ReportError_Locked(PipelineStatus error) {
  DVLOG(1) << "ReportError_Locked(" << error << ")";
  lock_.AssertAcquired();
  DCHECK_NE(error, PIPELINE_OK);

  ChangeState_Locked(PARSE_ERROR);

  PipelineStatusCB cb;

  if (!init_cb_.is_null()) {
    std::swap(cb, init_cb_);
  } else {
    if (!seek_cb_.is_null())
      std::swap(cb, seek_cb_);

    if (audio_)
      audio_->Shutdown();

    if (video_)
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

bool ChunkDemuxer::IsSeekPending_Locked() const {
  lock_.AssertAcquired();
  bool seek_pending = false;

  if (audio_)
    seek_pending = audio_->IsSeekPending();

  if (!seek_pending && video_)
    seek_pending = video_->IsSeekPending();

  return seek_pending;
}

bool ChunkDemuxer::CanEndOfStream_Locked() const {
  lock_.AssertAcquired();
  return (!audio_ || audio_->CanEndOfStream()) &&
         (!video_ || video_->CanEndOfStream());
}

void ChunkDemuxer::OnStreamParserInitDone(bool success, TimeDelta duration) {
  DVLOG(1) << "OnStreamParserInitDone(" << success << ", "
           << duration.InSecondsF() << ")";
  lock_.AssertAcquired();
  DCHECK_EQ(state_, INITIALIZING);
  if (!success || (!audio_ && !video_)) {
    ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  if (duration != base::TimeDelta() && duration_ == base::TimeDelta())
    UpdateDuration(duration);

  // Wait until all streams have initialized.
  if ((!source_id_audio_.empty() && !audio_) ||
      (!source_id_video_.empty() && !video_))
    return;

  if (audio_)
    audio_->Seek(TimeDelta());

  if (video_)
    video_->Seek(TimeDelta());

  if (duration_ == TimeDelta())
    duration_ = kInfiniteDuration();

  // The demuxer is now initialized after the |start_timestamp_| was set.
  ChangeState_Locked(INITIALIZED);
  base::ResetAndReturn(&init_cb_).Run(PIPELINE_OK);

}

bool ChunkDemuxer::OnNewConfigs(bool has_audio, bool has_video,
                                const AudioDecoderConfig& audio_config,
                                const VideoDecoderConfig& video_config) {
  DVLOG(1) << "OnNewConfigs(" << has_audio << ", " << has_video
           << ", " << audio_config.IsValidConfig()
           << ", " << video_config.IsValidConfig() << ")";
  lock_.AssertAcquired();

  if (!audio_config.IsValidConfig() && !video_config.IsValidConfig()) {
    DVLOG(1) << "OnNewConfigs() : Audio & video config are not valid!";
    return false;
  }

  // Signal an error if we get configuration info for stream types that weren't
  // specified in AddId() or more configs after a stream is initialized.
  // Only allow a single audio config for now.
  if (has_audio != audio_config.IsValidConfig()) {
    DVLOG(1) << "OnNewConfigs() : Got unexpected audio config.";
    return false;
  }

  // Only allow a single video config for now.
  if (has_video != video_config.IsValidConfig()) {
    DVLOG(1) << "OnNewConfigs() : Got unexpected video config.";
    return false;
  }

  bool success = true;
  if (audio_config.IsValidConfig()) {
    if (audio_) {
      success &= audio_->UpdateAudioConfig(audio_config);
    } else {
      audio_ = new ChunkDemuxerStream(audio_config);
    }
  }

  if (video_config.IsValidConfig()) {
    if (video_) {
      success &= video_->UpdateVideoConfig(video_config);
    } else {
      video_ = new ChunkDemuxerStream(video_config);
    }
  }

  DVLOG(1) << "OnNewConfigs() : success " << success;
  return success;
}

bool ChunkDemuxer::OnAudioBuffers(const StreamParser::BufferQueue& buffers) {
  lock_.AssertAcquired();
  DCHECK_NE(state_, SHUTDOWN);

  if (!audio_)
    return false;

  CHECK(IsValidId(source_id_audio_));
  AdjustBufferTimestamps(
      buffers, source_info_map_[source_id_audio_].timestamp_offset);

  if (!audio_->Append(buffers))
    return false;

  IncreaseDurationIfNecessary(buffers, audio_);
  return true;
}

bool ChunkDemuxer::OnVideoBuffers(const StreamParser::BufferQueue& buffers) {
  lock_.AssertAcquired();
  DCHECK_NE(state_, SHUTDOWN);

  if (!video_)
    return false;

  CHECK(IsValidId(source_id_video_));
  AdjustBufferTimestamps(
      buffers, source_info_map_[source_id_video_].timestamp_offset);

  if (!video_->Append(buffers))
    return false;

  IncreaseDurationIfNecessary(buffers, video_);
  return true;
}

// TODO(acolwell): Remove bool from StreamParser::NeedKeyCB so that
// this method can be removed and need_key_cb_ can be passed directly
// to the parser.
bool ChunkDemuxer::OnNeedKey(scoped_array<uint8> init_data,
                             int init_data_size) {
  lock_.AssertAcquired();
  need_key_cb_.Run(init_data.Pass(), init_data_size);
  return true;
}

void ChunkDemuxer::OnNewMediaSegment(const std::string& source_id,
                                     TimeDelta timestamp) {
  DCHECK(timestamp != kNoTimestamp());
  DVLOG(2) << "OnNewMediaSegment(" << source_id << ", "
           << timestamp.InSecondsF() << ")";
  lock_.AssertAcquired();

  CHECK(IsValidId(source_id));
  source_info_map_[source_id].can_update_offset = false;
  base::TimeDelta start_timestamp =
      timestamp + source_info_map_[source_id].timestamp_offset;

  if (audio_ && source_id == source_id_audio_)
    audio_->OnNewMediaSegment(start_timestamp);
  if (video_ && source_id == source_id_video_)
    video_->OnNewMediaSegment(start_timestamp);
}

void ChunkDemuxer::OnEndOfMediaSegment(const std::string& source_id) {
  DVLOG(2) << "OnEndOfMediaSegment(" << source_id << ")";
  CHECK(IsValidId(source_id));
  source_info_map_[source_id].can_update_offset = true;
}

void ChunkDemuxer::AdjustBufferTimestamps(
    const StreamParser::BufferQueue& buffers,
    base::TimeDelta timestamp_offset) {
  if (timestamp_offset == base::TimeDelta())
    return;

  for (StreamParser::BufferQueue::const_iterator itr = buffers.begin();
       itr != buffers.end(); ++itr) {
    (*itr)->SetDecodeTimestamp(
        (*itr)->GetDecodeTimestamp() + timestamp_offset);
    (*itr)->SetTimestamp((*itr)->GetTimestamp() + timestamp_offset);
  }
}

bool ChunkDemuxer::IsValidId(const std::string& source_id) const {
  return source_info_map_.count(source_id) > 0u &&
      stream_parser_map_.count(source_id) > 0u;
}

void ChunkDemuxer::UpdateDuration(base::TimeDelta new_duration) {
  DCHECK(duration_ != new_duration);
  duration_ = new_duration;
  host_->SetDuration(new_duration);
}

void ChunkDemuxer::IncreaseDurationIfNecessary(
    const StreamParser::BufferQueue& buffers,
    const scoped_refptr<ChunkDemuxerStream>& stream) {
  DCHECK(!buffers.empty());
  if (buffers.back()->GetTimestamp() <= duration_)
    return;

  Ranges<TimeDelta> ranges = stream->GetBufferedRanges(kInfiniteDuration());
  DCHECK_GT(ranges.size(), 0u);

  base::TimeDelta last_timestamp_buffered = ranges.end(ranges.size() - 1);
  if (last_timestamp_buffered > duration_)
    UpdateDuration(last_timestamp_buffered);
}

void ChunkDemuxer::DecreaseDurationIfNecessary() {
  Ranges<TimeDelta> ranges = GetBufferedRanges();
  if (ranges.size() == 0u)
    return;

  base::TimeDelta last_timestamp_buffered = ranges.end(ranges.size() - 1);
  if (last_timestamp_buffered < duration_)
    UpdateDuration(last_timestamp_buffered);
}

Ranges<TimeDelta> ChunkDemuxer::GetBufferedRanges() const {
  if (audio_ && !video_)
    return audio_->GetBufferedRanges(duration_);
  else if (!audio_ && video_)
    return video_->GetBufferedRanges(duration_);
  return ComputeIntersection();
}

}  // namespace media
