// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "media/base/data_buffer.h"
#include "media/base/filter_host.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
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
  AVPacketBuffer(AVPacket* packet, const base::TimeDelta& timestamp,
                 const base::TimeDelta& duration)
      : packet_(packet) {
    SetTimestamp(timestamp);
    SetDuration(duration);
  }

  virtual ~AVPacketBuffer() {
  }

  // Buffer implementation.
  virtual const uint8* GetData() const {
    return reinterpret_cast<const uint8*>(packet_->data);
  }

  virtual size_t GetDataSize() const {
    return static_cast<size_t>(packet_->size);
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

void FFmpegDemuxerStream::EnqueuePacket(AVPacket* packet) {
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
      !bitstream_converter_->ConvertPacket(packet)) {
    LOG(ERROR) << "Format converstion failed.";
  }

  // Enqueue the callback and attempt to satisfy a read immediately.
  scoped_refptr<Buffer> buffer(
      new AVPacketBuffer(packet, timestamp, duration));
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

void FFmpegDemuxerStream::Read(const ReadCallback& read_callback) {
  DCHECK(!read_callback.is_null());

  base::AutoLock auto_lock(lock_);
  // Don't accept any additional reads if we've been told to stop.
  // The demuxer_ may have been destroyed in the pipleine thread.
  //
  // TODO(scherkus): it would be cleaner if we replied with an error message.
  if (stopped_) {
    read_callback.Run(scoped_refptr<Buffer>(new DataBuffer(0)));
    return;
  }

  if (!buffer_queue_.empty()) {
    // Dequeue a buffer send back.
    scoped_refptr<Buffer> buffer = buffer_queue_.front();
    buffer_queue_.pop_front();

    // Execute the callback.
    read_callback.Run(buffer);

    if (!read_queue_.empty())
      demuxer_->PostDemuxTask();

  } else {
    demuxer_->message_loop()->PostTask(FROM_HERE, base::Bind(
        &FFmpegDemuxerStream::ReadTask, this, read_callback));
  }
}

void FFmpegDemuxerStream::ReadTask(const ReadCallback& read_callback) {
  DCHECK_EQ(MessageLoop::current(), demuxer_->message_loop());

  base::AutoLock auto_lock(lock_);
  // Don't accept any additional reads if we've been told to stop.
  //
  // TODO(scherkus): it would be cleaner if we replied with an error message.
  if (stopped_) {
    read_callback.Run(scoped_refptr<Buffer>(new DataBuffer(0)));
    return;
  }

  // Enqueue the callback and attempt to satisfy it immediately.
  read_queue_.push_back(read_callback);
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
  ReadCallback read_callback(read_queue_.front());
  buffer_queue_.pop_front();
  read_queue_.pop_front();

  // Execute the callback.
  read_callback.Run(buffer);
}

void FFmpegDemuxerStream::EnableBitstreamConverter() {
  // Called by hardware decoder to require different bitstream converter.
  // Currently we assume that converter is determined by codec_id;
  DCHECK(stream_);

  const char* filter_name = NULL;
  if (stream_->codec->codec_id == CODEC_ID_H264) {
    filter_name = "h264_mp4toannexb";
    // Use Chromium bitstream converter in case of H.264
    bitstream_converter_.reset(
        new FFmpegH264BitstreamConverter(stream_->codec));
    CHECK(bitstream_converter_->Initialize());
    return;
  }
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
    return kNoTimestamp;

  return ConvertFromTimeBase(time_base, timestamp);
}

//
// FFmpegDemuxer
//
FFmpegDemuxer::FFmpegDemuxer(MessageLoop* message_loop, bool local_source)
    : message_loop_(message_loop),
      local_source_(local_source),
      format_context_(NULL),
      read_event_(false, false),
      read_has_failed_(false),
      last_read_bytes_(0),
      read_position_(0),
      max_duration_(base::TimeDelta::FromMicroseconds(-1)),
      deferred_status_(PIPELINE_OK),
      first_seek_hack_(true),
      start_time_(kNoTimestamp) {
  DCHECK(message_loop_);
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

void FFmpegDemuxer::Seek(base::TimeDelta time, const FilterStatusCB& cb) {
  message_loop_->PostTask(FROM_HERE,
                          base::Bind(&FFmpegDemuxer::SeekTask, this, time, cb));
}

void FFmpegDemuxer::SetPlaybackRate(float playback_rate) {
  DCHECK(data_source_.get());
  data_source_->SetPlaybackRate(playback_rate);
}

void FFmpegDemuxer::SetPreload(Preload preload) {
  DCHECK(data_source_.get());
  data_source_->SetPreload(preload);
}

void FFmpegDemuxer::OnAudioRendererDisabled() {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &FFmpegDemuxer::DisableAudioStreamTask, this));
}

void FFmpegDemuxer::set_host(FilterHost* filter_host) {
  Demuxer::set_host(filter_host);
  if (data_source_)
    data_source_->set_host(filter_host);
  if (max_duration_.InMicroseconds() >= 0)
    host()->SetDuration(max_duration_);
  if (read_position_ > 0)
    host()->SetCurrentReadPosition(read_position_);
  if (deferred_status_ != PIPELINE_OK)
    host()->SetError(deferred_status_);
}

void FFmpegDemuxer::Initialize(DataSource* data_source,
                               const PipelineStatusCB& callback) {
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&FFmpegDemuxer::InitializeTask, this,
                 make_scoped_refptr(data_source),
                 callback));
}

scoped_refptr<DemuxerStream> FFmpegDemuxer::GetStream(
    DemuxerStream::Type type) {
  DCHECK_GE(type, 0);
  DCHECK_LT(type, DemuxerStream::NUM_TYPES);
  return streams_[type];
}

base::TimeDelta FFmpegDemuxer::GetStartTime() const {
  return start_time_;
}

size_t FFmpegDemuxer::Read(size_t size, uint8* data) {
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
  data_source_->Read(read_position_, size, data,
                     base::Bind(&FFmpegDemuxer::OnReadCompleted, this));

  // TODO(hclam): The method is called on the demuxer thread and this method
  // call will block the thread. We need to implemented an additional thread to
  // let FFmpeg demuxer methods to run on.
  size_t last_read_bytes = WaitForRead();
  if (last_read_bytes == DataSource::kReadError) {
    if (host())
      host()->SetError(PIPELINE_ERROR_READ);
    else
      deferred_status_ = PIPELINE_ERROR_READ;

    // Returns with a negative number to signal an error to FFmpeg.
    read_has_failed_ = true;
    return AVERROR(EIO);
  }
  read_position_ += last_read_bytes;

  if (host())
    host()->SetCurrentReadPosition(read_position_);

  return last_read_bytes;
}

bool FFmpegDemuxer::GetPosition(int64* position_out) {
  *position_out = read_position_;
  return true;
}

bool FFmpegDemuxer::SetPosition(int64 position) {
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
  DCHECK(data_source_);

  return data_source_->GetSize(size_out);
}

bool FFmpegDemuxer::IsStreaming() {
  DCHECK(data_source_);

  return data_source_->IsStreaming();
}

MessageLoop* FFmpegDemuxer::message_loop() {
  return message_loop_;
}

void FFmpegDemuxer::InitializeTask(DataSource* data_source,
                                   const PipelineStatusCB& callback) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  data_source_ = data_source;
  if (host())
    data_source_->set_host(host());

  // Add ourself to Protocol list and get our unique key.
  std::string key = FFmpegGlue::GetInstance()->AddProtocol(this);

  // Open FFmpeg AVFormatContext.
  DCHECK(!format_context_);
  AVFormatContext* context = NULL;
  int result = av_open_input_file(&context, key.c_str(), NULL, 0, NULL);

  // Remove ourself from protocol list.
  FFmpegGlue::GetInstance()->RemoveProtocol(this);

  if (result < 0) {
    callback.Run(DEMUXER_ERROR_COULD_NOT_OPEN);
    return;
  }

  DCHECK(context);
  format_context_ = context;

  // Fully initialize AVFormatContext by parsing the stream a little.
  result = av_find_stream_info(format_context_);
  if (result < 0) {
    callback.Run(DEMUXER_ERROR_COULD_NOT_PARSE);
    return;
  }

  // Create demuxer streams for all supported streams.
  streams_.resize(DemuxerStream::NUM_TYPES);
  base::TimeDelta max_duration;
  bool no_supported_streams = true;
  for (size_t i = 0; i < format_context_->nb_streams; ++i) {
    AVCodecContext* codec_context = format_context_->streams[i]->codec;
    AVMediaType codec_type = codec_context->codec_type;
    if (codec_type == AVMEDIA_TYPE_AUDIO || codec_type == AVMEDIA_TYPE_VIDEO) {
      AVStream* stream = format_context_->streams[i];
      scoped_refptr<FFmpegDemuxerStream> demuxer_stream(
          new FFmpegDemuxerStream(this, stream));
      if (!streams_[demuxer_stream->type()]) {
        no_supported_streams = false;
        streams_[demuxer_stream->type()] = demuxer_stream;
        max_duration = std::max(max_duration, demuxer_stream->duration());

        if (stream->first_dts != static_cast<int64_t>(AV_NOPTS_VALUE)) {
          const base::TimeDelta first_dts = ConvertFromTimeBase(
              stream->time_base, stream->first_dts);
          if (start_time_ == kNoTimestamp || first_dts < start_time_)
            start_time_ = first_dts;
        }
      }
      packet_streams_.push_back(demuxer_stream);
    } else {
      packet_streams_.push_back(NULL);
    }
  }
  if (no_supported_streams) {
    callback.Run(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
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
    // The duration is not a valid value. Assume that this is a live stream
    // and set duration to the maximum int64 number to represent infinity.
    max_duration = base::TimeDelta::FromMicroseconds(
        Limits::kMaxTimeInMicroseconds);
  }

  // Some demuxers, like WAV, do not put timestamps on their frames. We
  // assume the the start time is 0.
  if (start_time_ == kNoTimestamp)
    start_time_ = base::TimeDelta();

  // Good to go: set the duration and bitrate and notify we're done
  // initializing.
  if (host())
    host()->SetDuration(max_duration);
  max_duration_ = max_duration;

  int bitrate = GetBitrate();
  if (bitrate > 0)
    data_source_->SetBitrate(bitrate);

  callback.Run(PIPELINE_OK);
}

int FFmpegDemuxer::GetBitrate() {
  DCHECK(format_context_);

  // If there is a bitrate set on the container, use it.
  if (format_context_->bit_rate > 0)
    return format_context_->bit_rate;

  // Then try to sum the bitrates individually per stream.
  int bitrate = 0;
  for (size_t i = 0; i < format_context_->nb_streams; ++i) {
    AVCodecContext* codec_context = format_context_->streams[i]->codec;
    bitrate += codec_context->bit_rate;
  }
  if (bitrate > 0)
    return bitrate;

  // If there isn't a bitrate set in the container or streams, but there is a
  // valid duration, approximate the bitrate using the duration.
  if (max_duration_.InMilliseconds() > 0 &&
      max_duration_.InMicroseconds() < Limits::kMaxTimeInMicroseconds) {
    int64 filesize_in_bytes;
    if (GetSize(&filesize_in_bytes))
      return 8000 * filesize_in_bytes / max_duration_.InMilliseconds();
  }

  // Bitrate could not be determined.
  return 0;
}

bool FFmpegDemuxer::IsLocalSource() {
  return local_source_;
}

bool FFmpegDemuxer::IsSeekable() {
  return !IsStreaming();
}

void FFmpegDemuxer::SeekTask(base::TimeDelta time, const FilterStatusCB& cb) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);

  // TODO(scherkus): remove this by separating Seek() from Flush() from
  // Preroll() states (i.e., the implicit Seek(0) should really be a Preroll()).
  if (first_seek_hack_) {
    first_seek_hack_ = false;

    if (time == start_time_) {
      cb.Run(PIPELINE_OK);
      return;
    }
  }

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
  DCHECK_LT(packet->stream_index, static_cast<int>(packet_streams_.size()));
  FFmpegDemuxerStream* demuxer_stream = NULL;
  size_t i = packet->stream_index;
  // Defend against ffmpeg giving us a bad stream index.
  if (i < packet_streams_.size()) {
    demuxer_stream = packet_streams_[i];
  }
  if (demuxer_stream) {
    // Queue the packet with the appropriate stream.  The stream takes
    // ownership of the AVPacket.
    if (packet.get()) {
      // If a packet is returned by FFmpeg's av_parser_parse2()
      // the packet will reference an inner memory of FFmpeg.
      // In this case, the packet's "destruct" member is NULL,
      // and it MUST be duplicated. This fixes issue with MP3 and possibly
      // other codecs.  It is safe to call this function even if the packet does
      // not refer to inner memory from FFmpeg.
      av_dup_packet(packet.get());
      demuxer_stream->EnqueuePacket(packet.release());
    }
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

  StreamVector::iterator iter;
  for (size_t i = 0; i < packet_streams_.size(); ++i) {
    if (!packet_streams_[i])
      continue;

    // If the codec type is audio, remove the reference. DemuxTask() will
    // look for such reference, and this will result in deleting the
    // audio packets after they are demuxed.
    if (packet_streams_[i]->type() == DemuxerStream::AUDIO) {
      packet_streams_[i] = NULL;
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
    if (!*iter)
      continue;
    AVPacket* packet = new AVPacket();
    memset(packet, 0, sizeof(*packet));
    (*iter)->EnqueuePacket(packet);
  }
}

void FFmpegDemuxer::OnReadCompleted(size_t size) {
  SignalReadCompleted(size);
}

size_t FFmpegDemuxer::WaitForRead() {
  read_event_.Wait();
  return last_read_bytes_;
}

void FFmpegDemuxer::SignalReadCompleted(size_t size) {
  last_read_bytes_ = size;
  read_event_.Signal();
}

}  // namespace media
