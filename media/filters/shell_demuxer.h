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

#ifndef MEDIA_FILTERS_SHELL_DEMUXER_H_
#define MEDIA_FILTERS_SHELL_DEMUXER_H_

#include <deque>
#include <map>
#include <vector>

#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "media/base/demuxer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/ranges.h"
#include "media/base/shell_buffer_factory.h"
#include "media/filters/shell_parser.h"

namespace media {

class DecoderBuffer;
class ShellDemuxer;

// Due to round-off error in calculation of timestamps and durations
// not computed in SI time units it's possible contiguous timetamps
// may not be exactly equal. We accept buffers that are within + or -
// this time range.
static const uint64_t kBufferTimeEpsilonMicroseconds = 100;

class ShellDemuxerStream : public DemuxerStream {
 public:
  ShellDemuxerStream(ShellDemuxer* demuxer, Type type);

  // DemuxerStream implementation
  virtual void Read(const ReadCB& read_cb);
  virtual const AudioDecoderConfig& audio_decoder_config();
  virtual const VideoDecoderConfig& video_decoder_config();
  virtual Ranges<base::TimeDelta> GetBufferedRanges();
  virtual Type type();
  virtual void EnableBitstreamConverter();

  // Functions used by ShellDemuxer
  void EnqueueBuffer(scoped_refptr<ShellBuffer> buffer);
  void FlushBuffers();
  void Stop();
  // returns the timestamp of the first buffer in buffer_queue_,
  // or kNoTimestamp() if the queue is empty.
  base::TimeDelta OldestEnqueuedTimestamp();
  void SetBuffering(bool buffering);

 private:
  // non-owning pointer to avoid circular reference
  ShellDemuxer* demuxer_;
  Type type_;

  // Used to protect everything below.
  mutable base::Lock lock_;
  Ranges<base::TimeDelta> buffered_ranges_;
  bool stopped_;
  // If true we store all read callbacks regardless of our ability
  // to service them.
  bool buffering_;

  typedef std::deque<scoped_refptr<ShellBuffer> > BufferQueue;
  BufferQueue buffer_queue_;

  typedef std::deque<ReadCB> ReadQueue;
  ReadQueue read_queue_;

  DISALLOW_COPY_AND_ASSIGN(ShellDemuxerStream);
};

class MEDIA_EXPORT ShellDemuxer : public Demuxer {
 public:
  ShellDemuxer(const scoped_refptr<base::MessageLoopProxy>& message_loop,
               const scoped_refptr<DataSource>& data_source);
  virtual ~ShellDemuxer();

  // Demuxer implementation.
  virtual void Initialize(DemuxerHost* host,
                          const PipelineStatusCB& status_cb) OVERRIDE;
  virtual void Stop(const base::Closure& callback) OVERRIDE;
  virtual void Seek(base::TimeDelta time, const PipelineStatusCB& cb) OVERRIDE;
  virtual void OnAudioRendererDisabled() OVERRIDE;
  virtual void SetPlaybackRate(float playback_rate) OVERRIDE;
  virtual scoped_refptr<DemuxerStream> GetStream(
      DemuxerStream::Type type) OVERRIDE;
  virtual base::TimeDelta GetStartTime() const OVERRIDE;

  // Issues a task to the demuxer to identify the next buffer of provided type
  // in the stream, allocate memory to contain that buffer, download the bytes
  // in to it, and enqueue the data in the appropriate demuxer stream.
  void Request(DemuxerStream::Type type);

  // The DemuxerStream objects ask their parent ShellDemuxer stream class
  // for these configuration data rather than duplicating in the child classes
  const AudioDecoderConfig& AudioConfig();
  const VideoDecoderConfig& VideoConfig();

  // Provide access to ShellDemuxerStream.
  bool MessageLoopBelongsToCurrentThread() const;

  // Callback from ShellBufferFactory
  void BufferAllocated(scoped_refptr<ShellBuffer> buffer);

 private:
  // Carries out initialization on the demuxer thread.
  void InitializeTask(DemuxerHost* host, const PipelineStatusCB& status_cb);
  void ParseConfigDone(const PipelineStatusCB& status_cb, bool result);
  void RequestTask(DemuxerStream::Type type);
  void DownloadTask(scoped_refptr<ShellBuffer> buffer);
  // Carries out a seek on the demuxer thread.
  void SeekTask(base::TimeDelta time, const PipelineStatusCB& cb);
  // Carries out stopping the demuxer streams on the demuxer thread.
  void StopTask(const base::Closure& callback);

  // callback from DataSourceReader on read error from DataSource. We handle
  // internally and propagate error to DemuxerHost.
  void OnDataSourceReaderError();

  bool WithinEpsilon(const base::TimeDelta& a, const base::TimeDelta& b);

  // methods that perform blocking I/O, and are therefore run on the
  // blocking_thread_
  // download enough of the stream to parse the configuration. returns
  // false on error.
  bool ParseConfigBlocking();

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  DemuxerHost* host_;

  // Thread on which all blocking operations are executed.
  base::Thread blocking_thread_;
  scoped_refptr<DataSource> data_source_;
  scoped_refptr<ShellDataSourceReader> reader_;

  // Even normal beginning-to-end playback results in one seek, to the
  // start time of the media. Before signaling to the pipeline that we
  // have completed seeking, we attempt to preload until we have either
  // kDemuxPreloadTimeMs demuxed, or we have filled our fixed-size demux
  // buffer. At that point we call the seek_cb_ to signal the pipeline
  // that we are prepared to play.
  PipelineStatusCB seek_cb_;

  bool read_has_failed_;

  scoped_refptr<ShellDemuxerStream> audio_demuxer_stream_;
  scoped_refptr<ShellDemuxerStream> video_demuxer_stream_;
  scoped_refptr<ShellParser> parser_;

  base::TimeDelta next_audio_unit_;
  base::TimeDelta next_video_unit_;
  base::TimeDelta highest_video_unit_;
  base::TimeDelta epsilon_;
  base::TimeDelta preload_;
  int video_out_of_order_frames_;

  typedef std::list<scoped_refptr<ShellAU> > AUList;
  AUList active_aus_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_SHELL_DEMUXER_H_
