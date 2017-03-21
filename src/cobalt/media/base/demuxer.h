// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_DEMUXER_H_
#define COBALT_MEDIA_BASE_DEMUXER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cobalt/media/base/data_source.h"
#include "cobalt/media/base/demuxer_stream.h"
#include "cobalt/media/base/demuxer_stream_provider.h"
#include "cobalt/media/base/eme_constants.h"
#include "cobalt/media/base/media_export.h"
#include "cobalt/media/base/media_track.h"
#include "cobalt/media/base/pipeline_status.h"
#include "cobalt/media/base/ranges.h"

namespace media {

class TextTrackConfig;
class MediaTracks;

class MEDIA_EXPORT DemuxerHost {
 public:
  // Notify the host that buffered time ranges have changed. Note that buffered
  // time ranges can grow (when new media data is appended), but they can also
  // shrink (when buffering reaches limit capacity and some buffered data
  // becomes evicted, e.g. due to MSE GC algorithm, or by explicit removal of
  // ranges directed by MSE web app).
  virtual void OnBufferedTimeRangesChanged(
      const Ranges<base::TimeDelta>& ranges) = 0;

  // Sets the duration of the media in microseconds.
  // Duration may be kInfiniteDuration if the duration is not known.
  virtual void SetDuration(base::TimeDelta duration) = 0;

  // Stops execution of the pipeline due to a fatal error. Do not call this
  // method with PIPELINE_OK. Stopping is not immediate so demuxers must be
  // prepared to soft fail on subsequent calls. E.g., if Demuxer::Seek() is
  // called after an unrecoverable error the provided PipelineStatusCB must be
  // called with an error.
  virtual void OnDemuxerError(PipelineStatus error) = 0;

  // Add |text_stream| to the collection managed by the text renderer.
  virtual void AddTextStream(DemuxerStream* text_stream,
                             const TextTrackConfig& config) = 0;

  // Remove |text_stream| from the presentation.
  virtual void RemoveTextStream(DemuxerStream* text_stream) = 0;

 protected:
  virtual ~DemuxerHost();
};

class MEDIA_EXPORT Demuxer : public DemuxerStreamProvider {
 public:
  // A new potentially encrypted stream has been parsed.
  // First parameter - The type of initialization data.
  // Second parameter - The initialization data associated with the stream.
  typedef base::Callback<void(EmeInitDataType type,
                              const std::vector<uint8_t>& init_data)>
      EncryptedMediaInitDataCB;

  // Notifies demuxer clients that media track configuration has been updated
  // (e.g. the initial stream metadata has been parsed successfully, or a new
  // init segment has been parsed successfully in MSE case).
  typedef base::Callback<void(scoped_ptr<MediaTracks>)> MediaTracksUpdatedCB;

  Demuxer();
  ~Demuxer() OVERRIDE;

  // Returns the name of the demuxer for logging purpose.
  virtual std::string GetDisplayName() const = 0;

  // Completes initialization of the demuxer.
  //
  // The demuxer does not own |host| as it is guaranteed to outlive the
  // lifetime of the demuxer. Don't delete it!  |status_cb| must only be run
  // after this method has returned.
  virtual void Initialize(DemuxerHost* host, const PipelineStatusCB& status_cb,
                          bool enable_text_tracks) = 0;

  // Aborts any pending read operations that the demuxer is involved with; any
  // read aborted will be aborted with a status of kAborted.
  virtual void AbortPendingReads() = 0;

  // Indicates that a new Seek() call is on its way. Implementations may abort
  // pending reads and future Read() calls may return kAborted until Seek() is
  // executed. |seek_time| is the presentation timestamp of the new Seek() call.
  //
  // In actual use, this call occurs on the main thread while Seek() is called
  // on the media thread. StartWaitingForSeek() can be used to synchronize the
  // two.
  //
  // StartWaitingForSeek() MUST be called before Seek().
  virtual void StartWaitingForSeek(base::TimeDelta seek_time) = 0;

  // Indicates that the current Seek() operation is obsoleted by a new one.
  // Implementations can expect that StartWaitingForSeek() will be called
  // when the current seek operation completes.
  //
  // Like StartWaitingForSeek(), CancelPendingSeek() is called on the main
  // thread. Ordering with respect to the to-be-canceled Seek() is not
  // guaranteed. Regardless of ordering, implementations may abort pending reads
  // and may return kAborted from future Read() calls, until after
  // StartWaitingForSeek() and the following Seek() call occurs.
  //
  // |seek_time| should match that passed to the next StartWaitingForSeek(), but
  // may not if the seek target changes again before the current seek operation
  // completes or is aborted.
  virtual void CancelPendingSeek(base::TimeDelta seek_time) = 0;

  // Carry out any actions required to seek to the given time, executing the
  // callback upon completion.
  virtual void Seek(base::TimeDelta time,
                    const PipelineStatusCB& status_cb) = 0;

  // Stops this demuxer.
  //
  // After this call the demuxer may be destroyed. It is illegal to call any
  // method (including Stop()) after a demuxer has stopped.
  virtual void Stop() = 0;

  // Returns the starting time for the media file; it's always positive.
  virtual base::TimeDelta GetStartTime() const = 0;

  // Returns Time represented by presentation timestamp 0.
  // If the timstamps are not associated with a Time, then
  // a null Time is returned.
  virtual base::Time GetTimelineOffset() const = 0;

  // Returns the memory usage in bytes for the demuxer.
  virtual int64_t GetMemoryUsage() const = 0;

  virtual void OnEnabledAudioTracksChanged(
      const std::vector<MediaTrack::Id>& track_ids,
      base::TimeDelta currTime) = 0;

  // |track_ids| is either empty or contains a single video track id.
  virtual void OnSelectedVideoTrackChanged(
      const std::vector<MediaTrack::Id>& track_ids,
      base::TimeDelta currTime) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Demuxer);
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_DEMUXER_H_
