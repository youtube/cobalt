// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_demuxer.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/task_runner_util.h"
#include "base/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/bind_to_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/video_decoder_config.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/ffmpeg_h264_to_annex_b_bitstream_converter.h"

namespace media {

//
// FFmpegDemuxerStream
//
FFmpegDemuxerStream::FFmpegDemuxerStream(
    FFmpegDemuxer* demuxer,
    AVStream* stream)
    : demuxer_(demuxer),
      message_loop_(base::MessageLoopProxy::current()),
      stream_(stream),
      type_(UNKNOWN),
      stopped_(false),
      end_of_stream_(false),
      last_packet_timestamp_(kNoTimestamp()),
      bitstream_converter_enabled_(false) {
  DCHECK(demuxer_);

  // Determine our media format.
  switch (stream->codec->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
      type_ = AUDIO;
      AVCodecContextToAudioDecoderConfig(stream->codec, &audio_config_);
      break;
    case AVMEDIA_TYPE_VIDEO:
      type_ = VIDEO;
      AVStreamToVideoDecoderConfig(stream, &video_config_);
      break;
    default:
      NOTREACHED();
      break;
  }

  // Calculate the duration.
  duration_ = ConvertStreamTimestamp(stream->time_base, stream->duration);

  if (stream_->codec->codec_id == CODEC_ID_H264) {
    bitstream_converter_.reset(
        new FFmpegH264ToAnnexBBitstreamConverter(stream_->codec));
  }
}

void FFmpegDemuxerStream::EnqueuePacket(ScopedAVPacket packet) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (stopped_ || end_of_stream_) {
    NOTREACHED() << "Attempted to enqueue packet on a stopped stream";
    return;
  }

  // Convert the packet if there is a bitstream filter.
  if (packet->data && bitstream_converter_enabled_ &&
      !bitstream_converter_->ConvertPacket(packet.get())) {
    LOG(ERROR) << "Format conversion failed.";
  }

  // If a packet is returned by FFmpeg's av_parser_parse2() the packet will
  // reference inner memory of FFmpeg.  As such we should transfer the packet
  // into memory we control.
  scoped_refptr<DecoderBuffer> buffer;
  buffer = DecoderBuffer::CopyFrom(packet->data, packet->size);
  buffer->SetTimestamp(ConvertStreamTimestamp(
      stream_->time_base, packet->pts));
  buffer->SetDuration(ConvertStreamTimestamp(
      stream_->time_base, packet->duration));
  if (buffer->GetTimestamp() != kNoTimestamp() &&
      last_packet_timestamp_ != kNoTimestamp() &&
      last_packet_timestamp_ < buffer->GetTimestamp()) {
    buffered_ranges_.Add(last_packet_timestamp_, buffer->GetTimestamp());
    demuxer_->NotifyBufferingChanged();
  }
  last_packet_timestamp_ = buffer->GetTimestamp();

  buffer_queue_.Push(buffer);
  SatisfyPendingReads();
}

void FFmpegDemuxerStream::SetEndOfStream() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  end_of_stream_ = true;
  SatisfyPendingReads();
}

void FFmpegDemuxerStream::FlushBuffers() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(read_queue_.empty()) << "Read requests should be empty";
  buffer_queue_.Clear();
  end_of_stream_ = false;
  last_packet_timestamp_ = kNoTimestamp();
}

void FFmpegDemuxerStream::Stop() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  buffer_queue_.Clear();
  for (ReadQueue::iterator it = read_queue_.begin();
       it != read_queue_.end(); ++it) {
    it->Run(DemuxerStream::kOk, DecoderBuffer::CreateEOSBuffer());
  }
  read_queue_.clear();
  stopped_ = true;
  end_of_stream_ = true;
}

base::TimeDelta FFmpegDemuxerStream::duration() {
  return duration_;
}

DemuxerStream::Type FFmpegDemuxerStream::type() {
  return type_;
}

void FFmpegDemuxerStream::Read(const ReadCB& read_cb) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FFmpegDemuxerStream::Read, this, read_cb));
    return;
  }

  // Don't accept any additional reads if we've been told to stop.
  // The |demuxer_| may have been destroyed in the pipeline thread.
  //
  // TODO(scherkus): it would be cleaner to reply with an error message.
  if (stopped_) {
    read_cb.Run(DemuxerStream::kOk, DecoderBuffer::CreateEOSBuffer());
    return;
  }

  read_queue_.push_back(read_cb);
  SatisfyPendingReads();
}

void FFmpegDemuxerStream::EnableBitstreamConverter() {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &FFmpegDemuxerStream::EnableBitstreamConverter, this));
    return;
  }
  CHECK(bitstream_converter_.get());
  bitstream_converter_enabled_ = true;
}

const AudioDecoderConfig& FFmpegDemuxerStream::audio_decoder_config() {
  CHECK_EQ(type_, AUDIO);
  return audio_config_;
}

const VideoDecoderConfig& FFmpegDemuxerStream::video_decoder_config() {
  CHECK_EQ(type_, VIDEO);
  return video_config_;
}

FFmpegDemuxerStream::~FFmpegDemuxerStream() {
  DCHECK(stopped_);
  DCHECK(read_queue_.empty());
  DCHECK(buffer_queue_.IsEmpty());
}

base::TimeDelta FFmpegDemuxerStream::GetElapsedTime() const {
  return ConvertStreamTimestamp(stream_->time_base, stream_->cur_dts);
}

Ranges<base::TimeDelta> FFmpegDemuxerStream::GetBufferedRanges() const {
  return buffered_ranges_;
}

void FFmpegDemuxerStream::SatisfyPendingReads() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  while (!read_queue_.empty()) {
    scoped_refptr<DecoderBuffer> buffer;

    if (!buffer_queue_.IsEmpty()) {
      buffer = buffer_queue_.Pop();
    } else if (end_of_stream_) {
      buffer = DecoderBuffer::CreateEOSBuffer();
    } else {
      break;
    }

    // Send buffer back on a new execution stack to avoid recursing.
    ReadCB read_cb = read_queue_.front();
    read_queue_.pop_front();
    message_loop_->PostTask(FROM_HERE, base::Bind(
        read_cb, DemuxerStream::kOk, buffer));
  }

  // Have capacity? Ask for more!
  if (HasAvailableCapacity() && !end_of_stream_) {
    demuxer_->NotifyCapacityAvailable();
  }
}

bool FFmpegDemuxerStream::HasAvailableCapacity() {
  // Try to have one second's worth of encoded data per stream.
  const base::TimeDelta kCapacity = base::TimeDelta::FromSeconds(1);
  return buffer_queue_.IsEmpty() || buffer_queue_.Duration() < kCapacity;
}

// static
base::TimeDelta FFmpegDemuxerStream::ConvertStreamTimestamp(
    const AVRational& time_base, int64 timestamp) {
  if (timestamp == static_cast<int64>(AV_NOPTS_VALUE))
    return kNoTimestamp();

  return ConvertFromTimeBase(time_base, timestamp);
}

//
// FFmpegDemuxer
//
FFmpegDemuxer::FFmpegDemuxer(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const scoped_refptr<DataSource>& data_source)
    : host_(NULL),
      message_loop_(message_loop),
      blocking_thread_("FFmpegDemuxer"),
      pending_read_(false),
      pending_seek_(false),
      data_source_(data_source),
      bitrate_(0),
      start_time_(kNoTimestamp()),
      audio_disabled_(false),
      duration_known_(false),
      url_protocol_(data_source, BindToLoop(message_loop_, base::Bind(
          &FFmpegDemuxer::OnDataSourceError, base::Unretained(this)))) {
  DCHECK(message_loop_);
  DCHECK(data_source_);
}

FFmpegDemuxer::~FFmpegDemuxer() {}

void FFmpegDemuxer::Stop(const base::Closure& callback) {
  // Post a task to notify the streams to stop as well.
  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&FFmpegDemuxer::StopTask, this, callback));
}

void FFmpegDemuxer::Seek(base::TimeDelta time, const PipelineStatusCB& cb) {
  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&FFmpegDemuxer::SeekTask, this, time, cb));
}

void FFmpegDemuxer::SetPlaybackRate(float playback_rate) {
  DCHECK(data_source_.get());
  data_source_->SetPlaybackRate(playback_rate);
}

void FFmpegDemuxer::OnAudioRendererDisabled() {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &FFmpegDemuxer::DisableAudioStreamTask, this));
}

void FFmpegDemuxer::Initialize(DemuxerHost* host,
                               const PipelineStatusCB& status_cb) {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &FFmpegDemuxer::InitializeTask, this, host, status_cb));
}

scoped_refptr<DemuxerStream> FFmpegDemuxer::GetStream(
    DemuxerStream::Type type) {
  return GetFFmpegStream(type);
}

scoped_refptr<FFmpegDemuxerStream> FFmpegDemuxer::GetFFmpegStream(
    DemuxerStream::Type type) const {
  StreamVector::const_iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (*iter && (*iter)->type() == type) {
      return *iter;
    }
  }
  return NULL;
}

base::TimeDelta FFmpegDemuxer::GetStartTime() const {
  return start_time_;
}

// Helper for calculating the bitrate of the media based on information stored
// in |format_context| or failing that the size and duration of the media.
//
// Returns 0 if a bitrate could not be determined.
static int CalculateBitrate(
    AVFormatContext* format_context,
    const base::TimeDelta& duration,
    int64 filesize_in_bytes) {
  // If there is a bitrate set on the container, use it.
  if (format_context->bit_rate > 0)
    return format_context->bit_rate;

  // Then try to sum the bitrates individually per stream.
  int bitrate = 0;
  for (size_t i = 0; i < format_context->nb_streams; ++i) {
    AVCodecContext* codec_context = format_context->streams[i]->codec;
    bitrate += codec_context->bit_rate;
  }
  if (bitrate > 0)
    return bitrate;

  // See if we can approximate the bitrate as long as we have a filesize and
  // valid duration.
  if (duration.InMicroseconds() <= 0 ||
      duration == kInfiniteDuration() ||
      filesize_in_bytes == 0) {
    return 0;
  }

  // Do math in floating point as we'd overflow an int64 if the filesize was
  // larger than ~1073GB.
  double bytes = filesize_in_bytes;
  double duration_us = duration.InMicroseconds();
  return bytes * 8000000.0 / duration_us;
}

void FFmpegDemuxer::InitializeTask(DemuxerHost* host,
                                   const PipelineStatusCB& status_cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  host_ = host;

  // TODO(scherkus): DataSource should have a host by this point,
  // see http://crbug.com/122071
  data_source_->set_host(host);

  glue_.reset(new FFmpegGlue(&url_protocol_));
  AVFormatContext* format_context = glue_->format_context();

  // Disable ID3v1 tag reading to avoid costly seeks to end of file for data we
  // don't use.  FFmpeg will only read ID3v1 tags if no other metadata is
  // available, so add a metadata entry to ensure some is always present.
  av_dict_set(&format_context->metadata, "skip_id3v1_tags", "", 0);

  // Open the AVFormatContext using our glue layer.
  CHECK(blocking_thread_.Start());
  base::PostTaskAndReplyWithResult(
      blocking_thread_.message_loop_proxy(), FROM_HERE,
      base::Bind(&FFmpegGlue::OpenContext, base::Unretained(glue_.get())),
      base::Bind(&FFmpegDemuxer::OnOpenContextDone, this, status_cb));
}

void FFmpegDemuxer::OnOpenContextDone(const PipelineStatusCB& status_cb,
                                      bool result) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (!blocking_thread_.IsRunning()) {
    status_cb.Run(PIPELINE_ERROR_ABORT);
    return;
  }

  if (!result) {
    status_cb.Run(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  // Fully initialize AVFormatContext by parsing the stream a little.
  base::PostTaskAndReplyWithResult(
      blocking_thread_.message_loop_proxy(), FROM_HERE,
      base::Bind(&avformat_find_stream_info, glue_->format_context(),
                 static_cast<AVDictionary**>(NULL)),
      base::Bind(&FFmpegDemuxer::OnFindStreamInfoDone, this, status_cb));
}

void FFmpegDemuxer::OnFindStreamInfoDone(const PipelineStatusCB& status_cb,
                                         int result) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (!blocking_thread_.IsRunning()) {
    status_cb.Run(PIPELINE_ERROR_ABORT);
    return;
  }

  if (result < 0) {
    status_cb.Run(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  // Create demuxer stream entries for each possible AVStream.
  AVFormatContext* format_context = glue_->format_context();
  streams_.resize(format_context->nb_streams);
  bool found_audio_stream = false;
  bool found_video_stream = false;

  base::TimeDelta max_duration;
  for (size_t i = 0; i < format_context->nb_streams; ++i) {
    AVCodecContext* codec_context = format_context->streams[i]->codec;
    AVMediaType codec_type = codec_context->codec_type;

    if (codec_type == AVMEDIA_TYPE_AUDIO) {
      if (found_audio_stream)
        continue;
      // Ensure the codec is supported.
      if (CodecIDToAudioCodec(codec_context->codec_id) == kUnknownAudioCodec)
        continue;
      found_audio_stream = true;
    } else if (codec_type == AVMEDIA_TYPE_VIDEO) {
      if (found_video_stream)
        continue;
      // Ensure the codec is supported.
      if (CodecIDToVideoCodec(codec_context->codec_id) == kUnknownVideoCodec)
        continue;
      found_video_stream = true;
    } else {
      continue;
    }

    AVStream* stream = format_context->streams[i];
    scoped_refptr<FFmpegDemuxerStream> demuxer_stream(
        new FFmpegDemuxerStream(this, stream));

    streams_[i] = demuxer_stream;
    max_duration = std::max(max_duration, demuxer_stream->duration());

    if (stream->first_dts != static_cast<int64_t>(AV_NOPTS_VALUE)) {
      const base::TimeDelta first_dts = ConvertFromTimeBase(
          stream->time_base, stream->first_dts);
      if (start_time_ == kNoTimestamp() || first_dts < start_time_)
        start_time_ = first_dts;
    }
  }

  if (!found_audio_stream && !found_video_stream) {
    status_cb.Run(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
    return;
  }

  if (format_context->duration != static_cast<int64_t>(AV_NOPTS_VALUE)) {
    // If there is a duration value in the container use that to find the
    // maximum between it and the duration from A/V streams.
    const AVRational av_time_base = {1, AV_TIME_BASE};
    max_duration =
        std::max(max_duration,
                 ConvertFromTimeBase(av_time_base, format_context->duration));
  } else {
    // The duration is unknown, in which case this is likely a live stream.
    max_duration = kInfiniteDuration();
  }

  // Some demuxers, like WAV, do not put timestamps on their frames. We
  // assume the the start time is 0.
  if (start_time_ == kNoTimestamp())
    start_time_ = base::TimeDelta();

  // Good to go: set the duration and bitrate and notify we're done
  // initializing.
  host_->SetDuration(max_duration);
  duration_known_ = (max_duration != kInfiniteDuration());

  int64 filesize_in_bytes = 0;
  url_protocol_.GetSize(&filesize_in_bytes);
  bitrate_ = CalculateBitrate(format_context, max_duration, filesize_in_bytes);
  if (bitrate_ > 0)
    data_source_->SetBitrate(bitrate_);

  status_cb.Run(PIPELINE_OK);
}

void FFmpegDemuxer::SeekTask(base::TimeDelta time, const PipelineStatusCB& cb) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  CHECK(!pending_seek_);

  // TODO(scherkus): Inspect |pending_read_| and cancel IO via |blocking_url_|,
  // otherwise we can end up waiting for a pre-seek read to complete even though
  // we know we're going to drop it on the floor.

  // Always seek to a timestamp less than or equal to the desired timestamp.
  int flags = AVSEEK_FLAG_BACKWARD;

  // Passing -1 as our stream index lets FFmpeg pick a default stream.  FFmpeg
  // will attempt to use the lowest-index video stream, if present, followed by
  // the lowest-index audio stream.
  pending_seek_ = true;
  base::PostTaskAndReplyWithResult(
      blocking_thread_.message_loop_proxy(), FROM_HERE,
      base::Bind(&av_seek_frame, glue_->format_context(), -1,
                 time.InMicroseconds(), flags),
      base::Bind(&FFmpegDemuxer::OnSeekFrameDone, this, cb));
}

void FFmpegDemuxer::OnSeekFrameDone(const PipelineStatusCB& cb, int result) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  CHECK(pending_seek_);
  pending_seek_ = false;

  if (!blocking_thread_.IsRunning()) {
    cb.Run(PIPELINE_ERROR_ABORT);
    return;
  }

  if (result < 0) {
    // Use VLOG(1) instead of NOTIMPLEMENTED() to prevent the message being
    // captured from stdout and contaminates testing.
    // TODO(scherkus): Implement this properly and signal error (BUG=23447).
    VLOG(1) << "Not implemented";
  }

  // Tell streams to flush buffers due to seeking.
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (*iter)
      (*iter)->FlushBuffers();
  }

  // Resume demuxing until capacity.
  DemuxTask();

  // Notify we're finished seeking.
  cb.Run(PIPELINE_OK);
}

void FFmpegDemuxer::DemuxTask() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  // Make sure we have work to do before demuxing.
  if (!blocking_thread_.IsRunning() || !StreamsHaveAvailableCapacity() ||
      pending_read_ || pending_seek_) {
    return;
  }

  // Allocate and read an AVPacket from the media. Save |packet_ptr| since
  // evaluation order of packet.get() and base::Passed(&packet) is
  // undefined.
  ScopedAVPacket packet(new AVPacket());
  AVPacket* packet_ptr = packet.get();

  pending_read_ = true;
  base::PostTaskAndReplyWithResult(
      blocking_thread_.message_loop_proxy(), FROM_HERE,
      base::Bind(&av_read_frame, glue_->format_context(), packet_ptr),
      base::Bind(&FFmpegDemuxer::OnReadFrameDone, this, base::Passed(&packet)));
}

void FFmpegDemuxer::OnReadFrameDone(ScopedAVPacket packet, int result) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(pending_read_);
  pending_read_ = false;

  if (!blocking_thread_.IsRunning() || pending_seek_) {
    return;
  }

  if (result < 0) {
    // Update the duration based on the audio stream if
    // it was previously unknown http://crbug.com/86830
    if (!duration_known_) {
      // Search streams for AUDIO one.
      for (StreamVector::iterator iter = streams_.begin();
           iter != streams_.end();
           ++iter) {
        if (*iter && (*iter)->type() == DemuxerStream::AUDIO) {
          base::TimeDelta duration = (*iter)->GetElapsedTime();
          if (duration != kNoTimestamp() && duration > base::TimeDelta()) {
            host_->SetDuration(duration);
            duration_known_ = true;
          }
          break;
        }
      }
    }
    // If we have reached the end of stream, tell the downstream filters about
    // the event.
    StreamHasEnded();
    return;
  }

  // Queue the packet with the appropriate stream.
  DCHECK_GE(packet->stream_index, 0);
  DCHECK_LT(packet->stream_index, static_cast<int>(streams_.size()));

  // Defend against ffmpeg giving us a bad stream index.
  if (packet->stream_index >= 0 &&
      packet->stream_index < static_cast<int>(streams_.size()) &&
      streams_[packet->stream_index] &&
      (!audio_disabled_ ||
       streams_[packet->stream_index]->type() != DemuxerStream::AUDIO)) {
    FFmpegDemuxerStream* demuxer_stream = streams_[packet->stream_index];
    demuxer_stream->EnqueuePacket(packet.Pass());
  }

  // Keep demuxing until we've reached capacity.
  DemuxTask();
}

void FFmpegDemuxer::StopTask(const base::Closure& callback) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  url_protocol_.Abort();
  data_source_->Stop(BindToLoop(message_loop_, base::Bind(
      &FFmpegDemuxer::OnDataSourceStopped, this, callback)));
}

void FFmpegDemuxer::OnDataSourceStopped(const base::Closure& callback) {
  // This will block until all tasks complete. Note that after this returns it's
  // possible for reply tasks (e.g., OnReadFrameDone()) to be queued on this
  // thread. Each of the reply task methods must check whether we've stopped the
  // thread and drop their results on the floor.
  DCHECK(message_loop_->BelongsToCurrentThread());
  blocking_thread_.Stop();

  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (*iter)
      (*iter)->Stop();
  }

  callback.Run();
}

void FFmpegDemuxer::DisableAudioStreamTask() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  audio_disabled_ = true;
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (*iter && (*iter)->type() == DemuxerStream::AUDIO) {
      (*iter)->Stop();
    }
  }
}

bool FFmpegDemuxer::StreamsHaveAvailableCapacity() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (*iter && (*iter)->HasAvailableCapacity()) {
      return true;
    }
  }
  return false;
}

void FFmpegDemuxer::StreamHasEnded() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (!*iter ||
        (audio_disabled_ && (*iter)->type() == DemuxerStream::AUDIO)) {
      continue;
    }
    (*iter)->SetEndOfStream();
  }
}

void FFmpegDemuxer::NotifyCapacityAvailable() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DemuxTask();
}

void FFmpegDemuxer::NotifyBufferingChanged() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  Ranges<base::TimeDelta> buffered;
  scoped_refptr<FFmpegDemuxerStream> audio =
      audio_disabled_ ? NULL : GetFFmpegStream(DemuxerStream::AUDIO);
  scoped_refptr<FFmpegDemuxerStream> video =
      GetFFmpegStream(DemuxerStream::VIDEO);
  if (audio && video) {
    buffered = audio->GetBufferedRanges().IntersectionWith(
        video->GetBufferedRanges());
  } else if (audio) {
    buffered = audio->GetBufferedRanges();
  } else if (video) {
    buffered = video->GetBufferedRanges();
  }
  for (size_t i = 0; i < buffered.size(); ++i)
    host_->AddBufferedTimeRange(buffered.start(i), buffered.end(i));
}

void FFmpegDemuxer::OnDataSourceError() {
  host_->OnDemuxerError(PIPELINE_ERROR_READ);
}

}  // namespace media
