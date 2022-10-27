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

#include "cobalt/media/progressive/progressive_demuxer.h"

#include <inttypes.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/media/base/bind_to_current_loop.h"
#include "cobalt/media/base/data_source.h"
#include "cobalt/media/base/starboard_utils.h"
#include "cobalt/media/base/timestamp_constants.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

ProgressiveDemuxerStream::ProgressiveDemuxerStream(ProgressiveDemuxer* demuxer,
                                                   Type type)
    : demuxer_(demuxer), type_(type) {
  TRACE_EVENT0("media_stack",
               "ProgressiveDemuxerStream::ProgressiveDemuxerStream()");
  DCHECK(demuxer_);
}

void ProgressiveDemuxerStream::Read(const ReadCB& read_cb) {
  TRACE_EVENT0("media_stack", "ProgressiveDemuxerStream::Read()");
  DCHECK(!read_cb.is_null());

  base::AutoLock auto_lock(lock_);

  // Don't accept any additional reads if we've been told to stop.
  // The demuxer_ may have been destroyed in the pipeline thread.
  if (stopped_) {
    TRACE_EVENT0("media_stack", "ProgressiveDemuxerStream::Read() EOS sent.");
    read_cb.Run(DemuxerStream::kOk,
                scoped_refptr<DecoderBuffer>(DecoderBuffer::CreateEOSBuffer()));
    return;
  }

  // Buffers are only queued when there are no pending reads.
  DCHECK(buffer_queue_.empty() || read_queue_.empty());

  if (!buffer_queue_.empty()) {
    // Send the oldest buffer back.
    scoped_refptr<DecoderBuffer> buffer = buffer_queue_.front();
    if (buffer->end_of_stream()) {
      TRACE_EVENT0("media_stack", "ProgressiveDemuxerStream::Read() EOS sent.");
    } else {
      // Do not pop EOS buffers, so that subsequent read requests also get EOS
      total_buffer_size_ -= buffer->data_size();
      --total_buffer_count_;
      buffer_queue_.pop_front();
    }
    read_cb.Run(DemuxerStream::kOk, buffer);
  } else {
    TRACE_EVENT0("media_stack",
                 "ProgressiveDemuxerStream::Read() request queued.");
    read_queue_.push_back(read_cb);
  }
}

AudioDecoderConfig ProgressiveDemuxerStream::audio_decoder_config() {
  return demuxer_->AudioConfig();
}

VideoDecoderConfig ProgressiveDemuxerStream::video_decoder_config() {
  return demuxer_->VideoConfig();
}

Ranges<base::TimeDelta> ProgressiveDemuxerStream::GetBufferedRanges() {
  base::AutoLock auto_lock(lock_);
  return buffered_ranges_;
}

DemuxerStream::Type ProgressiveDemuxerStream::type() const { return type_; }

void ProgressiveDemuxerStream::EnableBitstreamConverter() { NOTIMPLEMENTED(); }

void ProgressiveDemuxerStream::EnqueueBuffer(
    scoped_refptr<DecoderBuffer> buffer) {
  TRACE_EVENT1(
      "media_stack", "ProgressiveDemuxerStream::EnqueueBuffer()", "timestamp",
      buffer->end_of_stream() ? -1 : buffer->timestamp().InMicroseconds());
  base::AutoLock auto_lock(lock_);
  if (stopped_) {
    // it's possible due to pipelining both downstream and within the
    // demuxer that several pipelined reads will be enqueuing packets
    // on a stopped stream. Drop them after complaining.
    DLOG(WARNING) << "attempted to enqueue packet on stopped stream";
    return;
  }

  if (buffer->end_of_stream()) {
    TRACE_EVENT0("media_stack",
                 "ProgressiveDemuxerStream::EnqueueBuffer() EOS received.");
  } else if (buffer->timestamp() != kNoTimestamp) {
    if (last_buffer_timestamp_ != kNoTimestamp &&
        last_buffer_timestamp_ < buffer->timestamp()) {
      buffered_ranges_.Add(last_buffer_timestamp_, buffer->timestamp());
    }
    last_buffer_timestamp_ = buffer->timestamp();
  } else {
    DLOG(WARNING) << "bad timestamp info on enqueued buffer.";
  }

  // Check for any already waiting reads, service oldest read if there
  if (read_queue_.size()) {
    // assumption here is that buffer queue is empty
    DCHECK_EQ(buffer_queue_.size(), 0);
    ReadCB read_cb(read_queue_.front());
    read_queue_.pop_front();
    read_cb.Run(DemuxerStream::kOk, buffer);
  } else {
    // save the buffer for next read request
    buffer_queue_.push_back(buffer);
    if (!buffer->end_of_stream()) {
      total_buffer_size_ += buffer->data_size();
      ++total_buffer_count_;
    }
  }
}

base::TimeDelta ProgressiveDemuxerStream::GetLastBufferTimestamp() const {
  base::AutoLock auto_lock(lock_);
  return last_buffer_timestamp_;
}

size_t ProgressiveDemuxerStream::GetTotalBufferSize() const {
  base::AutoLock auto_lock(lock_);
  return total_buffer_size_;
}

size_t ProgressiveDemuxerStream::GetTotalBufferCount() const {
  base::AutoLock auto_lock(lock_);
  return total_buffer_count_;
}

void ProgressiveDemuxerStream::FlushBuffers() {
  TRACE_EVENT0("media_stack", "ProgressiveDemuxerStream::FlushBuffers()");
  base::AutoLock auto_lock(lock_);
  // TODO: Investigate if the following warning is valid.
  DLOG_IF(WARNING, !read_queue_.empty()) << "Read requests should be empty";
  buffer_queue_.clear();
  total_buffer_size_ = 0;
  total_buffer_count_ = 0;
  last_buffer_timestamp_ = kNoTimestamp;
}

void ProgressiveDemuxerStream::Stop() {
  TRACE_EVENT0("media_stack", "ProgressiveDemuxerStream::Stop()");
  DCHECK(demuxer_->MessageLoopBelongsToCurrentThread());
  base::AutoLock auto_lock(lock_);
  buffer_queue_.clear();
  total_buffer_size_ = 0;
  total_buffer_count_ = 0;
  last_buffer_timestamp_ = kNoTimestamp;
  // fulfill any pending callbacks with EOS buffers set to end timestamp
  for (ReadQueue::iterator it = read_queue_.begin(); it != read_queue_.end();
       ++it) {
    TRACE_EVENT0("media_stack", "ProgressiveDemuxerStream::Stop() EOS sent.");
    it->Run(DemuxerStream::kOk,
            scoped_refptr<DecoderBuffer>(DecoderBuffer::CreateEOSBuffer()));
  }
  read_queue_.clear();
  stopped_ = true;
}

//
// ProgressiveDemuxer
//
ProgressiveDemuxer::ProgressiveDemuxer(
    const scoped_refptr<base::SingleThreadTaskRunner>& message_loop,
    DecoderBuffer::Allocator* buffer_allocator, DataSource* data_source,
    const scoped_refptr<MediaLog>& media_log)
    : message_loop_(message_loop),
      buffer_allocator_(buffer_allocator),
      host_(NULL),
      blocking_thread_("ProgDemuxerBlk"),
      data_source_(data_source),
      media_log_(media_log),
      stopped_(false),
      flushing_(false),
      audio_reached_eos_(false),
      video_reached_eos_(false) {
  DCHECK(message_loop_);
  DCHECK(buffer_allocator_);
  DCHECK(data_source_);
  DCHECK(media_log_);
  reader_ = new DataSourceReader();
  reader_->SetDataSource(data_source_);
}

ProgressiveDemuxer::~ProgressiveDemuxer() {
  // Explicitly stop |blocking_thread_| to ensure that it stops before the
  // destructing of any other members.
  blocking_thread_.Stop();
}

void ProgressiveDemuxer::Initialize(DemuxerHost* host,
                                    const PipelineStatusCB& status_cb,
                                    bool enable_text_tracks) {
  TRACE_EVENT0("media_stack", "ProgressiveDemuxer::Initialize()");
  DCHECK(!enable_text_tracks);
  DCHECK(MessageLoopBelongsToCurrentThread());
  DCHECK(reader_);
  DCHECK(!parser_);

  DLOG(INFO) << "this is a PROGRESSIVE playback.";

  host_ = host;

  // create audio and video demuxer stream objects
  audio_demuxer_stream_.reset(
      new ProgressiveDemuxerStream(this, DemuxerStream::AUDIO));
  video_demuxer_stream_.reset(
      new ProgressiveDemuxerStream(this, DemuxerStream::VIDEO));

  // start the blocking thread and have it download and parse the media config
  if (!blocking_thread_.Start()) {
    status_cb.Run(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  blocking_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&ProgressiveDemuxer::ParseConfigBlocking,
                            base::Unretained(this), status_cb));
}

void ProgressiveDemuxer::ParseConfigBlocking(
    const PipelineStatusCB& status_cb) {
  DCHECK(blocking_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!parser_);

  // construct stream parser with error callback
  PipelineStatus status =
      ProgressiveParser::Construct(reader_, &parser_, media_log_);
  // if we can't construct a parser for this stream it's a fatal error, return
  // false so ParseConfigDone will notify the caller to Initialize() via
  // status_cb.
  if (!parser_ || status != PIPELINE_OK) {
    DCHECK(!parser_);
    DCHECK_NE(status, PIPELINE_OK);
    if (status == PIPELINE_OK) {
      status = DEMUXER_ERROR_COULD_NOT_PARSE;
    }
    ParseConfigDone(status_cb, status);
    return;
  }

  // instruct the parser to extract audio and video config from the file
  if (!parser_->ParseConfig()) {
    ParseConfigDone(status_cb, DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  // make sure we got a valid and complete configuration
  if (!parser_->IsConfigComplete()) {
    ParseConfigDone(status_cb, DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  // IsConfigComplete() should guarantee we know the duration
  DCHECK(parser_->Duration() != kInfiniteDuration);
  host_->SetDuration(parser_->Duration());
  // Bitrate may not be known, however
  uint32 bitrate = parser_->BitsPerSecond();
  if (bitrate > 0) {
    data_source_->SetBitrate(bitrate);
  }

  // successful parse of config data, inform the nonblocking demuxer thread
  DCHECK_EQ(status, PIPELINE_OK);
  ParseConfigDone(status_cb, PIPELINE_OK);
}

void ProgressiveDemuxer::ParseConfigDone(const PipelineStatusCB& status_cb,
                                         PipelineStatus status) {
  DCHECK(blocking_thread_.task_runner()->BelongsToCurrentThread());

  if (HasStopCalled()) {
    return;
  }

  // if the blocking parser thread cannot parse config we're done.
  if (status != PIPELINE_OK) {
    status_cb.Run(status);
    return;
  }
  DCHECK(parser_);
  // start downloading data
  Request(DemuxerStream::AUDIO);

  status_cb.Run(PIPELINE_OK);
}

void ProgressiveDemuxer::Request(DemuxerStream::Type type) {
  if (!blocking_thread_.task_runner()->BelongsToCurrentThread()) {
    blocking_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ProgressiveDemuxer::Request, base::Unretained(this), type));
    return;
  }

  DCHECK(!requested_au_) << "overlapping requests not supported!";
  flushing_ = false;
  // Ask parser for next AU
  scoped_refptr<AvcAccessUnit> au = parser_->GetNextAU(type);
  // fatal parsing error returns NULL or malformed AU
  if (!au || !au->IsValid()) {
    if (!HasStopCalled()) {
      DLOG(ERROR) << "got back bad AU from parser";
      host_->OnDemuxerError(DEMUXER_ERROR_COULD_NOT_PARSE);
    }
    return;
  }

  // make sure we got back an AU of the correct type
  DCHECK(au->GetType() == type);

  const char* ALLOW_UNUSED_TYPE event_type =
      type == DemuxerStream::AUDIO ? "audio" : "video";
  TRACE_EVENT2("media_stack", "ProgressiveDemuxer::RequestTask()", "type",
               event_type, "timestamp", au->GetTimestamp().InMicroseconds());

  // don't issue allocation requests for EOS AUs
  if (au->IsEndOfStream()) {
    TRACE_EVENT0("media_stack", "ProgressiveDemuxer::RequestTask() EOS sent");
    // enqueue EOS buffer with correct stream
    scoped_refptr<DecoderBuffer> eos_buffer = DecoderBuffer::CreateEOSBuffer();
    if (type == DemuxerStream::AUDIO) {
      audio_reached_eos_ = true;
      audio_demuxer_stream_->EnqueueBuffer(eos_buffer);
    } else if (type == DemuxerStream::VIDEO) {
      video_reached_eos_ = true;
      video_demuxer_stream_->EnqueueBuffer(eos_buffer);
    }
    IssueNextRequest();
    return;
  }

  // enqueue the request
  requested_au_ = au;

  AllocateBuffer();
}

void ProgressiveDemuxer::AllocateBuffer() {
  DCHECK(requested_au_);

  if (HasStopCalled()) {
    return;
  }

  if (requested_au_) {
    size_t total_buffer_size = audio_demuxer_stream_->GetTotalBufferSize() +
                               video_demuxer_stream_->GetTotalBufferSize();
    size_t total_buffer_count = audio_demuxer_stream_->GetTotalBufferCount() +
                                video_demuxer_stream_->GetTotalBufferCount();
    int progressive_budget = SbMediaGetProgressiveBufferBudget(
        MediaVideoCodecToSbMediaVideoCodec(VideoConfig().codec()),
        VideoConfig().visible_rect().size().width(),
        VideoConfig().visible_rect().size().height(),
        VideoConfig().webm_color_metadata().BitsPerChannel);
    int progressive_duration_cap_in_seconds =
        SbMediaGetBufferGarbageCollectionDurationThreshold() / kSbTimeSecond;
    const int kEstimatedBufferCountPerSeconds = 70;
    int progressive_buffer_count_cap =
        progressive_duration_cap_in_seconds * kEstimatedBufferCountPerSeconds;
    if (total_buffer_size >= progressive_budget ||
        total_buffer_count > progressive_buffer_count_cap) {
      // Retry after 100 milliseconds.
      const base::TimeDelta kDelay = base::TimeDelta::FromMilliseconds(100);
      blocking_thread_.message_loop()->task_runner()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&ProgressiveDemuxer::AllocateBuffer,
                     base::Unretained(this)),
          kDelay);
      return;
    }
    // Note that "new DecoderBuffer" may return NULL if it is unable to allocate
    // any DecoderBuffer.
    scoped_refptr<DecoderBuffer> decoder_buffer(
        DecoderBuffer::Create(buffer_allocator_, requested_au_->GetType(),
                              requested_au_->GetMaxSize()));
    if (decoder_buffer) {
      decoder_buffer->set_is_key_frame(requested_au_->IsKeyframe());
      buffer_allocator_->UpdateVideoConfig(VideoConfig());
      Download(decoder_buffer);
    } else {
      // As the buffer is full of media data, it is safe to delay 100
      // milliseconds.
      const base::TimeDelta kDelay = base::TimeDelta::FromMilliseconds(100);
      blocking_thread_.message_loop()->task_runner()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&ProgressiveDemuxer::AllocateBuffer,
                     base::Unretained(this)),
          kDelay);
    }
  }
}

void ProgressiveDemuxer::Download(scoped_refptr<DecoderBuffer> buffer) {
  DCHECK(blocking_thread_.task_runner()->BelongsToCurrentThread());
  // We need a requested_au_ or to have canceled this request and
  // are buffering to a new location for this to make sense
  DCHECK(requested_au_);

  const char* ALLOW_UNUSED_TYPE event_type =
      requested_au_->GetType() == DemuxerStream::AUDIO ? "audio" : "video";
  TRACE_EVENT2("media_stack", "ProgressiveDemuxer::Download()", "type",
               event_type, "timestamp",
               requested_au_->GetTimestamp().InMicroseconds());
  // do nothing if stopped
  if (HasStopCalled()) {
    DLOG(INFO) << "aborting download task, stopped";
    return;
  }

  // Flushing is a signal to restart the request->download cycle with
  // a new request. Drop current request and issue a new one.
  // flushing_ will be reset by the next call to RequestTask()
  if (flushing_) {
    DLOG(INFO) << "skipped AU download due to flush";
    requested_au_ = NULL;
    IssueNextRequest();
    return;
  }

  if (!requested_au_->Read(reader_.get(), buffer.get())) {
    DLOG(ERROR) << "au read failed";
    host_->OnDemuxerError(PIPELINE_ERROR_READ);
    return;
  }

  // copy timestamp and duration values
  buffer->set_timestamp(requested_au_->GetTimestamp());
  buffer->set_duration(requested_au_->GetDuration());

  // enqueue buffer into appropriate stream
  if (requested_au_->GetType() == DemuxerStream::AUDIO) {
    audio_demuxer_stream_->EnqueueBuffer(buffer);
  } else if (requested_au_->GetType() == DemuxerStream::VIDEO) {
    video_demuxer_stream_->EnqueueBuffer(buffer);
  } else {
    NOTREACHED() << "invalid buffer type enqueued";
  }

  // finished with this au, deref
  requested_au_ = NULL;

  // Calculate total range of buffered data for both audio and video.
  Ranges<base::TimeDelta> buffered(
      audio_demuxer_stream_->GetBufferedRanges().IntersectionWith(
          video_demuxer_stream_->GetBufferedRanges()));
  // Notify host of each disjoint range.
  host_->OnBufferedTimeRangesChanged(buffered);

  blocking_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&ProgressiveDemuxer::IssueNextRequest,
                            base::Unretained(this)));
}

void ProgressiveDemuxer::IssueNextRequest() {
  DCHECK(!requested_au_);
  // if we're stopped don't download anymore
  if (HasStopCalled()) {
    DLOG(INFO) << "stopped so request loop is stopping";
    return;
  }

  DemuxerStream::Type type = DemuxerStream::UNKNOWN;
  // if we have eos in one or both buffers the decision is easy
  if (audio_reached_eos_ || video_reached_eos_) {
    if (audio_reached_eos_) {
      if (video_reached_eos_) {
        // both are true, issue no more requests!
        DLOG(INFO) << "both streams at EOS, request loop stopping";
        return;
      } else {
        // audio is at eos, video isn't, get more video
        type = DemuxerStream::VIDEO;
      }
    } else {
      // audio is not at eos, video is, get more audio
      type = DemuxerStream::AUDIO;
    }
  } else {
    // priority order for figuring out what to download next
    base::TimeDelta audio_stamp =
        audio_demuxer_stream_->GetLastBufferTimestamp();
    base::TimeDelta video_stamp =
        video_demuxer_stream_->GetLastBufferTimestamp();
    // if the audio demuxer stream is empty, always fill it first
    if (audio_stamp == kNoTimestamp) {
      type = DemuxerStream::AUDIO;
    } else if (video_stamp == kNoTimestamp) {
      // the video demuxer stream is empty, we need data for it
      type = DemuxerStream::VIDEO;
    } else if (video_stamp < audio_stamp) {
      // video is earlier, fill it first
      type = DemuxerStream::VIDEO;
    } else {
      type = DemuxerStream::AUDIO;
    }
  }
  DCHECK_NE(type, DemuxerStream::UNKNOWN);
  // We cannot call Request() directly even if this function is also run on
  // |blocking_thread_| as otherwise it is possible that this function is
  // running in a tight loop and seek or stop request has no chance to kick in.
  blocking_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ProgressiveDemuxer::Request, base::Unretained(this), type));
}

void ProgressiveDemuxer::Stop() {
  DCHECK(MessageLoopBelongsToCurrentThread());
  // set our internal stop flag, to not treat read failures as
  // errors anymore but as a natural part of stopping
  {
    base::AutoLock auto_lock(lock_for_stopped_);
    stopped_ = true;
  }

  // stop the reader, which will stop the datasource and call back
  reader_->Stop();
}

void ProgressiveDemuxer::DataSourceStopped(const base::Closure& callback) {
  TRACE_EVENT0("media_stack", "ProgressiveDemuxer::DataSourceStopped()");
  DCHECK(MessageLoopBelongsToCurrentThread());
  // stop the download thread
  blocking_thread_.Stop();

  // tell downstream we've stopped
  if (audio_demuxer_stream_) audio_demuxer_stream_->Stop();
  if (video_demuxer_stream_) video_demuxer_stream_->Stop();

  callback.Run();
}

bool ProgressiveDemuxer::HasStopCalled() {
  base::AutoLock auto_lock(lock_for_stopped_);
  return stopped_;
}

void ProgressiveDemuxer::Seek(base::TimeDelta time,
                              const PipelineStatusCB& cb) {
  blocking_thread_.message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ProgressiveDemuxer::SeekTask, base::Unretained(this), time,
                 BindToCurrentLoop(cb)));
}

// runs on blocking thread
void ProgressiveDemuxer::SeekTask(base::TimeDelta time,
                                  const PipelineStatusCB& cb) {
  TRACE_EVENT1("media_stack", "ProgressiveDemuxer::SeekTask()", "timestamp",
               time.InMicroseconds());
  DLOG(INFO) << base::StringPrintf("seek to: %" PRId64 " ms",
                                   time.InMilliseconds());
  // clear any enqueued buffers on demuxer streams
  audio_demuxer_stream_->FlushBuffers();
  video_demuxer_stream_->FlushBuffers();
  // advance parser to new timestamp
  if (!parser_->SeekTo(time)) {
    DLOG(ERROR) << "parser seek failed.";
    cb.Run(PIPELINE_ERROR_READ);
    return;
  }
  // if both streams had finished downloading, we need to restart the request
  bool issue_new_request = audio_reached_eos_ && video_reached_eos_;
  audio_reached_eos_ = false;
  video_reached_eos_ = false;
  flushing_ = true;
  cb.Run(PIPELINE_OK);
  if (issue_new_request) {
    DLOG(INFO) << "restarting stopped request loop";
    Request(DemuxerStream::AUDIO);
  }
}

DemuxerStream* ProgressiveDemuxer::GetStream(media::DemuxerStream::Type type) {
  if (type == DemuxerStream::AUDIO) {
    return audio_demuxer_stream_.get();
  } else if (type == DemuxerStream::VIDEO) {
    return video_demuxer_stream_.get();
  } else {
    DLOG(WARNING) << "unsupported stream type requested";
  }
  return NULL;
}

base::TimeDelta ProgressiveDemuxer::GetStartTime() const {
  // we always assume a start time of 0
  return base::TimeDelta();
}

const AudioDecoderConfig& ProgressiveDemuxer::AudioConfig() {
  return parser_->AudioConfig();
}

const VideoDecoderConfig& ProgressiveDemuxer::VideoConfig() {
  return parser_->VideoConfig();
}

bool ProgressiveDemuxer::MessageLoopBelongsToCurrentThread() const {
  return message_loop_->BelongsToCurrentThread();
}

}  // namespace media
}  // namespace cobalt
