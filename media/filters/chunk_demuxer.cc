// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/chunk_demuxer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/chunk_demuxer_client.h"
#if defined(USE_PROPRIETARY_CODECS)
#include "media/mp4/mp4_stream_parser.h"
#endif
#include "media/webm/webm_stream_parser.h"

using base::TimeDelta;

namespace media {

struct CodecInfo {
  const char* pattern;
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

#if defined(USE_PROPRIETARY_CODECS)
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

static StreamParser* BuildMP4Parser() {
  return new mp4::MP4StreamParser();
}
#endif

static const SupportedTypeInfo kSupportedTypeInfo[] = {
  { "video/webm", &BuildWebMParser, kVideoWebMCodecs },
  { "audio/webm", &BuildWebMParser, kAudioWebMCodecs },
#if defined(USE_PROPRIETARY_CODECS)
  { "video/mp4", &BuildMP4Parser, kVideoMP4Codecs },
  { "audio/mp4", &BuildMP4Parser, kAudioMP4Codecs },
#endif
};


// The fake total size we use for converting times to bytes
// for AddBufferedByteRange() calls.
// TODO(acolwell): Remove this once Pipeline accepts buffered times
// instead of only buffered bytes.
enum { kFakeTotalBytes = 1000000 };

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
  bool IsSeekPending() const;
  void Flush();

  // Add buffers to this stream.  Buffers are stored in SourceBufferStreams,
  // which handle ordering and overlap resolution.
  // Returns true if buffers were successfully added.
  bool Append(const StreamParser::BufferQueue& buffers);

  // Returns a list of the buffered time ranges.
  Ranges<TimeDelta> GetBufferedTime() const;

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

  scoped_ptr<SourceBufferStream> stream_;

  mutable base::Lock lock_;
  State state_;
  ReadCBQueue read_cbs_;

  // The timestamp of the current media segment being parsed by
  // |stream_parser_|.
  TimeDelta media_segment_start_time_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChunkDemuxerStream);
};

ChunkDemuxerStream::ChunkDemuxerStream(const AudioDecoderConfig& audio_config)
    : type_(AUDIO),
      state_(RETURNING_DATA_FOR_READS),
      media_segment_start_time_(kNoTimestamp()) {
  stream_.reset(new SourceBufferStream(audio_config));
}

ChunkDemuxerStream::ChunkDemuxerStream(const VideoDecoderConfig& video_config)
    : type_(VIDEO),
      state_(RETURNING_DATA_FOR_READS) {
  stream_.reset(new SourceBufferStream(video_config));
}

void ChunkDemuxerStream::StartWaitingForSeek() {
  DVLOG(1) << "StartWaitingForSeek()";
  ReadCBQueue read_cbs;
  {
    base::AutoLock auto_lock(lock_);
    ChangeState_Locked(WAITING_FOR_SEEK);

    std::swap(read_cbs_, read_cbs);
  }

  for (ReadCBQueue::iterator it = read_cbs.begin(); it != read_cbs.end(); ++it)
    it->Run(NULL);
}

void ChunkDemuxerStream::Seek(TimeDelta time) {
  base::AutoLock auto_lock(lock_);

  DCHECK(read_cbs_.empty());

  stream_->Seek(time);

  if (state_ == WAITING_FOR_SEEK)
    ChangeState_Locked(RETURNING_DATA_FOR_READS);
}

bool ChunkDemuxerStream::IsSeekPending() const {
  base::AutoLock auto_lock(lock_);
  return stream_->IsSeekPending();
}

void ChunkDemuxerStream::Flush() {
  media_segment_start_time_ = kNoTimestamp();
}

void ChunkDemuxerStream::OnNewMediaSegment(TimeDelta start_timestamp) {
  media_segment_start_time_ = start_timestamp;
}

bool ChunkDemuxerStream::Append(const StreamParser::BufferQueue& buffers) {
  if (buffers.empty())
    return false;

  ClosureQueue closures;
  {
    base::AutoLock auto_lock(lock_);
    DCHECK_NE(state_, SHUTDOWN);
    DCHECK(media_segment_start_time_ != kNoTimestamp());
    if (!stream_->Append(buffers, media_segment_start_time_)) {
      DVLOG(1) << "ChunkDemuxerStream::Append() : stream append failed";
      return false;
    }
    CreateReadDoneClosures_Locked(&closures);
  }

  for (ClosureQueue::iterator it = closures.begin(); it != closures.end(); ++it)
    it->Run();

  return true;
}

Ranges<TimeDelta> ChunkDemuxerStream::GetBufferedTime() const {
  base::AutoLock auto_lock(lock_);
  return stream_->GetBufferedTime();
}

bool ChunkDemuxerStream::UpdateAudioConfig(const AudioDecoderConfig& config) {
  DCHECK(config.IsValidConfig());
  DCHECK_EQ(type_, AUDIO);

  const AudioDecoderConfig& current_config =
      stream_->GetCurrentAudioDecoderConfig();

  bool success = (current_config.codec() == config.codec()) &&
      (current_config.bits_per_channel() == config.bits_per_channel()) &&
      (current_config.channel_layout() == config.channel_layout()) &&
      (current_config.samples_per_second() == config.samples_per_second()) &&
      (current_config.extra_data_size() == config.extra_data_size()) &&
      (!current_config.extra_data() ||
       !memcmp(current_config.extra_data(), config.extra_data(),
               current_config.extra_data_size()));

  if (!success)
    DVLOG(1) << "UpdateAudioConfig() : Failed to update audio config.";

  return success;
}

bool ChunkDemuxerStream::UpdateVideoConfig(const VideoDecoderConfig& config) {
  DCHECK(config.IsValidConfig());
  DCHECK_EQ(type_, VIDEO);
  const VideoDecoderConfig& current_config =
      stream_->GetCurrentVideoDecoderConfig();

  bool success = (current_config.codec() == config.codec()) &&
      (current_config.format() == config.format()) &&
      (current_config.profile() == config.profile()) &&
      (current_config.coded_size() == config.coded_size()) &&
      (current_config.visible_rect() == config.visible_rect()) &&
      (current_config.natural_size() == config.natural_size()) &&
      (current_config.extra_data_size() == config.extra_data_size()) &&
      (!current_config.extra_data() ||
       !memcmp(current_config.extra_data(), config.extra_data(),
               current_config.extra_data_size()));

  if (!success)
    DVLOG(1) << "UpdateVideoConfig() : Failed to update video config.";

  return success;
}

void ChunkDemuxerStream::EndOfStream() {
  ClosureQueue closures;
  {
    base::AutoLock auto_lock(lock_);
    stream_->EndOfStream();
    CreateReadDoneClosures_Locked(&closures);
  }

  for (ClosureQueue::iterator it = closures.begin(); it != closures.end(); ++it)
    it->Run();
}

bool ChunkDemuxerStream::CanEndOfStream() const {
  base::AutoLock auto_lock(lock_);
  return stream_->CanEndOfStream();
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
                             const scoped_refptr<DecoderBuffer>& buffer) {
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
        // If we already have pending reads or we don't have any buffers ready,
        // then defer this read.
        if (!read_cbs_.empty() || !stream_->GetNextBuffer(&buffer)) {
          DeferRead_Locked(read_cb);
          return;
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
    if (!stream_->GetNextBuffer(&buffer))
      return;
    closures->push_back(base::Bind(read_cbs_.front(), buffer));
    read_cbs_.pop_front();
  }
}

ChunkDemuxer::ChunkDemuxer(ChunkDemuxerClient* client)
    : state_(WAITING_FOR_INIT),
      host_(NULL),
      client_(client) {
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

void ChunkDemuxer::Seek(TimeDelta time, const PipelineStatusCB& cb) {
  DVLOG(1) << "Seek(" << time.InSecondsF() << ")";

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

TimeDelta ChunkDemuxer::GetStartTime() const {
  DVLOG(1) << "GetStartTime()";
  // TODO(acolwell) : Fix this so it uses the time on the first packet.
  // (crbug.com/132815)
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

ChunkDemuxer::Status ChunkDemuxer::AddId(const std::string& id,
                                         const std::string& type,
                                         std::vector<std::string>& codecs) {
  DCHECK_GT(codecs.size(), 0u);
  base::AutoLock auto_lock(lock_);

  if (state_ != WAITING_FOR_INIT && state_ != INITIALIZING)
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

  scoped_ptr<StreamParser> stream_parser(factory_function());
  CHECK(stream_parser.get());

  stream_parser->Init(
      base::Bind(&ChunkDemuxer::OnStreamParserInitDone, this),
      base::Bind(&ChunkDemuxer::OnNewConfigs, base::Unretained(this),
                 has_audio, has_video),
      audio_cb,
      video_cb,
      base::Bind(&ChunkDemuxer::OnNeedKey, base::Unretained(this)),
      base::Bind(&ChunkDemuxer::OnNewMediaSegment, base::Unretained(this), id));

  stream_parser_map_[id] = stream_parser.release();

  return kOk;
}

void ChunkDemuxer::RemoveId(const std::string& id) {
  CHECK_GT(stream_parser_map_.count(id), 0u);
  base::AutoLock auto_lock(lock_);

  delete stream_parser_map_[id];
  stream_parser_map_.erase(id);

  if (source_id_audio_ == id && audio_)
    audio_->Shutdown();

  if (source_id_video_ == id && video_)
    video_->Shutdown();
}

Ranges<TimeDelta> ChunkDemuxer::GetBufferedRanges(const std::string& id) const {
  DCHECK(!id.empty());
  DCHECK_GT(stream_parser_map_.count(id), 0u);
  DCHECK(id == source_id_audio_ || id == source_id_video_);

  base::AutoLock auto_lock(lock_);

  if (id == source_id_audio_ && id != source_id_video_) {
    // Only include ranges that have been buffered in |audio_|
    return audio_ ? audio_->GetBufferedTime() : Ranges<TimeDelta>();
  }

  if (id != source_id_audio_ && id == source_id_video_) {
    // Only include ranges that have been buffered in |video_|
    return video_ ? video_->GetBufferedTime() : Ranges<TimeDelta>();
  }

  return ComputeIntersection();
}

Ranges<TimeDelta> ChunkDemuxer::ComputeIntersection() const {
  lock_.AssertAcquired();

  if (!audio_ || !video_)
    return Ranges<TimeDelta>();

  // Include ranges that have been buffered in both |audio_| and |video_|.
  Ranges<TimeDelta> audio_ranges = audio_->GetBufferedTime();
  Ranges<TimeDelta> video_ranges = video_->GetBufferedTime();
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
        DCHECK_GT(stream_parser_map_.count(id), 0u);
        if (!stream_parser_map_[id]->Parse(data, length)) {
          DCHECK_EQ(state_, INITIALIZING);
          ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
          return true;
        }
        break;

      case INITIALIZED: {
        DCHECK_GT(stream_parser_map_.count(id), 0u);
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

    if (duration_ > TimeDelta() && duration_ != kInfiniteDuration()) {
      if (audio_ && !video_) {
        ranges = audio_->GetBufferedTime();
      } else if (!audio_ && video_) {
        ranges = video_->GetBufferedTime();
      } else {
        ranges = ComputeIntersection();
      }
    }
  }

  DCHECK(!ranges.size() || duration_ > TimeDelta());
  for (size_t i = 0; i < ranges.size(); ++i) {
    // Notify the host of 'network activity' because we got data.
    int64 start =
        kFakeTotalBytes * ranges.start(i).InSecondsF() / duration_.InSecondsF();
    int64 end =
        kFakeTotalBytes * ranges.end(i).InSecondsF() / duration_.InSecondsF();
    host_->AddBufferedByteRange(start, end);
  }

  if (!cb.is_null())
    cb.Run(PIPELINE_OK);

  return true;
}

void ChunkDemuxer::Abort(const std::string& id) {
  DVLOG(1) << "Abort(" << id << ")";
  DCHECK(!id.empty());
  DCHECK_GT(stream_parser_map_.count(id), 0u);

  stream_parser_map_[id]->Flush();
  if (audio_ && source_id_audio_ == id)
    audio_->Flush();
  if (video_ && source_id_video_ == id)
    video_->Flush();
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

  if (status != PIPELINE_OK)
    ReportError_Locked(status);
  else
    ChangeState_Locked(ENDED);

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

    for (StreamParserMap::iterator it = stream_parser_map_.begin();
         it != stream_parser_map_.end(); ++it) {
      delete it->second;
    }
    stream_parser_map_.clear();

    ChangeState_Locked(SHUTDOWN);
  }

  if (!cb.is_null())
    cb.Run(PIPELINE_ERROR_ABORT);

  client_->DemuxerClosed();
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
  DVLOG(1) << "OnSourceBufferInitDone(" << success << ", "
           << duration.InSecondsF() << ")";
  lock_.AssertAcquired();
  DCHECK_EQ(state_, INITIALIZING);
  if (!success || (!audio_ && !video_)) {
    ReportError_Locked(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  if (duration > duration_)
    duration_ = duration;

  // Wait until all streams have initialized.
  if ((!source_id_audio_.empty() && !audio_) ||
      (!source_id_video_.empty() && !video_))
    return;

  if (duration_ > TimeDelta() && duration_ != kInfiniteDuration())
    host_->SetTotalBytes(kFakeTotalBytes);
  host_->SetDuration(duration_);

  ChangeState_Locked(INITIALIZED);
  PipelineStatusCB cb;
  std::swap(cb, init_cb_);
  cb.Run(PIPELINE_OK);
}

bool ChunkDemuxer::OnNewConfigs(bool has_audio, bool has_video,
                                const AudioDecoderConfig& audio_config,
                                const VideoDecoderConfig& video_config) {
  DVLOG(1) << "OnNewConfigs(" << has_audio << ", " << has_video
           << ", " << audio_config.IsValidConfig()
           << ", " << video_config.IsValidConfig() << ")";
  CHECK(audio_config.IsValidConfig() || video_config.IsValidConfig());
  lock_.AssertAcquired();

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
  DCHECK_NE(state_, SHUTDOWN);

  if (!audio_)
    return false;

  return audio_->Append(buffers);
}

bool ChunkDemuxer::OnVideoBuffers(const StreamParser::BufferQueue& buffers) {
  DCHECK_NE(state_, SHUTDOWN);

  if (!video_)
    return false;

  return video_->Append(buffers);
}

bool ChunkDemuxer::OnNeedKey(scoped_array<uint8> init_data,
                             int init_data_size) {
  client_->DemuxerNeedKey(init_data.Pass(), init_data_size);
  return true;
}

void ChunkDemuxer::OnNewMediaSegment(const std::string& source_id,
                                     TimeDelta start_timestamp) {
  // TODO(vrk): There should be a special case for the first appends where all
  // streams (for both demuxed and muxed case) begin at the earliest stream
  // timestamp. (crbug.com/132815)
  if (audio_ && source_id == source_id_audio_)
    audio_->OnNewMediaSegment(start_timestamp);
  if (video_ && source_id == source_id_video_)
    video_->OnNewMediaSegment(start_timestamp);
}

}  // namespace media
