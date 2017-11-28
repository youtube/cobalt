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

class ShellDemuxerStream : public DemuxerStream {
 public:
  ShellDemuxerStream(ShellDemuxer* demuxer, Type type);

  // DemuxerStream implementation
  virtual void Read(const ReadCB& read_cb) override;
  virtual const AudioDecoderConfig& audio_decoder_config() override;
  virtual const VideoDecoderConfig& video_decoder_config() override;
  virtual Type type() override;
  virtual void EnableBitstreamConverter() override;
  virtual bool StreamWasEncrypted() const override;

  // Functions used by ShellDemuxer
  Ranges<base::TimeDelta> GetBufferedRanges();
  void EnqueueBuffer(scoped_refptr<DecoderBuffer> buffer);
  void FlushBuffers();
  void Stop();
  base::TimeDelta GetLastBufferTimestamp() const;

 private:
  // The Ranges object doesn't offer a complement object so we rebuild
  // enqueued ranges from the union of all of the buffers in the queue.
  // Call me whenever _removing_ data from buffer_queue_.
  void RebuildEnqueuedRanges_Locked();

  // non-owning pointer to avoid circular reference
  ShellDemuxer* demuxer_;
  Type type_;

  // Used to protect everything below.
  mutable base::Lock lock_;
  // Keeps track of all time ranges this object has seen since creation.
  // The demuxer uses these ranges to update the pipeline about what data
  // it has demuxed.
  Ranges<base::TimeDelta> buffered_ranges_;
  // The last timestamp of buffer enqueued. This is used in two places:
  //   1. Used with the timestamp of the current frame to calculate the
  //      buffer range.
  //   2. Used by the demuxer to deteminate what type of frame to get next.
  base::TimeDelta last_buffer_timestamp_;
  bool stopped_;

  typedef std::deque<scoped_refptr<DecoderBuffer> > BufferQueue;
  BufferQueue buffer_queue_;

  typedef std::deque<ReadCB> ReadQueue;
  ReadQueue read_queue_;

  DISALLOW_COPY_AND_ASSIGN(ShellDemuxerStream);
};

class MEDIA_EXPORT ShellDemuxer : public Demuxer {
 public:
  ShellDemuxer(const scoped_refptr<base::MessageLoopProxy>& message_loop,
               DataSource* data_source);
  virtual ~ShellDemuxer();

  // Demuxer implementation.
  virtual void Initialize(DemuxerHost* host,
                          const PipelineStatusCB& status_cb) override;
  virtual void Stop(const base::Closure& callback) override;
  virtual void Seek(base::TimeDelta time, const PipelineStatusCB& cb) override;
  virtual void OnAudioRendererDisabled() override;
  virtual void SetPlaybackRate(float playback_rate) override;
  virtual scoped_refptr<DemuxerStream> GetStream(
      DemuxerStream::Type type) override;
  virtual base::TimeDelta GetStartTime() const override;

  // TODO: Consider move the following functions to private section.

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
  void BufferAllocated(scoped_refptr<DecoderBuffer> buffer);

 private:
  void ParseConfigDone(const PipelineStatusCB& status_cb,
                       PipelineStatus status);
  void DataSourceStopped(const base::Closure& callback);

  // methods that perform blocking I/O, and are therefore run on the
  // blocking_thread_
  // download enough of the stream to parse the configuration. returns
  // false on error.
  PipelineStatus ParseConfigBlocking(const PipelineStatusCB& status_cb);
  void RequestTask(DemuxerStream::Type type);
  void DownloadTask(scoped_refptr<DecoderBuffer> buffer);
  void IssueNextRequestTask();
  void SeekTask(base::TimeDelta time, const PipelineStatusCB& cb);

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  DemuxerHost* host_;

  // Thread on which all blocking operations are executed.
  base::Thread blocking_thread_;
  DataSource* data_source_;
  scoped_refptr<ShellDataSourceReader> reader_;

  bool stopped_;
  bool flushing_;

  scoped_refptr<ShellDemuxerStream> audio_demuxer_stream_;
  scoped_refptr<ShellDemuxerStream> video_demuxer_stream_;
  scoped_refptr<ShellParser> parser_;

  scoped_refptr<ShellAU> requested_au_;
  bool audio_reached_eos_;
  bool video_reached_eos_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_SHELL_DEMUXER_H_
