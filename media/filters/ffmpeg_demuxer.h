// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Demuxer interface using FFmpeg's libavformat.  At this time
// will support demuxing any audio/video format thrown at it.  The streams
// output mime types audio/x-ffmpeg and video/x-ffmpeg and include an integer
// key FFmpegCodecID which contains the CodecID enumeration value.  The CodecIDs
// can be used to create and initialize the corresponding FFmpeg decoder.
//
// FFmpegDemuxer sets the duration of pipeline during initialization by using
// the duration of the longest audio/video stream.
//
// NOTE: since FFmpegDemuxer reads packets sequentially without seeking, media
// files with very large drift between audio/video streams may result in
// excessive memory consumption.
//
// When stopped, FFmpegDemuxer and FFmpegDemuxerStream release all callbacks
// and buffered packets.  Reads from a stopped FFmpegDemuxerStream will not be
// replied to.

#ifndef MEDIA_FILTERS_FFMPEG_DEMUXER_H_
#define MEDIA_FILTERS_FFMPEG_DEMUXER_H_

#include <deque>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/synchronization/waitable_event.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffers.h"
#include "media/base/demuxer.h"
#include "media/base/pipeline.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/ffmpeg_glue.h"

// FFmpeg forward declarations.
struct AVFormatContext;
struct AVPacket;
struct AVRational;
struct AVStream;

namespace media {

class BitstreamConverter;
class FFmpegDemuxer;

class FFmpegDemuxerStream : public DemuxerStream {
 public:
  // Keeps a copy of |demuxer| and initializes itself using information
  // inside |stream|.  Both parameters must outlive |this|.
  FFmpegDemuxerStream(FFmpegDemuxer* demuxer, AVStream* stream);

  // Returns true is this stream has pending reads, false otherwise.
  //
  // Safe to call on any thread.
  bool HasPendingReads();

  // Enqueues and takes ownership over the given AVPacket.
  void EnqueuePacket(AVPacket* packet);

  // Signals to empty the buffer queue and mark next packet as discontinuous.
  void FlushBuffers();

  // Empties the queues and ignores any additional calls to Read().
  void Stop();

  // Returns the duration of this stream.
  base::TimeDelta duration();

  // DemuxerStream implementation.
  virtual Type type() OVERRIDE;

  // If |buffer_queue_| is not empty will execute on caller's thread, otherwise
  // will post ReadTask to execute on demuxer's thread. Read will acquire
  // |lock_| for the life of the function so that means |read_callback| must
  // not make calls into FFmpegDemuxerStream directly or that may cause a
  // deadlock. |read_callback| should execute as quickly as possible because
  // |lock_| is held throughout the life of the callback.
  virtual void Read(const ReadCallback& read_callback) OVERRIDE;
  virtual void EnableBitstreamConverter() OVERRIDE;
  virtual const AudioDecoderConfig& audio_decoder_config() OVERRIDE;
  virtual const VideoDecoderConfig& video_decoder_config() OVERRIDE;

 private:
  virtual ~FFmpegDemuxerStream();

  // Carries out enqueuing a pending read on the demuxer thread.
  void ReadTask(const ReadCallback& read_callback);

  // Attempts to fulfill a single pending read by dequeueing a buffer and read
  // callback pair and executing the callback. The calling function must
  // acquire |lock_| before calling this function.
  void FulfillPendingRead();

  // Converts an FFmpeg stream timestamp into a base::TimeDelta.
  static base::TimeDelta ConvertStreamTimestamp(const AVRational& time_base,
                                                int64 timestamp);

  FFmpegDemuxer* demuxer_;
  AVStream* stream_;
  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;
  Type type_;
  base::TimeDelta duration_;
  bool discontinuous_;
  bool stopped_;

  typedef std::deque<scoped_refptr<Buffer> > BufferQueue;
  BufferQueue buffer_queue_;

  typedef std::deque<ReadCallback> ReadQueue;
  ReadQueue read_queue_;

  // Used to translate bitstream formats.
  scoped_ptr<BitstreamConverter> bitstream_converter_;

  // Used to synchronize access to |buffer_queue_|, |read_queue_|, and
  // |stopped_|. This is so other threads can get access to buffers that have
  // already been demuxed without having the demuxer thread sending the
  // buffers. |lock_| must be acquired before any access to |buffer_queue_|,
  // |read_queue_|, or |stopped_|.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxerStream);
};

class MEDIA_EXPORT FFmpegDemuxer : public Demuxer, public FFmpegURLProtocol {
 public:
  explicit FFmpegDemuxer(MessageLoop* message_loop);
  virtual ~FFmpegDemuxer();

  // Posts a task to perform additional demuxing.
  virtual void PostDemuxTask();

  void Initialize(
      DataSource* data_source, const PipelineStatusCB& callback);

  // Demuxer implementation.
  virtual void Stop(const base::Closure& callback) OVERRIDE;
  virtual void Seek(base::TimeDelta time, const FilterStatusCB& cb) OVERRIDE;
  virtual void OnAudioRendererDisabled() OVERRIDE;
  virtual void set_host(FilterHost* filter_host) OVERRIDE;
  virtual void SetPlaybackRate(float playback_rate) OVERRIDE;
  virtual scoped_refptr<DemuxerStream> GetStream(
      DemuxerStream::Type type) OVERRIDE;
  virtual void SetPreload(Preload preload) OVERRIDE;
  virtual base::TimeDelta GetStartTime() const OVERRIDE;
  virtual int GetBitrate() OVERRIDE;

  // FFmpegURLProtocol implementation.
  virtual size_t Read(size_t size, uint8* data) OVERRIDE;
  virtual bool GetPosition(int64* position_out) OVERRIDE;
  virtual bool SetPosition(int64 position) OVERRIDE;
  virtual bool GetSize(int64* size_out) OVERRIDE;
  virtual bool IsStreaming() OVERRIDE;

  // Provide access to FFmpegDemuxerStream.
  MessageLoop* message_loop();

  // For testing purposes.
  void disable_first_seek_hack_for_testing() { first_seek_hack_ = false; }

 private:
  // Only allow a factory to create this class.
  friend class MockFFmpegDemuxer;
  FRIEND_TEST_ALL_PREFIXES(FFmpegDemuxerTest, ProtocolRead);

  // Carries out initialization on the demuxer thread.
  void InitializeTask(
      DataSource* data_source, const PipelineStatusCB& callback);

  // Carries out a seek on the demuxer thread.
  void SeekTask(base::TimeDelta time, const FilterStatusCB& cb);

  // Carries out demuxing and satisfying stream reads on the demuxer thread.
  void DemuxTask();

  // Carries out stopping the demuxer streams on the demuxer thread.
  void StopTask(const base::Closure& callback);

  // Carries out disabling the audio stream on the demuxer thread.
  void DisableAudioStreamTask();

  // Returns true if any of the streams have pending reads.  Since we lazily
  // post a DemuxTask() for every read, we use this method to quickly terminate
  // the tasks if there is no work to do.
  //
  // Must be called on the demuxer thread.
  bool StreamsHavePendingReads();

  // Signal all FFmpegDemuxerStream that the stream has ended.
  //
  // Must be called on the demuxer thread.
  void StreamHasEnded();

  // Read callback method to be passed to DataSource. When the asynchronous
  // read has completed, this method will be called from DataSource with
  // number of bytes read or kDataSource in case of error.
  void OnReadCompleted(size_t size);

  // Wait for asynchronous read to complete and return number of bytes read.
  virtual size_t WaitForRead();

  // Signal that read has completed, and |size| bytes have been read.
  virtual void SignalReadCompleted(size_t size);

  MessageLoop* message_loop_;

  // FFmpeg context handle.
  AVFormatContext* format_context_;

  // Two vector of streams:
  //   - |streams_| is indexed by type for the Demuxer interface GetStream(),
  //     and contains NULLs for types which aren't present.
  //   - |packet_streams_| is indexed to mirror AVFormatContext when dealing
  //     with AVPackets returned from av_read_frame() and contain NULL entries
  //     representing unsupported streams where we throw away the data.
  //
  // Ownership is handled via reference counting.
  //
  // Once initialized, operations on FFmpegDemuxerStreams should be carried out
  // on the demuxer thread.
  typedef std::vector< scoped_refptr<FFmpegDemuxerStream> > StreamVector;
  StreamVector streams_;
  StreamVector packet_streams_;

  // Reference to the data source. Asynchronous read requests are submitted to
  // this object.
  scoped_refptr<DataSource> data_source_;

  // This member is used to block on read method calls from FFmpeg and wait
  // until the asynchronous reads in the data source to complete. It is also
  // signaled when the demuxer is being stopped.
  base::WaitableEvent read_event_;

  // Flag to indicate if read has ever failed. Once set to true, it will
  // never be reset. This flag is set true and accessed in Read().
  bool read_has_failed_;

  size_t last_read_bytes_;
  int64 read_position_;

  // Initialization can happen before set_host() is called, in which case we
  // store these bits for deferred reporting to the FilterHost when we get one.
  base::TimeDelta max_duration_;
  PipelineStatus deferred_status_;

  // Used to skip the implicit "first seek" to avoid resetting FFmpeg's internal
  // state.
  bool first_seek_hack_;

  // The first timestamp of the opened media file. This is used to set the
  // starting clock value to match the timestamps in the media file. Default
  // is 0.
  base::TimeDelta start_time_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegDemuxer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_DEMUXER_H_
