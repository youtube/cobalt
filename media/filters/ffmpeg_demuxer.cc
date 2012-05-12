// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/data_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/video_decoder_config.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/bitstream_converter.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/ffmpeg_h264_bitstream_converter.h"

namespace media {

//
// AVPacketBuffer
//
class AVPacketBuffer : public Buffer {
 public:
  AVPacketBuffer(scoped_ptr_malloc<AVPacket, ScopedPtrAVFreePacket> packet,
                 const base::TimeDelta& timestamp,
                 const base::TimeDelta& duration)
      : Buffer(timestamp, duration),
        packet_(packet.Pass()) {
  }

  virtual ~AVPacketBuffer() {}

  // Buffer implementation.
  virtual const uint8* GetData() const {
    return reinterpret_cast<const uint8*>(packet_->data);
  }

  virtual int GetDataSize() const {
    return packet_->size;
  }

 private:
  scoped_ptr_malloc<AVPacket, ScopedPtrAVFreePacket> packet_;

  DISALLOW_COPY_AND_ASSIGN(AVPacketBuffer);
};


//
// FFmpegDemuxerStream
//
FFmpegDemuxerStream::FFmpegDemuxerStream(FFmpegDemuxer* demuxer,
                                         AVStream* stream)
    : demuxer_(demuxer),
      stream_(stream),
      type_(UNKNOWN),
      discontinuous_(false),
      stopped_(false) {
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
}

FFmpegDemuxerStream::~FFmpegDemuxerStream() {
  base::AutoLock auto_lock(lock_);
  DCHECK(stopped_);
  DCHECK(read_queue_.empty());
  DCHECK(buffer_queue_.empty());
}

bool FFmpegDemuxerStream::HasPendingReads() {
  DCHECK_EQ(MessageLoop::current(), demuxer_->message_loop());
  base::AutoLock auto_lock(lock_);
  DCHECK(!stopped_ || read_queue_.empty())
      << "Read queue should have been emptied if demuxing stream is stopped";
  return !read_queue_.empty();
}

void FFmpegDemuxerStream::EnqueuePacket(
    scoped_ptr_malloc<AVPacket, ScopedPtrAVFreePacket> packet) {
  DCHECK_EQ(MessageLoop::current(), demuxer_->message_loop());
  base::TimeDelta timestamp =
      ConvertStreamTimestamp(stream_->time_base, packet->pts);
  base::TimeDelta duration =
      ConvertStreamTimestamp(stream_->time_base, packet->duration);

  base::AutoLock auto_lock(lock_);
  if (stopped_) {
    NOTREACHED() << "Attempted to enqueue packet on a stopped stream";
    return;
  }

  // Convert if the packet if there is bitstream filter.
  if (packet->data && bitstream_converter_.get() &&
      !bitstream_converter_->ConvertPacket(packet.get())) {
    LOG(ERROR) << "Format converstion failed.";
  }

  // Enqueue the callback and attempt to satisfy a read immediately.
  scoped_refptr<Buffer> buffer(
      new AVPacketBuffer(packet.Pass(), timestamp, duration));
  if (!buffer) {
    NOTREACHED() << "Unable to allocate AVPacketBuffer";
    return;
  }
  buffer_queue_.push_back(buffer);
  FulfillPendingRead();
  return;
}

void FFmpegDemuxerStream::FlushBuffers() {
  DCHECK_EQ(MessageLoop::current(), demuxer_->message_loop());
  base::AutoLock auto_lock(lock_);
  DCHECK(read_queue_.empty()) << "Read requests should be empty";
  buffer_queue_.clear();
}

void FFmpegDemuxerStream::Stop() {
  DCHECK_EQ(MessageLoop::current(), demuxer_->message_loop());
  base::AutoLock auto_lock(lock_);
  buffer_queue_.clear();
  for (ReadQueue::iterator it = read_queue_.begin();
       it != read_queue_.end(); ++it) {
    it->Run(scoped_refptr<Buffer>(new DataBuffer(0)));
  }
  read_queue_.clear();
  stopped_ = true;
}

base::TimeDelta FFmpegDemuxerStream::duration() {
  return duration_;
}

DemuxerStream::Type FFmpegDemuxerStream::type() {
  return type_;
}

void FFmpegDemuxerStream::Read(const ReadCB& read_cb) {
  DCHECK(!read_cb.is_null());

  base::AutoLock auto_lock(lock_);
  // Don't accept any additional reads if we've been told to stop.
  // The demuxer_ may have been destroyed in the pipleine thread.
  //
  // TODO(scherkus): it would be cleaner if we replied with an error message.
  if (stopped_) {
    read_cb.Run(scoped_refptr<Buffer>(new DataBuffer(0)));
    return;
  }

  // Buffers are only queued when there are no pending reads.
  DCHECK(buffer_queue_.empty() || read_queue_.empty());

  if (buffer_queue_.empty()) {
    demuxer_->message_loop()->PostTask(FROM_HERE, base::Bind(
        &FFmpegDemuxerStream::ReadTask, this, read_cb));
    return;
  }

  // Send the oldest buffer back.
  scoped_refptr<Buffer> buffer = buffer_queue_.front();
  buffer_queue_.pop_front();
  read_cb.Run(buffer);
}

void FFmpegDemuxerStream::ReadTask(const ReadCB& read_cb) {
  DCHECK_EQ(MessageLoop::current(), demuxer_->message_loop());

  base::AutoLock auto_lock(lock_);
  // Don't accept any additional reads if we've been told to stop.
  //
  // TODO(scherkus): it would be cleaner if we replied with an error message.
  if (stopped_) {
    read_cb.Run(scoped_refptr<Buffer>(new DataBuffer(0)));
    return;
  }

  // Enqueue the callback and attempt to satisfy it immediately.
  read_queue_.push_back(read_cb);
  FulfillPendingRead();

  // Check if there are still pending reads, demux some more.
  if (!read_queue_.empty()) {
    demuxer_->PostDemuxTask();
  }
}

void FFmpegDemuxerStream::FulfillPendingRead() {
  DCHECK_EQ(MessageLoop::current(), demuxer_->message_loop());
  lock_.AssertAcquired();
  if (buffer_queue_.empty() || read_queue_.empty()) {
    return;
  }

  // Dequeue a buffer and pending read pair.
  scoped_refptr<Buffer> buffer = buffer_queue_.front();
  ReadCB read_cb(read_queue_.front());
  buffer_queue_.pop_front();
  read_queue_.pop_front();

  // Execute the callback.
  read_cb.Run(buffer);
}

void FFmpegDemuxerStream::EnableBitstreamConverter() {
  // Called by hardware decoder to require different bitstream converter.
  // Currently we assume that converter is determined by codec_id;
  DCHECK(stream_);

  if (stream_->codec->codec_id == CODEC_ID_H264) {
    // Use Chromium bitstream converter in case of H.264
    bitstream_converter_.reset(
        new FFmpegH264BitstreamConverter(stream_->codec));
    CHECK(bitstream_converter_->Initialize());
    return;
  }

  const char* filter_name = NULL;
  if (stream_->codec->codec_id == CODEC_ID_MPEG4) {
    filter_name = "mpeg4video_es";
  } else if (stream_->codec->codec_id == CODEC_ID_WMV3) {
    filter_name = "vc1_asftorcv";
  } else if (stream_->codec->codec_id == CODEC_ID_VC1) {
    filter_name = "vc1_asftoannexg";
  }

  if (filter_name) {
    bitstream_converter_.reset(
        new FFmpegBitstreamConverter(filter_name, stream_->codec));
    CHECK(bitstream_converter_->Initialize());
  }
}

const AudioDecoderConfig& FFmpegDemuxerStream::audio_decoder_config() {
  CHECK_EQ(type_, AUDIO);
  return audio_config_;
}

const VideoDecoderConfig& FFmpegDemuxerStream::video_decoder_config() {
  CHECK_EQ(type_, VIDEO);
  return video_config_;
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
    MessageLoop* message_loop,
    const scoped_refptr<DataSource>& data_source)
    : host_(NULL),
      message_loop_(message_loop),
      format_context_(NULL),
      data_source_(data_source),
      read_event_(false, false),
      read_has_failed_(false),
      last_read_bytes_(0),
      read_position_(0),
      bitrate_(0),
      start_time_(kNoTimestamp()),
      audio_disabled_(false) {
  DCHECK(message_loop_);
  DCHECK(data_source_);
}

FFmpegDemuxer::~FFmpegDemuxer() {
  // In this destructor, we clean up resources held by FFmpeg. It is ugly to
  // close the codec contexts here because the corresponding codecs are opened
  // in the decoder filters. By reaching this point, all filters should have
  // stopped, so this is the only safe place to do the global clean up.
  // TODO(hclam): close the codecs in the corresponding decoders.
  if (!format_context_)
    return;

  DestroyAVFormatContext(format_context_);
  format_context_ = NULL;
}

void FFmpegDemuxer::PostDemuxTask() {
  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&FFmpegDemuxer::DemuxTask, this));
}

void FFmpegDemuxer::Stop(const base::Closure& callback) {
  // Post a task to notify the streams to stop as well.
  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&FFmpegDemuxer::StopTask, this, callback));

  // Then wakes up the thread from reading.
  SignalReadCompleted(DataSource::kReadError);
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
  StreamVector::iterator iter;
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

size_t FFmpegDemuxer::Read(size_t size, uint8* data) {
  DCHECK(host_);
  DCHECK(data_source_);

  // If read has ever failed, return with an error.
  // TODO(hclam): use a more meaningful constant as error.
  if (read_has_failed_)
    return AVERROR(EIO);

  // Even though FFmpeg defines AVERROR_EOF, it's not to be used with I/O
  // routines.  Instead return 0 for any read at or past EOF.
  int64 file_size;
  if (data_source_->GetSize(&file_size) && read_position_ >= file_size)
    return 0;

  // Asynchronous read from data source.
  data_source_->Read(read_position_, size, data, base::Bind(
      &FFmpegDemuxer::SignalReadCompleted, this));

  // TODO(hclam): The method is called on the demuxer thread and this method
  // call will block the thread. We need to implemented an additional thread to
  // let FFmpeg demuxer methods to run on.
  int last_read_bytes = WaitForRead();
  if (last_read_bytes == DataSource::kReadError) {
    host_->OnDemuxerError(PIPELINE_ERROR_READ);

    // Returns with a negative number to signal an error to FFmpeg.
    read_has_failed_ = true;
    return AVERROR(EIO);
  }
  read_position_ += last_read_bytes;
  host_->SetCurrentReadPosition(read_position_);

  return last_read_bytes;
}

bool FFmpegDemuxer::GetPosition(int64* position_out) {
  DCHECK(host_);
  *position_out = read_position_;
  return true;
}

bool FFmpegDemuxer::SetPosition(int64 position) {
  DCHECK(host_);
  DCHECK(data_source_);

  int64 file_size;
  if ((data_source_->GetSize(&file_size) && position >= file_size) ||
      position < 0) {
    return false;
  }

  read_position_ = position;
  return true;
}

bool FFmpegDemuxer::GetSize(int64* size_out) {
  DCHECK(host_);
  DCHECK(data_source_);
  return data_source_->GetSize(size_out);
}

bool FFmpegDemuxer::IsStreaming() {
  DCHECK(host_);
  DCHECK(data_source_);
  return data_source_->IsStreaming();
}

MessageLoop* FFmpegDemuxer::message_loop() {
  return message_loop_;
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
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  host_ = host;

  // TODO(scherkus): DataSource should have a host by this point,
  // see http://crbug.com/122071
  data_source_->set_host(host);

  // Add ourself to Protocol list and get our unique key.
  std::string key = FFmpegGlue::GetInstance()->AddProtocol(this);

  // Open FFmpeg AVFormatContext.
  DCHECK(!format_context_);
  AVFormatContext* context = avformat_alloc_context();

  // Disable ID3v1 tag reading to avoid costly seeks to end of file for data we
  // don't use.  FFmpeg will only read ID3v1 tags if no other metadata is
  // available, so add a metadata entry to ensure some is always present.
  av_dict_set(&context->metadata, "skip_id3v1_tags", "", 0);

  int result = avformat_open_input(&context, key.c_str(), NULL, NULL);

  // Remove ourself from protocol list.
  FFmpegGlue::GetInstance()->RemoveProtocol(this);

  if (result < 0) {
    status_cb.Run(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  DCHECK(context);
  format_context_ = context;

  // Fully initialize AVFormatContext by parsing the stream a little.
  result = avformat_find_stream_info(format_context_, NULL);
  if (result < 0) {
    status_cb.Run(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  // Create demuxer stream entries for each possible AVStream.
  streams_.resize(format_context_->nb_streams);
  bool found_audio_stream = false;
  bool found_video_stream = false;

  base::TimeDelta max_duration;
  for (size_t i = 0; i < format_context_->nb_streams; ++i) {
    AVCodecContext* codec_context = format_context_->streams[i]->codec;
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

    AVStream* stream = format_context_->streams[i];
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

  if (format_context_->duration != static_cast<int64_t>(AV_NOPTS_VALUE)) {
    // If there is a duration value in the container use that to find the
    // maximum between it and the duration from A/V streams.
    const AVRational av_time_base = {1, AV_TIME_BASE};
    max_duration =
        std::max(max_duration,
                 ConvertFromTimeBase(av_time_base, format_context_->duration));
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

  int64 filesize_in_bytes = 0;
  GetSize(&filesize_in_bytes);
  bitrate_ = CalculateBitrate(format_context_, max_duration, filesize_in_bytes);
  if (bitrate_ > 0)
    data_source_->SetBitrate(bitrate_);

  status_cb.Run(PIPELINE_OK);
}


int FFmpegDemuxer::GetBitrate() {
  DCHECK(format_context_) << "Initialize() has not been called";
  return bitrate_;
}

void FFmpegDemuxer::SeekTask(base::TimeDelta time, const PipelineStatusCB& cb) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  // Tell streams to flush buffers due to seeking.
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (*iter)
      (*iter)->FlushBuffers();
  }

  // Always seek to a timestamp less than or equal to the desired timestamp.
  int flags = AVSEEK_FLAG_BACKWARD;

  // Passing -1 as our stream index lets FFmpeg pick a default stream.  FFmpeg
  // will attempt to use the lowest-index video stream, if present, followed by
  // the lowest-index audio stream.
  if (av_seek_frame(format_context_, -1, time.InMicroseconds(), flags) < 0) {
    // Use VLOG(1) instead of NOTIMPLEMENTED() to prevent the message being
    // captured from stdout and contaminates testing.
    // TODO(scherkus): Implement this properly and signal error (BUG=23447).
    VLOG(1) << "Not implemented";
  }

  // Notify we're finished seeking.
  cb.Run(PIPELINE_OK);
}

void FFmpegDemuxer::DemuxTask() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  // Make sure we have work to do before demuxing.
  if (!StreamsHavePendingReads()) {
    return;
  }

  // Allocate and read an AVPacket from the media.
  scoped_ptr_malloc<AVPacket, ScopedPtrAVFreePacket> packet(new AVPacket());
  int result = av_read_frame(format_context_, packet.get());
  if (result < 0) {
    // If we have reached the end of stream, tell the downstream filters about
    // the event.
    StreamHasEnded();
    return;
  }

  // Queue the packet with the appropriate stream.
  // TODO(scherkus): should we post this back to the pipeline thread?  I'm
  // worried about downstream filters (i.e., decoders) executing on this
  // thread.
  DCHECK_GE(packet->stream_index, 0);
  DCHECK_LT(packet->stream_index, static_cast<int>(streams_.size()));

  // Defend against ffmpeg giving us a bad stream index.
  if (packet->stream_index >= 0 &&
      packet->stream_index < static_cast<int>(streams_.size()) &&
      streams_[packet->stream_index] &&
      (!audio_disabled_ ||
       streams_[packet->stream_index]->type() != DemuxerStream::AUDIO)) {
    FFmpegDemuxerStream* demuxer_stream = streams_[packet->stream_index];

    // If a packet is returned by FFmpeg's av_parser_parse2()
    // the packet will reference an inner memory of FFmpeg.
    // In this case, the packet's "destruct" member is NULL,
    // and it MUST be duplicated. This fixes issue with MP3 and possibly
    // other codecs.  It is safe to call this function even if the packet does
    // not refer to inner memory from FFmpeg.
    av_dup_packet(packet.get());

    demuxer_stream->EnqueuePacket(packet.Pass());
  }

  // Create a loop by posting another task.  This allows seek and message loop
  // quit tasks to get processed.
  if (StreamsHavePendingReads()) {
    PostDemuxTask();
  }
}

void FFmpegDemuxer::StopTask(const base::Closure& callback) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (*iter)
      (*iter)->Stop();
  }
  if (data_source_) {
    data_source_->Stop(callback);
  } else {
    callback.Run();
  }
}

void FFmpegDemuxer::DisableAudioStreamTask() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  audio_disabled_ = true;
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (*iter && (*iter)->type() == DemuxerStream::AUDIO) {
      (*iter)->Stop();
    }
  }
}

bool FFmpegDemuxer::StreamsHavePendingReads() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (*iter && (*iter)->HasPendingReads()) {
      return true;
    }
  }
  return false;
}

void FFmpegDemuxer::StreamHasEnded() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if (!*iter ||
        (audio_disabled_ && (*iter)->type() == DemuxerStream::AUDIO)) {
      continue;
    }
    scoped_ptr_malloc<AVPacket, ScopedPtrAVFreePacket> packet(new AVPacket());
    memset(packet.get(), 0, sizeof(*packet.get()));
    (*iter)->EnqueuePacket(packet.Pass());
  }
}

int FFmpegDemuxer::WaitForRead() {
  read_event_.Wait();
  return last_read_bytes_;
}

void FFmpegDemuxer::SignalReadCompleted(int size) {
  last_read_bytes_ = size;
  read_event_.Signal();
}

}  // namespace media
