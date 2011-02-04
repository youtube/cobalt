// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/time.h"
#include "media/base/filter_host.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/ffmpeg_util.h"
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
      stopped_(false) {
  DCHECK(demuxer_);

  // Determine our media format.
  switch (stream->codec->codec_type) {
    case CODEC_TYPE_AUDIO:
      media_format_.SetAsString(MediaFormat::kMimeType,
                                mime_type::kFFmpegAudio);
      media_format_.SetAsInteger(MediaFormat::kFFmpegCodecID,
                                 stream->codec->codec_id);
      break;
    case CODEC_TYPE_VIDEO:
      media_format_.SetAsString(MediaFormat::kMimeType,
                                mime_type::kFFmpegVideo);
      media_format_.SetAsInteger(MediaFormat::kFFmpegCodecID,
                                 stream->codec->codec_id);
      break;
    default:
      NOTREACHED();
      break;
  }

  // Calculate the duration.
  duration_ = ConvertStreamTimestamp(stream->time_base, stream->duration);
}

FFmpegDemuxerStream::~FFmpegDemuxerStream() {
  DCHECK(stopped_);
  DCHECK(read_queue_.empty());
  DCHECK(buffer_queue_.empty());
}

void* FFmpegDemuxerStream::QueryInterface(const char* id) {
  DCHECK(id);
  AVStreamProvider* interface_ptr = NULL;
  if (0 == strcmp(id, AVStreamProvider::interface_id())) {
    interface_ptr = this;
  }
  return interface_ptr;
}

bool FFmpegDemuxerStream::HasPendingReads() {
  DCHECK_EQ(MessageLoop::current(), demuxer_->message_loop());
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
  DCHECK(read_queue_.empty()) << "Read requests should be empty";
  buffer_queue_.clear();
}

void FFmpegDemuxerStream::Stop() {
  DCHECK_EQ(MessageLoop::current(), demuxer_->message_loop());
  buffer_queue_.clear();
  STLDeleteElements(&read_queue_);
  stopped_ = true;
}

base::TimeDelta FFmpegDemuxerStream::duration() {
  return duration_;
}

const MediaFormat& FFmpegDemuxerStream::media_format() {
  return media_format_;
}

void FFmpegDemuxerStream::Read(Callback1<Buffer*>::Type* read_callback) {
  DCHECK(read_callback);
  demuxer_->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &FFmpegDemuxerStream::ReadTask, read_callback));
}

void FFmpegDemuxerStream::ReadTask(Callback1<Buffer*>::Type* read_callback) {
  DCHECK_EQ(MessageLoop::current(), demuxer_->message_loop());

  // Don't accept any additional reads if we've been told to stop.
  //
  // TODO(scherkus): it would be cleaner if we replied with an error message.
  if (stopped_) {
    delete read_callback;
    return;
  }

  // Enqueue the callback and attempt to satisfy it immediately.
  read_queue_.push_back(read_callback);
  FulfillPendingRead();

  // There are still pending reads, demux some more.
  if (HasPendingReads()) {
    demuxer_->PostDemuxTask();
  }
}

void FFmpegDemuxerStream::FulfillPendingRead() {
  DCHECK_EQ(MessageLoop::current(), demuxer_->message_loop());
  if (buffer_queue_.empty() || read_queue_.empty()) {
    return;
  }

  // Dequeue a buffer and pending read pair.
  scoped_refptr<Buffer> buffer = buffer_queue_.front();
  scoped_ptr<Callback1<Buffer*>::Type> read_callback(read_queue_.front());
  buffer_queue_.pop_front();
  read_queue_.pop_front();

  // Execute the callback.
  read_callback->Run(buffer);
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

AVStream* FFmpegDemuxerStream::GetAVStream() {
  return stream_;
}

// static
base::TimeDelta FFmpegDemuxerStream::ConvertStreamTimestamp(
    const AVRational& time_base, int64 timestamp) {
  if (timestamp == static_cast<int64>(AV_NOPTS_VALUE))
    return kNoTimestamp;

  return ConvertTimestamp(time_base, timestamp);
}

//
// FFmpegDemuxer
//
FFmpegDemuxer::FFmpegDemuxer(MessageLoop* message_loop)
    : message_loop_(message_loop),
      format_context_(NULL),
      read_event_(false, false),
      read_has_failed_(false),
      last_read_bytes_(0),
      read_position_(0) {
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

  // Iterate each stream and destroy each one of them.
  int streams = format_context_->nb_streams;
  for (int i = 0; i < streams; ++i) {
    AVStream* stream = format_context_->streams[i];

    // The conditions for calling avcodec_close():
    // 1. AVStream is alive.
    // 2. AVCodecContext in AVStream is alive.
    // 3. AVCodec in AVCodecContext is alive.
    // Notice that closing a codec context without prior avcodec_open() will
    // result in a crash in FFmpeg.
    if (stream && stream->codec && stream->codec->codec) {
      stream->discard = AVDISCARD_ALL;
      avcodec_close(stream->codec);
    }
  }

  // Then finally cleanup the format context.
  av_close_input_file(format_context_);
  format_context_ = NULL;
}

void FFmpegDemuxer::PostDemuxTask() {
  message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &FFmpegDemuxer::DemuxTask));
}

void FFmpegDemuxer::Stop(FilterCallback* callback) {
  // Post a task to notify the streams to stop as well.
  message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &FFmpegDemuxer::StopTask, callback));

  // Then wakes up the thread from reading.
  SignalReadCompleted(DataSource::kReadError);
}

void FFmpegDemuxer::Seek(base::TimeDelta time, FilterCallback* callback) {
  // TODO(hclam): by returning from this method, it is assumed that the seek
  // operation is completed and filters behind the demuxer is good to issue
  // more reads, but we are posting a task here, which makes the seek operation
  // asynchronous, should change how seek works to make it fully asynchronous.
  message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &FFmpegDemuxer::SeekTask, time, callback));
}

void FFmpegDemuxer::OnAudioRendererDisabled() {
  message_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &FFmpegDemuxer::DisableAudioStreamTask));
}

void FFmpegDemuxer::Initialize(DataSource* data_source,
                               FilterCallback* callback) {
  message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &FFmpegDemuxer::InitializeTask,
                        make_scoped_refptr(data_source),
                        callback));
}

size_t FFmpegDemuxer::GetNumberOfStreams() {
  return streams_.size();
}

scoped_refptr<DemuxerStream> FFmpegDemuxer::GetStream(int stream) {
  DCHECK_GE(stream, 0);
  DCHECK_LT(stream, static_cast<int>(streams_.size()));
  return streams_[stream].get();
}

int FFmpegDemuxer::Read(int size, uint8* data) {
  DCHECK(data_source_);

  // If read has ever failed, return with an error.
  // TODO(hclam): use a more meaningful constant as error.
  if (read_has_failed_)
    return AVERROR_IO;

  // Even though FFmpeg defines AVERROR_EOF, it's not to be used with I/O
  // routines.  Instead return 0 for any read at or past EOF.
  int64 file_size;
  if (data_source_->GetSize(&file_size) && read_position_ >= file_size)
    return 0;

  // Asynchronous read from data source.
  data_source_->Read(read_position_, size, data,
                     NewCallback(this, &FFmpegDemuxer::OnReadCompleted));

  // TODO(hclam): The method is called on the demuxer thread and this method
  // call will block the thread. We need to implemented an additional thread to
  // let FFmpeg demuxer methods to run on.
  size_t last_read_bytes = WaitForRead();
  if (last_read_bytes == DataSource::kReadError) {
    host()->SetError(PIPELINE_ERROR_READ);

    // Returns with a negative number to signal an error to FFmpeg.
    read_has_failed_ = true;
    return AVERROR_IO;
  }
  read_position_ += last_read_bytes;

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
  if (!data_source_->GetSize(&file_size) || position >= file_size ||
      position < 0)
    return false;

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
                                   FilterCallback* callback) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  scoped_ptr<FilterCallback> c(callback);

  data_source_ = data_source;

  // Add ourself to Protocol list and get our unique key.
  std::string key = FFmpegGlue::GetInstance()->AddProtocol(this);

  // Open FFmpeg AVFormatContext.
  DCHECK(!format_context_);
  AVFormatContext* context = NULL;
  int result = av_open_input_file(&context, key.c_str(), NULL, 0, NULL);

  // Remove ourself from protocol list.
  FFmpegGlue::GetInstance()->RemoveProtocol(this);

  if (result < 0) {
    host()->SetError(DEMUXER_ERROR_COULD_NOT_OPEN);
    callback->Run();
    return;
  }

  DCHECK(context);
  format_context_ = context;

  // Fully initialize AVFormatContext by parsing the stream a little.
  result = av_find_stream_info(format_context_);
  if (result < 0) {
    host()->SetError(DEMUXER_ERROR_COULD_NOT_PARSE);
    callback->Run();
    return;
  }

  // Create demuxer streams for all supported streams.
  base::TimeDelta max_duration;
  const bool kDemuxerIsWebm = !strcmp("webm", format_context_->iformat->name);
  for (size_t i = 0; i < format_context_->nb_streams; ++i) {
    AVCodecContext* codec_context = format_context_->streams[i]->codec;
    CodecType codec_type = codec_context->codec_type;
    if (codec_type == CODEC_TYPE_AUDIO || codec_type == CODEC_TYPE_VIDEO) {
      AVStream* stream = format_context_->streams[i];
      // WebM is currently strictly VP8 and Vorbis.
      if (kDemuxerIsWebm && (stream->codec->codec_id != CODEC_ID_VP8 &&
        stream->codec->codec_id != CODEC_ID_VORBIS)) {
        packet_streams_.push_back(NULL);
        continue;
      }

      FFmpegDemuxerStream* demuxer_stream
          = new FFmpegDemuxerStream(this, stream);

      DCHECK(demuxer_stream);
      streams_.push_back(make_scoped_refptr(demuxer_stream));
      packet_streams_.push_back(make_scoped_refptr(demuxer_stream));
      max_duration = std::max(max_duration, demuxer_stream->duration());
    } else {
      packet_streams_.push_back(NULL);
    }
  }
  if (streams_.empty()) {
    host()->SetError(DEMUXER_ERROR_NO_SUPPORTED_STREAMS);
    callback->Run();
    return;
  }
  if (format_context_->duration != static_cast<int64_t>(AV_NOPTS_VALUE)) {
    // If there is a duration value in the container use that to find the
    // maximum between it and the duration from A/V streams.
    const AVRational av_time_base = {1, AV_TIME_BASE};
    max_duration =
        std::max(max_duration,
                 ConvertTimestamp(av_time_base, format_context_->duration));
  } else {
    // If the duration is not a valid value. Assume that this is a live stream
    // and we set duration to the maximum int64 number to represent infinity.
    max_duration = base::TimeDelta::FromMicroseconds(
        Limits::kMaxTimeInMicroseconds);
  }

  // Good to go: set the duration and notify we're done initializing.
  host()->SetDuration(max_duration);
  callback->Run();
}

void FFmpegDemuxer::SeekTask(base::TimeDelta time, FilterCallback* callback) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  scoped_ptr<FilterCallback> c(callback);

  // Tell streams to flush buffers due to seeking.
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
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
  callback->Run();
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

void FFmpegDemuxer::StopTask(FilterCallback* callback) {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    (*iter)->Stop();
  }
  if (callback) {
    callback->Run();
    delete callback;
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
    if (packet_streams_[i]->GetAVStream()->codec->codec_type ==
        CODEC_TYPE_AUDIO) {
      packet_streams_[i] = NULL;
    }
  }
}

bool FFmpegDemuxer::StreamsHavePendingReads() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
    if ((*iter)->HasPendingReads()) {
      return true;
    }
  }
  return false;
}

void FFmpegDemuxer::StreamHasEnded() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  StreamVector::iterator iter;
  for (iter = streams_.begin(); iter != streams_.end(); ++iter) {
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
