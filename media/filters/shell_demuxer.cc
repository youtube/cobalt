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

ShellDemuxerStream::ShellDemuxerStream(ShellDemuxer* demuxer,
                                       Type type)
    : demuxer_(demuxer)
    , type_(type)
    , last_buffer_timestamp_(kNoTimestamp())
    , stopped_(false) {
  DCHECK(demuxer_);
  filter_graph_log_ = demuxer->filter_graph_log();
  filter_graph_log_->LogEvent(GraphLogObjectId(), kEventConstructor);
}

scoped_refptr<ShellFilterGraphLog> ShellDemuxerStream::filter_graph_log() {
  return filter_graph_log_;
}

bool ShellDemuxerStream::StreamWasEncrypted() const {
  if (type_ == VIDEO)
    return demuxer_->VideoConfig().is_encrypted();
  else if (type_ == AUDIO)
    return demuxer_->AudioConfig().is_encrypted();

  NOTREACHED();
  return false;
}

void ShellDemuxerStream::Read(const ReadCB& read_cb) {
  DCHECK(!read_cb.is_null());
  filter_graph_log_->LogEvent(GraphLogObjectId(), kEventRead);

  base::AutoLock auto_lock(lock_);

  // Don't accept any additional reads if we've been told to stop.
  // The demuxer_ may have been destroyed in the pipleine thread.
  if (stopped_) {
    filter_graph_log_->LogEvent(GraphLogObjectId(), kEventEndOfStreamSent);
    read_cb.Run(DemuxerStream::kOk,
                scoped_refptr<ShellBuffer>(
                    ShellBuffer::CreateEOSBuffer(kNoTimestamp(),
                                                 filter_graph_log_)));
    return;
  }

  // Buffers are only queued when there are no pending reads.
  DCHECK(buffer_queue_.empty() || read_queue_.empty());

  if (!buffer_queue_.empty()) {
    // Send the oldest buffer back.
    scoped_refptr<ShellBuffer> buffer = buffer_queue_.front();
    if (buffer->IsEndOfStream()) {
      filter_graph_log_->LogEvent(GraphLogObjectId(), kEventEndOfStreamSent);
    } else {
      // Do not pop EOS buffers, so that subsequent read requests also get EOS
      buffer_queue_.pop_front();
    }
    read_cb.Run(DemuxerStream::kOk, buffer);
  } else {
    filter_graph_log_->LogEvent(GraphLogObjectId(), kEventRequestInterrupt);
    read_queue_.push_back(read_cb);
  }
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
  filter_graph_log_->LogEvent(GraphLogObjectId(),
                              kEventEnqueue,
                              buffer->GetTimestamp().InMilliseconds());
  base::AutoLock auto_lock(lock_);
  if (stopped_) {
    // it's possible due to pipelining both downstream and within the
    // demuxer that several pipelined reads will be enqueuing packets
    // on a stopped stream. Drop them after complaining.
    DLOG(WARNING) << "attempted to enqueue packet on stopped stream";
    return;
  }

  if (buffer->IsEndOfStream()) {
    filter_graph_log_->LogEvent(GraphLogObjectId(), kEventEndOfStreamReceived);
  } else if (buffer->GetTimestamp() != kNoTimestamp()) {
    if (last_buffer_timestamp_ != kNoTimestamp() &&
        last_buffer_timestamp_ < buffer->GetTimestamp()) {
      buffered_ranges_.Add(last_buffer_timestamp_, buffer->GetTimestamp());
    }
    last_buffer_timestamp_ = buffer->GetTimestamp();
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
  }
}

base::TimeDelta ShellDemuxerStream::GetLastBufferTimestamp() const {
  base::AutoLock auto_lock(lock_);
  return last_buffer_timestamp_;
}

void ShellDemuxerStream::FlushBuffers() {
  filter_graph_log_->LogEvent(GraphLogObjectId(), kEventFlush);
  base::AutoLock auto_lock(lock_);
  DCHECK(read_queue_.empty()) << "Read requests should be empty";
  buffer_queue_.clear();
  last_buffer_timestamp_ = kNoTimestamp();
}

void ShellDemuxerStream::Stop() {
  DCHECK(demuxer_->MessageLoopBelongsToCurrentThread());
  filter_graph_log_->LogEvent(GraphLogObjectId(), kEventStop);
  base::AutoLock auto_lock(lock_);
  buffer_queue_.clear();
  last_buffer_timestamp_ = kNoTimestamp();
  // fulfill any pending callbacks with EOS buffers set to end timestamp
  for (ReadQueue::iterator it = read_queue_.begin();
       it != read_queue_.end(); ++it) {
    filter_graph_log_->LogEvent(GraphLogObjectId(), kEventEndOfStreamSent);
    it->Run(DemuxerStream::kOk,
            scoped_refptr<ShellBuffer>(
                ShellBuffer::CreateEOSBuffer(kNoTimestamp(),
                                             filter_graph_log_)));
  }
  read_queue_.clear();
  stopped_ = true;
}

const uint32 ShellDemuxerStream::GraphLogObjectId() const {
  return type_ == DemuxerStream::AUDIO ? kObjectIdAudioDemuxerStream :
                                         kObjectIdVideoDemuxerStream;
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
  reader_ = new ShellDataSourceReader();
  reader_->SetDataSource(data_source_);
}

ShellDemuxer::~ShellDemuxer() {
  NOTIMPLEMENTED();
}

void ShellDemuxer::Initialize(DemuxerHost* host,
                              const PipelineStatusCB& status_cb) {
  DCHECK(MessageLoopBelongsToCurrentThread());
  DCHECK(reader_);
  DCHECK(filter_graph_log_);
  filter_graph_log_->LogEvent(kObjectIdDemuxer, kEventInitialize);

  DLOG(INFO) << "this is a PROGRESSIVE playback.";

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

  // start downloading data
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

  uint32 event_id = type == DemuxerStream::AUDIO ? kEventRequestAudio :
                                                   kEventRequestVideo;
  filter_graph_log_->LogEvent(kObjectIdDemuxer,
                              event_id,
                              au->GetTimestamp().InMilliseconds());

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

  // enqueue the request
  requested_au_ = au;

  // AllocateBuffer will return false if the requested size is larger
  // than the maximum limit for a single buffer.
  if (!ShellBufferFactory::Instance()->AllocateBuffer(
      au->GetMaxSize(),
      base::Bind(&ShellDemuxer::BufferAllocated, this),
      filter_graph_log_)) {
    DLOG(ERROR) << "buffer allocation failed.";
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

  uint32 event_id = requested_au_->GetType() == DemuxerStream::AUDIO ?
      kEventDownloadAudio : kEventDownloadVideo;
  filter_graph_log_->LogEvent(kObjectIdDemuxer,
                              event_id,
                              requested_au_->GetTimestamp().InMilliseconds());
  // do nothing if stopped
  if (stopped_) {
    DLOG(INFO) << "aborting download task, stopped";
    return;
  }

  // Flushing is a signal to restart the request->download cycle with
  // a new request. Drop current request and issue a new one.
  // flushing_ will be reset by the next call to RequestTask()
  if (flushing_) {
    DLOG(INFO) << "skipped AU download due to flush";
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

  IssueNextRequestTask();
}

void ShellDemuxer::IssueNextRequestTask() {
  DCHECK(!requested_au_);
  // if we're stopped don't download anymore
  if (stopped_) {
    DLOG(INFO) << "stopped so request loop is stopping";
    return;
  }
  // if we have eos in one or both buffers the decision is easy
  if (audio_reached_eos_ || video_reached_eos_) {
    if (audio_reached_eos_) {
      if (video_reached_eos_) {
        // both are true, issue no more requests!
        DLOG(INFO) << "both streams at EOS, request loop stopping";
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
  base::TimeDelta audio_stamp = audio_demuxer_stream_->GetLastBufferTimestamp();
  base::TimeDelta video_stamp = video_demuxer_stream_->GetLastBufferTimestamp();
  // if the audio demuxer stream is empty, always fill it first
  if (audio_stamp == kNoTimestamp()) {
    Request(DemuxerStream::AUDIO);
  } else if (video_stamp == kNoTimestamp()) {
    // the video demuxer stream is empty, we need data for it
    Request(DemuxerStream::VIDEO);
  } else if (video_stamp < audio_stamp) {
    // video is earlier, fill it first
    Request(DemuxerStream::VIDEO);
  } else {
    Request(DemuxerStream::AUDIO);
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
      base::Bind(&ShellDemuxer::SeekTask, this, time, BindToCurrentLoop(cb)));
}

// runs on blocking thread
void ShellDemuxer::SeekTask(base::TimeDelta time, const PipelineStatusCB& cb) {
  filter_graph_log_->LogEvent(kObjectIdDemuxer,
                              kEventSeek,
                              time.InMilliseconds());
  DLOG(INFO) << base::StringPrintf(
      "seek to: %" PRId64" ms", time.InMilliseconds());
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
  if (audio_reached_eos_ && video_reached_eos_) {
    DLOG(INFO) << "restarting stopped request loop";
    Request(DemuxerStream::AUDIO);
  }
  audio_reached_eos_ = false;
  video_reached_eos_ = false;
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

}  // namespace media
