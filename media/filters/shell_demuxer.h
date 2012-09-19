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

#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer.h"
#include "media/base/video_decoder_config.h"

namespace media {

class DecoderBuffer;
class ShellDemuxer;

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

  // functions for ShellDemuxer
  bool HasPendingReads();
  void EnqueueBuffer(scoped_refptr<DecoderBuffer> buffer);
  // Signals to empty the buffer queue and mark next packet as discontinuous.
  void FlushBuffers();
  // Empties the queues and ignores any additional calls to Read().
  void Stop();

 private:
  // Carries out enqueuing a pending read on the demuxer thread.
  void ReadTask(const ReadCB& read_cb);

  // Attempts to fulfill a single pending read by dequeueing a buffer and read
  // callback pair and executing the callback. The calling function must
  // acquire lock_ before calling this function.
  void FulfillPendingReadLocked();

  // Returns all enqueued buffers to the buffer factory. Assumes lock_ is
  // acquired
  void ClearBufferQueueLocked();

  ShellDemuxer* demuxer_;
  Type type_;
  bool stopped_;

  // used to protect buffer_queue_, read_queue_, and stopped_.
  mutable base::Lock lock_;

  typedef std::deque<scoped_refptr<DecoderBuffer> > BufferQueue;
  BufferQueue buffer_queue_;

  typedef std::deque<ReadCB> ReadQueue;
  ReadQueue read_queue_;

  DISALLOW_COPY_AND_ASSIGN(ShellDemuxerStream);
};

// audio and video decoder configs are supplied with these structures as
// extra data, allowing downstream decoders to supply the hardware with the
// necessary information.
struct AudioConfigExtraData {
 public:
  uint8 aac_object_type;
  uint8 aac_sampling_frequency_index;
  uint8 channel_count;
};

// Unfortunately these extra data structs are confined to value containers only
// right now, as they are byte-copied blindly into and out of the config
// objects, meaning that any pointer we allocate inside of these structures
// will be leaked. We need to store one sps and one pps for downstream decoders
// and each can be at most 64K size in size, as the standard calls for 16 bits
// to specify their size. 64K seems pretty unreasonable for a SPS or PPS
// structure, and in practice most are much smaller. So rather than require
// 128K of room in our extra data structure we limit these sizes to sane values
// and then treat larger values as decode errors. Tune these values to
// play all YT videos.
static const int kShellMaxSPSSize = 256;
static const int kShellMaxPPSSize = 256;

struct VideoConfigExtraData {
 public:
  uint32 num_ref_frames;
  uint8 sps[kShellMaxSPSSize];
  uint32 sps_size;
  uint8 pps[kShellMaxPPSSize];
  uint32 pps_size;
  uint8 nal_header_size;
};

class MEDIA_EXPORT ShellDemuxer : public Demuxer {
 public:
  ShellDemuxer(MessageLoop* message_loop,
               const scoped_refptr<DataSource>& data_source);

  // Posts a task to perform additional demuxing.
  virtual void PostDemuxTask();

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
  virtual int GetBitrate() OVERRIDE;

  // The DemuxerStream objects ask their parent ShellDemuxer stream class
  // for these configuration data rather than duplicating in the child classes
  const AudioDecoderConfig& audio_decoder_config();
  const VideoDecoderConfig& video_decoder_config();

  // Provide access to ShellDemuxerStream.
  MessageLoop* message_loop() { return message_loop_; }

 private:
  // Carries out initialization on the demuxer thread.
  void InitializeTask(DemuxerHost* host, const PipelineStatusCB& status_cb);
  // running on the demuxer's own message loop, block until read is complete
  // returns number of bytes read or -1 on error
  int BlockingRead(int64 position, int size, uint8* data);

  // Carries out a seek on the demuxer thread.
  void SeekTask(base::TimeDelta time, const PipelineStatusCB& cb);

  // Carries out demuxing and satisfying stream reads on the demuxer thread.
  void DemuxTask();

  // Carries out stopping the demuxer streams on the demuxer thread.
  void StopTask(const base::Closure& callback);

  // demux a single item from the input stream. Returns false on error or EOS
  bool DemuxSingleItem();
  bool DemuxFLVTag();

  // parse the AVCConfigRecord pointed to by avc_config, of size record_size,
  // and set the values in video_decoder_config_ if successful,
  // also sets video_decoder_config_known_ flag
  void ParseAVCConfigRecord(uint8* avc_config, size_t record_size);

  // Download individual video NALUs from an FLV AVCVIDEOPACKET tag and enqueue
  // them into our video DemuxerStream. Returns false on download error.
  bool ExtractAndEnqueueVideoNALUs(uint64_t stream_position,
                                   size_t tag_avc_data_size,
                                   base::TimeDelta timestamp);

  // returns true if either DemuxerStream objects are waiting on a read
  bool StreamsHavePendingReads();

  MessageLoop* message_loop_;
  scoped_refptr<DataSource> data_source_;
  DemuxerHost* host_;

  // BlockingRead Support
  base::WaitableEvent blocking_read_event_;
  bool read_has_failed_;
  int last_read_bytes_;
  // callback, returns number of bytes read
  int WaitForRead();
  void SignalReadCompleted(int size);

  scoped_refptr<ShellDemuxerStream> audio_demuxer_stream_;
  scoped_refptr<ShellDemuxerStream> video_demuxer_stream_;

  // general state demux variables
  uint8 nal_header_size_;

  // mp4-specific state demux variables
  bool is_mp4_;

  // flv-specific state demux variables
  bool is_flv_;
  uint32 flv_tag_size_;

  // NOTE: if we're going to be writing these outside of InitializeTask() then
  // we'll need thread protection
  bool audio_decoder_config_known_;
  AudioDecoderConfig audio_decoder_config_;
  bool video_decoder_config_known_;
  VideoDecoderConfig video_decoder_config_;
  int video_buffer_padding_;

  uint64 read_position_;

  // deferrred loading of metadata, this flag is true when available
  bool metadata_known_;
  base::TimeDelta start_time_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_SHELL_DEMUXER_H_
