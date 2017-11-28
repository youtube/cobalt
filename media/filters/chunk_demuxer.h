// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_CHUNK_DEMUXER_H_
#define MEDIA_FILTERS_CHUNK_DEMUXER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/synchronization/lock.h"
#include "media/base/byte_queue.h"
#include "media/base/demuxer.h"
#include "media/base/ranges.h"
#include "media/base/stream_parser.h"
#include "media/filters/source_buffer_stream.h"

namespace media {

class ChunkDemuxerStream;
class FFmpegURLProtocol;

// Demuxer implementation that allows chunks of media data to be passed
// from JavaScript to the media stack.
class MEDIA_EXPORT ChunkDemuxer : public Demuxer {
 public:
  enum Status {
    kOk,              // ID added w/o error.
    kNotSupported,    // Type specified is not supported.
    kReachedIdLimit,  // Reached ID limit. We can't handle any more IDs.
  };

  typedef base::Callback<void(const std::string& type,
                              scoped_array<uint8> init_data,
                              int init_data_size)> NeedKeyCB;

  // |open_cb| Run when Initialize() is called to signal that the demuxer
  //   is ready to receive media data via AppenData().
  // |need_key_cb| Run when the demuxer determines that an encryption key is
  //   needed to decrypt the content.
  // |log_cb| Run when parsing error messages need to be logged to the error
  //   console.
  ChunkDemuxer(const base::Closure& open_cb, const NeedKeyCB& need_key_cb,
               const LogCB& log_cb);

  // Demuxer implementation.
  virtual void Initialize(DemuxerHost* host,
                          const PipelineStatusCB& cb) override;
  virtual void Stop(const base::Closure& callback) override;
  virtual void Seek(base::TimeDelta time, const PipelineStatusCB&  cb) override;
  virtual void OnAudioRendererDisabled() override;
  virtual scoped_refptr<DemuxerStream> GetStream(
      DemuxerStream::Type type) override;
  virtual base::TimeDelta GetStartTime() const override;

  // Methods used by an external object to control this demuxer.
  void StartWaitingForSeek();
  void CancelPendingSeek();

  // Registers a new |id| to use for AppendData() calls. |type| indicates
  // the MIME type for the data that we intend to append for this ID.
  // kOk is returned if the demuxer has enough resources to support another ID
  //    and supports the format indicated by |type|.
  // kNotSupported is returned if |type| is not a supported format.
  // kReachedIdLimit is returned if the demuxer cannot handle another ID right
  //    now.
  Status AddId(const std::string& id, const std::string& type,
               std::vector<std::string>& codecs);

  // Removed an ID & associated resources that were previously added with
  // AddId().
  void RemoveId(const std::string& id);

  // Gets the currently buffered ranges for the specified ID.
  Ranges<base::TimeDelta> GetBufferedRanges(const std::string& id) const;

  // Appends media data to the source buffer associated with |id|. Returns
  // false if this method is called in an invalid state.
  bool AppendData(const std::string& id, const uint8* data, size_t length);

  // Aborts parsing the current segment and reset the parser to a state where
  // it can accept a new segment.
  void Abort(const std::string& id);

  // Get the duration of the media. It can be an inferred duration set by the
  // class or can be a duration explicitly set by script.
  double GetDuration() const;

  // Notifies the demuxer that the duration of the media has changed to
  // |duration|.
  void SetDuration(double duration);

  // Sets a time |offset| to be applied to subsequent buffers appended to the
  // source buffer assicated with |id|. Returns true if the offset is set
  // properly, false if the offset cannot be applied because we're in the
  // middle of parsing a media segment.
  bool SetTimestampOffset(const std::string& id, base::TimeDelta offset);

  // Signals an EndOfStream request.
  // Returns false if called in an unexpected state or if there is a gap between
  // the current position and the end of the buffered data.
  bool EndOfStream(PipelineStatus status);
  void Shutdown();

 protected:
  virtual ~ChunkDemuxer();

 private:
  enum State {
    WAITING_FOR_INIT,
    INITIALIZING,
    INITIALIZED,
    ENDED,
    PARSE_ERROR,
    SHUTDOWN,
  };

  void ChangeState_Locked(State new_state);

  // Reports an error and puts the demuxer in a state where it won't accept more
  // data.
  void ReportError_Locked(PipelineStatus error);

  // Returns true if any stream has seeked to a time without buffered data.
  bool IsSeekPending_Locked() const;

  // Returns true if all streams can successfully call EndOfStream,
  // false if any can not.
  bool CanEndOfStream_Locked() const;

  double GetDuration_Locked() const;

  // StreamParser callbacks.
  void OnStreamParserInitDone(bool success, base::TimeDelta duration);
  bool OnNewConfigs(bool has_audio, bool has_video,
                    const AudioDecoderConfig& audio_config,
                    const VideoDecoderConfig& video_config);
  bool OnAudioBuffers(const StreamParser::BufferQueue& buffers);
  bool OnVideoBuffers(const StreamParser::BufferQueue& buffers);
  bool OnNeedKey(const std::string& type,
                 scoped_array<uint8> init_data,
                 int init_data_size);
  void OnNewMediaSegment(const std::string& source_id,
                         base::TimeDelta start_timestamp);
  void OnEndOfMediaSegment(const std::string& source_id);

  // Computes the intersection between the video & audio
  // buffered ranges.
  Ranges<base::TimeDelta> ComputeIntersection() const;

  // Applies |time_offset| to the timestamps of |buffers|.
  void AdjustBufferTimestamps(const StreamParser::BufferQueue& buffers,
                              base::TimeDelta timestamp_offset);

  // Returns true if |source_id| is valid, false otherwise.
  bool IsValidId(const std::string& source_id) const;

  // Increases |duration_| if the newly appended |buffers| exceed the current
  // |duration_|. The |duration_| is set to the end buffered timestamp of
  // |stream|.
  void IncreaseDurationIfNecessary(
      const StreamParser::BufferQueue& buffers,
      const scoped_refptr<ChunkDemuxerStream>& stream);

  // Decreases |duration_| if the buffered region is less than |duration_| when
  // EndOfStream() is called.
  void DecreaseDurationIfNecessary();

  // Sets |duration_| to |new_duration| and notifies |host_|.
  void UpdateDuration(base::TimeDelta new_duration);

  // Returns the ranges representing the buffered data in the demuxer.
  Ranges<base::TimeDelta> GetBufferedRanges() const;

  mutable base::Lock lock_;
  State state_;

  DemuxerHost* host_;
  base::Closure open_cb_;
  NeedKeyCB need_key_cb_;
  // Callback used to report error strings that can help the web developer
  // figure out what is wrong with the content.
  LogCB log_cb_;

  PipelineStatusCB init_cb_;
  PipelineStatusCB seek_cb_;
  bool delayed_audio_seek_;

  scoped_refptr<ChunkDemuxerStream> audio_;
  scoped_refptr<ChunkDemuxerStream> video_;

  base::TimeDelta duration_;

  // The duration passed to the last SetDuration(). If
  // SetDuration() is never called or an AppendData() call or
  // a EndOfStream() call changes |duration_|, then this
  // variable is set to < 0 to indicate that the |duration_| represents
  // the actual duration instead of a user specified value.
  double user_specified_duration_;

  typedef std::map<std::string, StreamParser*> StreamParserMap;
  StreamParserMap stream_parser_map_;

  // Contains state belonging to a source id.
  struct SourceInfo {
    base::TimeDelta timestamp_offset;
    bool can_update_offset;
  };
  typedef std::map<std::string, SourceInfo> SourceInfoMap;
  SourceInfoMap source_info_map_;

  // Used to ensure that (1) config data matches the type and codec provided in
  // AddId(), (2) only 1 audio and 1 video sources are added, and (3) ids may be
  // removed with RemoveID() but can not be re-added (yet).
  std::string source_id_audio_;
  std::string source_id_video_;

  DISALLOW_COPY_AND_ASSIGN(ChunkDemuxer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_CHUNK_DEMUXER_H_
