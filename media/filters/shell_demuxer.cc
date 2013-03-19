/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "media/filters/shell_demuxer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/time.h"
#include "media/base/bind_to_loop.h"
#include "media/base/data_source.h"
#include "media/base/shell_filter_graph_log_constants.h"

#include <inttypes.h>

namespace media {

static const uint64 kDemuxerPreloadTimeMilliseconds = 5000;

ShellDemuxerStream::ShellDemuxerStream(ShellDemuxer* demuxer,
                                       Type type)
    : demuxer_(demuxer)
    , type_(type)
    , stopped_(false)
    , buffering_(true) {
  DCHECK(demuxer_);
  filter_graph_log_ = demuxer->filter_graph_log();
  filter_graph_log_->LogDemuxerStreamEvent(type_, kEventConstructor);
}

scoped_refptr<ShellFilterGraphLog> ShellDemuxerStream::filter_graph_log() {
  return filter_graph_log_;
}

void ShellDemuxerStream::Read(const ReadCB& read_cb) {
  DCHECK(!read_cb.is_null());
  filter_graph_log_->LogDemuxerStreamEvent(type_, kEventRead);

  base::AutoLock auto_lock(lock_);

  // Don't accept any additional reads if we've been told to stop.
  // The demuxer_ may have been destroyed in the pipleine thread.
  if (stopped_) {
    filter_graph_log_->LogDemuxerStreamEvent(type_, kEventEndOfStreamSent);
    read_cb.Run(DemuxerStream::kOk,
                scoped_refptr<ShellBuffer>(
                    ShellBuffer::CreateEOSBuffer(kNoTimestamp(),
                                                 filter_graph_log_)));
    return;
  }

  // Buffers are only queued when there are no pending reads.
  DCHECK(buffer_queue_.empty() || read_queue_.empty());

  if (!buffering_ && !buffer_queue_.empty()) {
    // Send the oldest buffer back.
    scoped_refptr<ShellBuffer> buffer = buffer_queue_.front();
    buffer_queue_.pop_front();
    RebuildEnqueuedRanges_Locked();
    if (buffer->IsEndOfStream()) {
      filter_graph_log_->LogDemuxerStreamEvent(type_, kEventEndOfStreamSent);
    }
    read_cb.Run(DemuxerStream::kOk, buffer);
  } else {
    // Ask the demuxer to issue a request for data of our type next,
    // giving this priority over its normal latency logic.
    filter_graph_log_->LogDemuxerStreamEvent(type_, kEventRequestInterrupt);
    read_queue_.push_back(read_cb);
  }
  filter_graph_log_->LogDemuxerStreamStateQueues(
      type_, buffer_queue_.size(), read_queue_.size());
}

const AudioDecoderConfig& ShellDemuxerStream::audio_decoder_config() {
  return demuxer_->AudioConfig();
}

const VideoDecoderConfig& ShellDemuxerStream::video_decoder_config() {
  return demuxer_->VideoConfig();
}

Ranges<base::TimeDelta> ShellDemuxerStream::GetBufferedRanges() {
  base::AutoLock auto_lock(lock_);
  return buffered_ranges_;
}

DemuxerStream::Type ShellDemuxerStream::type() {
  return type_;
}

void ShellDemuxerStream::EnableBitstreamConverter() {
  NOTIMPLEMENTED();
}

void ShellDemuxerStream::EnqueueBuffer(scoped_refptr<ShellBuffer> buffer) {
  filter_graph_log_->LogDemuxerStreamEvent(type_, kEventEnqueue);
  base::AutoLock auto_lock(lock_);
  if (stopped_) {
    // it's possible due to pipelining both downstream and within the
    // demuxer that several pipelined reads will be enqueuing packets
    // on a stopped stream. Drop them after complaining.
    DLOG(WARNING) << "attempted to enqueue packet on stopped stream";
    return;
  }

  bool range_valid = true;
  if (buffer->IsEndOfStream()) {
    filter_graph_log_->LogDemuxerStreamEvent(type_,
                                             kEventEndOfStreamReceived);
    range_valid = false;
  } else {
    if (buffer->GetTimestamp() != kNoTimestamp() &&
        buffer->GetDuration() != kInfiniteDuration()) {
      buffered_ranges_.Add(buffer->GetTimestamp(),
                           buffer->GetTimestamp() + buffer->GetDuration());
    } else {
      DLOG(WARNING) << "bad timestamp or duration info on enqueued buffer.";
      range_valid = false;
    }
  }

  // Check for any already waiting reads, service oldest read if there
  if (!buffering_ && read_queue_.size()) {
    // assumption here is that buffer queue is empty
    DCHECK_EQ(buffer_queue_.size(), 0);
    ReadCB read_cb(read_queue_.front());
    read_queue_.pop_front();
    read_cb.Run(DemuxerStream::kOk, buffer);
  } else {
    // save the buffer for next read request
    buffer_queue_.push_back(buffer);
    if (range_valid) {
      enqueued_ranges_.Add(buffer->GetTimestamp(),
                           buffer->GetTimestamp() + buffer->GetDuration());
    }
  }
  filter_graph_log_->LogDemuxerStreamStateQueues(
      type_, buffer_queue_.size(), read_queue_.size());
}

void ShellDemuxerStream::RebuildEnqueuedRanges_Locked() {
  lock_.AssertAcquired();
  enqueued_ranges_.clear();
  for (BufferQueue::iterator it = buffer_queue_.begin();
       it != buffer_queue_.end();
       it++) {
    if ((*it)->GetTimestamp() != kNoTimestamp() &&
        (*it)->GetDuration() != kInfiniteDuration()) {
      enqueued_ranges_.Add((*it)->GetTimestamp(),
                           (*it)->GetTimestamp() + (*it)->GetDuration());
    }
  }
}

base::TimeDelta ShellDemuxerStream::GetEndOfContiguousEnqueuedRange() {
  base::AutoLock auto_lock(lock_);
  if (enqueued_ranges_.size()) {
    return enqueued_ranges_.end(0);
  }
  return kNoTimestamp();
}

void ShellDemuxerStream::SetBuffering(bool buffering) {
  filter_graph_log_->LogDemuxerStreamStateBuffering(type_, buffering);
  base::AutoLock auto_lock(lock_);
  // if transitioning from buffering to not, service any pending
  // reads we have accumulated
  if (buffering_ && !buffering) {
    while (buffer_queue_.size() && read_queue_.size()) {
      scoped_refptr<ShellBuffer> buffer = buffer_queue_.front();
      buffer_queue_.pop_front();
      ReadCB read_cb(read_queue_.front());
      read_queue_.pop_front();
      read_cb.Run(DemuxerStream::kOk, buffer);
    }
    RebuildEnqueuedRanges_Locked();
  } else if (!buffering_ && buffering) {
    // if we're turning on buffering, we shouldn't have any pending reads
    DCHECK(read_queue_.empty()) <<
        "attempting to turn on buffering with pending reads!";
  }
  buffering_ = buffering;
  filter_graph_log_->LogDemuxerStreamStateQueues(
      type_, buffer_queue_.size(), read_queue_.size());
}

void ShellDemuxerStream::FlushBuffers() {
  filter_graph_log_->LogDemuxerStreamEvent(type_, kEventFlush);
  base::AutoLock auto_lock(lock_);
  DCHECK(read_queue_.empty()) << "Read requests should be empty";
  buffer_queue_.clear();
  enqueued_ranges_.clear();
  filter_graph_log_->LogDemuxerStreamStateQueues(
      type_, buffer_queue_.size(), read_queue_.size());
}

void ShellDemuxerStream::Stop() {
  DCHECK(demuxer_->MessageLoopBelongsToCurrentThread());
  filter_graph_log_->LogDemuxerStreamEvent(type_, kEventStop);
  base::AutoLock auto_lock(lock_);
  buffer_queue_.clear();
  enqueued_ranges_.clear();
  // fulfill any pending callbacks with EOS buffers set to end timestamp
  for (ReadQueue::iterator it = read_queue_.begin();
       it != read_queue_.end(); ++it) {
    filter_graph_log_->LogDemuxerStreamEvent(type_, kEventEndOfStreamSent);
    it->Run(DemuxerStream::kOk,
            scoped_refptr<ShellBuffer>(
                ShellBuffer::CreateEOSBuffer(kNoTimestamp(),
                                             filter_graph_log_)));
  }
  read_queue_.clear();
  stopped_ = true;
  filter_graph_log_->LogDemuxerStreamStateQueues(
      type_, buffer_queue_.size(), read_queue_.size());
}

//
// ShellDemuxer
//
ShellDemuxer::ShellDemuxer(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const scoped_refptr<DataSource>& data_source)
    : message_loop_(message_loop)
    , host_(NULL)
    , blocking_thread_("ShellDemuxerBlockingThread")
    , data_source_(data_source)
    , read_has_failed_(false)
    , stopped_(false)
    , flushing_(false)
    , audio_reached_eos_(false)
    , video_reached_eos_(false) {
  DCHECK(data_source_);
  DCHECK(message_loop_);
  buffer_timestamp_ =
      base::TimeDelta::FromMilliseconds(kDemuxerPreloadTimeMilliseconds);
  reader_ = new ShellDataSourceReader();
  reader_->SetDataSource(data_source_);
}

ShellDemuxer::~ShellDemuxer() {
  NOTIMPLEMENTED();
}

void ShellDemuxer::Initialize(DemuxerHost* host,
                              const PipelineStatusCB& status_cb) {
  DCHECK(filter_graph_log_);
  message_loop_->PostTask(FROM_HERE, base::Bind(&ShellDemuxer::InitializeTask,
                                                this,
                                                host,
                                                status_cb));
}

void ShellDemuxer::InitializeTask(DemuxerHost *host,
                                  const PipelineStatusCB &status_cb) {
  DCHECK(MessageLoopBelongsToCurrentThread());
  DCHECK(reader_);
  filter_graph_log_->LogEvent(kObjectIdDemuxer, kEventInitialize);

  host_ = host;
  data_source_->set_host(host);

  // construct stream parser with error callback
  parser_ = ShellParser::Construct(reader_, status_cb, filter_graph_log_);
  // if we can't construct a parser for this stream it's a fatal error, return
  if (!parser_) {
    status_cb.Run(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  // create audio and video demuxer stream objects
  audio_demuxer_stream_ = new ShellDemuxerStream(this, DemuxerStream::AUDIO);
  video_demuxer_stream_ = new ShellDemuxerStream(this, DemuxerStream::VIDEO);

  // start the blocking thread and have it download and parse the media config
  if (!blocking_thread_.Start()) {
    status_cb.Run(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  base::PostTaskAndReplyWithResult(
      blocking_thread_.message_loop_proxy(), FROM_HERE,
      base::Bind(&ShellDemuxer::ParseConfigBlocking, this),
      base::Bind(&ShellDemuxer::ParseConfigDone, this, status_cb));
}

bool ShellDemuxer::ParseConfigBlocking() {
  // instruct the parser to extract audio and video config from the file
  if (!parser_->ParseConfig()) {
    return false;
  }

  // make sure we got a valid and complete configuration
  if (!parser_->IsConfigComplete()) {
    return false;
  }

  // IsConfigComplete() should guarantee we know the duration
  DCHECK(parser_->Duration() != kInfiniteDuration());
  host_->SetDuration(parser_->Duration());
  // Bitrate may not be known, however
  uint32 bitrate = parser_->BitsPerSecond();
  if (bitrate > 0) {
    data_source_->SetBitrate(bitrate);
  }

  // successful parse of config data, inform the nonblocking demuxer thread
  return true;
}

void ShellDemuxer::ParseConfigDone(const PipelineStatusCB& status_cb,
                                   bool result) {
  DCHECK(MessageLoopBelongsToCurrentThread());
  // if the blocking parser thread cannot parse config we're done.
  if (!result) {
    status_cb.Run(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  // start buffering data
  SetBuffering(true);
  Request(DemuxerStream::AUDIO);

  status_cb.Run(PIPELINE_OK);
}

void ShellDemuxer::Request(DemuxerStream::Type type) {
  // post task to our blocking thread
  blocking_thread_.message_loop_proxy()->PostTask(FROM_HERE,
      base::Bind(&ShellDemuxer::RequestTask, this, type));
}

void ShellDemuxer::RequestTask(DemuxerStream::Type type) {
  DCHECK(!requested_au_) << "overlapping requests not supported!";
  filter_graph_log_->LogDemuxerRequestEvent(type);
  flushing_ = false;
  // Ask parser for next AU
  scoped_refptr<ShellAU> au = parser_->GetNextAU(type);
  // fatal parsing error returns NULL or malformed AU
  if (!au || !au->IsValid()) {
    if (!stopped_) {
      DLOG(ERROR) << "got back bad AU from parser";
      host_->OnDemuxerError(DEMUXER_ERROR_COULD_NOT_PARSE);
    }
    return;
  }

  // make sure we got back an AU of the correct type
  DCHECK(au->GetType() == type);

  // don't issue allocation requests for EOS AUs
  if (au->IsEndOfStream()) {
    filter_graph_log_->LogEvent(kObjectIdDemuxer, kEventEndOfStreamSent);
    // enqueue EOS buffer with correct stream
    scoped_refptr<ShellBuffer> eos_buffer =
        ShellBuffer::CreateEOSBuffer(au->GetTimestamp(),
                                     filter_graph_log_);
    if (type == DemuxerStream::AUDIO) {
      audio_reached_eos_ = true;
      audio_demuxer_stream_->EnqueueBuffer(eos_buffer);
    } else if (type == DemuxerStream::VIDEO) {
      video_reached_eos_ = true;
      video_demuxer_stream_->EnqueueBuffer(eos_buffer);
    }
    IssueNextRequestTask();
    return;
  }

  // If we're buffering and we've filled the demux buffer then we can
  // signal that the buffering has completed
  if (buffering_ &&
      (!ShellBufferFactory::Instance()->HasRoomForBufferNow(
          au->GetMaxSize()))) {
    DLOG(INFO) << "demuxer filled buffer, finished buffering";
    SetBuffering(false);
  }

  // enqueue the request
  requested_au_ = au;

  // AllocateBuffer will return false if the requested size is larger
  // than the maximum limit for a single buffer.
  if (!ShellBufferFactory::Instance()->AllocateBuffer(
      au->GetMaxSize(),
      base::Bind(&ShellDemuxer::BufferAllocated, this),
      filter_graph_log_)) {
    host_->OnDemuxerError(PIPELINE_ERROR_COULD_NOT_RENDER);
    return;
  }
}

// callback from ShellBufferAllocated, post a task to the blocking thread
void ShellDemuxer::BufferAllocated(scoped_refptr<ShellBuffer> buffer) {
  if (!stopped_) {
    blocking_thread_.message_loop_proxy()->PostTask(FROM_HERE,
        base::Bind(&ShellDemuxer::DownloadTask, this, buffer));
  }
}

void ShellDemuxer::DownloadTask(scoped_refptr<ShellBuffer> buffer) {
  // We need a requested_au_ or to have canceled this request and
  // are buffering to a new location for this to make sense
  DCHECK(requested_au_);
  filter_graph_log_->LogDemuxerDownloadEvent(requested_au_->GetType());

  // do nothing if stopped
  if (stopped_) {
    return;
  }

  // Flushing is a signal to restart the request->download cycle with
  // a new request. Drop current request and issue a new one.
  if (flushing_) {
    requested_au_ = NULL;
    IssueNextRequestTask();
    return;
  }

  if (!requested_au_->Read(reader_, buffer)) {
    DLOG(ERROR) << "au read failed";
    host_->OnDemuxerError(PIPELINE_ERROR_READ);
    return;
  }

  // copy timestamp and duration values
  buffer->SetTimestamp(requested_au_->GetTimestamp());
  buffer->SetDuration(requested_au_->GetDuration());

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
  for (size_t i = 0; i < buffered.size(); ++i) {
     host_->AddBufferedTimeRange(buffered.start(i), buffered.end(i));
  }

  // if we are buffering see if we've bufferered enough data to continue
  if (buffering_) {
    base::TimeDelta audio_buffered =
        audio_demuxer_stream_->GetEndOfContiguousEnqueuedRange();
    if (audio_buffered != kNoTimestamp()) {
      // If we've buffered enough audio to start playing, proceed
      // TODO: make this a function of data bitrate and download speed
      if (audio_buffered >= buffer_timestamp_) {
        DLOG(INFO) << "demuxer got preload time, finishing seek";
        SetBuffering(false);
      }
    } else {
      DLOG(INFO) << "buffering but no audio buffered!";
    }
  }
  IssueNextRequestTask();
}

void ShellDemuxer::IssueNextRequestTask() {
  DCHECK(!requested_au_);
  // if we're stopped don't download anymore
  if (stopped_) {
    return;
  }
  // if we have eos in one or both buffers the decision is easy
  if (audio_reached_eos_ || video_reached_eos_) {
    if (audio_reached_eos_) {
      if (video_reached_eos_) {
        // both are true, issue no more requests!
        return;
      } else {
        // audio is at eos, video isn't, get more video
        Request(DemuxerStream::VIDEO);
      }
    } else {
      // audio is not at eos, video is, get more audio
      Request(DemuxerStream::AUDIO);
    }
    return;
  }

  // priority order for figuring out what to download next
  base::TimeDelta next_audio_hole =
      audio_demuxer_stream_->GetEndOfContiguousEnqueuedRange();
  // if the audio demuxer stream is empty, always fill it first
  if (next_audio_hole == kNoTimestamp()) {
    Request(DemuxerStream::AUDIO);
  } else {
    base::TimeDelta next_video_hole =
        video_demuxer_stream_->GetEndOfContiguousEnqueuedRange();
    // if the video demuxer stream is empty, we need data for it
    if (next_video_hole == kNoTimestamp()) {
      Request(DemuxerStream::VIDEO);
    } else {
      // alright, fill the earlier hole in the enqueued ranges
      if (next_video_hole < next_audio_hole) {
        Request(DemuxerStream::VIDEO);
      } else {
        Request(DemuxerStream::AUDIO);
      }
    }
  }
}

void ShellDemuxer::Stop(const base::Closure &callback) {
  DCHECK(MessageLoopBelongsToCurrentThread());
  // set our internal stop flag, to not treat read failures as
  // errors anymore but as a natural part of stopping
  stopped_ = true;
  // stop the reader, which will stop the datasource and call back
  reader_->Stop(BindToCurrentLoop(base::Bind(
      &ShellDemuxer::DataSourceStopped, this, BindToCurrentLoop(callback))));
}

void ShellDemuxer::DataSourceStopped(const base::Closure& callback) {
  DCHECK(MessageLoopBelongsToCurrentThread());
  filter_graph_log_->LogEvent(kObjectIdDemuxer, kEventStop);
  // stop the download thread
  blocking_thread_.Stop();

  // tell downstream we've stopped
  if (audio_demuxer_stream_) audio_demuxer_stream_->Stop();
  if (video_demuxer_stream_) video_demuxer_stream_->Stop();

  callback.Run();
}

void ShellDemuxer::Seek(base::TimeDelta time, const PipelineStatusCB& cb) {
  blocking_thread_.message_loop()->PostTask(FROM_HERE,
                          base::Bind(&ShellDemuxer::SeekTask, this, time, cb));
}

// runs on blocking thread
void ShellDemuxer::SeekTask(base::TimeDelta time, const PipelineStatusCB& cb) {
  filter_graph_log_->LogEvent(kObjectIdDemuxer, kEventSeek);
  DLOG(INFO) << base::StringPrintf(
      "seek to: %"PRId64" ms", time.InMilliseconds());
  // clear any enqueued buffers on demuxer streams
  audio_demuxer_stream_->FlushBuffers();
  video_demuxer_stream_->FlushBuffers();
  // advance parser to new timestamp
  if (!parser_->SeekTo(time)) {
    DLOG(ERROR) << "parser seek failed.";
    cb.Run(PIPELINE_ERROR_READ);
    return;
  }
  // buffer out to the seek timestamp + preset preload buffer time
  SetBuffering(true);
  buffer_timestamp_ = time +
      base::TimeDelta::FromMilliseconds(kDemuxerPreloadTimeMilliseconds);
  flushing_ = true;
  cb.Run(PIPELINE_OK);
}

void ShellDemuxer::OnAudioRendererDisabled() {
  NOTIMPLEMENTED();
}

void ShellDemuxer::SetPlaybackRate(float playback_rate) {
  data_source_->SetPlaybackRate(playback_rate);
}

scoped_refptr<DemuxerStream> ShellDemuxer::GetStream(
    media::DemuxerStream::Type type) {
  if (type == DemuxerStream::AUDIO) {
    return audio_demuxer_stream_;
  } else if (type == DemuxerStream::VIDEO) {
    return video_demuxer_stream_;
  } else {
    DLOG(WARNING) << "unsupported stream type requested";
  }
  return NULL;
}

base::TimeDelta ShellDemuxer::GetStartTime() const {
  // we always assume a start time of 0
  return base::TimeDelta();
}

const AudioDecoderConfig& ShellDemuxer::AudioConfig() {
  return parser_->AudioConfig();
}

const VideoDecoderConfig& ShellDemuxer::VideoConfig() {
  return parser_->VideoConfig();
}

bool ShellDemuxer::MessageLoopBelongsToCurrentThread() const {
  return message_loop_->BelongsToCurrentThread();
}

void ShellDemuxer::SetFilterGraphLog(
    scoped_refptr<ShellFilterGraphLog> filter_graph_log) {
  filter_graph_log_ = filter_graph_log;
}

scoped_refptr<ShellFilterGraphLog> ShellDemuxer::filter_graph_log() {
  return filter_graph_log_;
}

void ShellDemuxer::SetBuffering(bool enable) {
  buffering_ = enable;
  audio_demuxer_stream_->SetBuffering(enable);
  video_demuxer_stream_->SetBuffering(enable);
}

}  // namespace media
