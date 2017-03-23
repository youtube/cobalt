// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_MEDIA_URL_DEMUXER_H_
#define COBALT_MEDIA_BASE_MEDIA_URL_DEMUXER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/media/base/demuxer.h"
#include "googleurl/src/gurl.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

// Class that saves a URL for later retrieval. To be used in conjunction with
// the MediaPlayerRenderer.
//
// Its primary purpose is to act as a dummy Demuxer, when there is no need
// for DemuxerStreams (e.g. in the MediaPlayerRenderer case). For the most part,
// its implementation of the Demuxer are NOPs that return the default values and
// fire any provided callbacks immediately.
//
// If Pipeline where to be refactored to use a DemuxerStreamProvider instead of
// a Demuxer, MediaUrlDemuxer should be refactored to inherit directly from
// DemuxerStreamProvider.
class MEDIA_EXPORT MediaUrlDemuxer : public Demuxer {
 public:
  MediaUrlDemuxer(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const GURL& url);
  ~MediaUrlDemuxer() OVERRIDE;

  // DemuxerStreamProvider interface.
  DemuxerStream* GetStream(DemuxerStream::Type type) OVERRIDE;
  GURL GetUrl() const OVERRIDE;
  DemuxerStreamProvider::Type GetType() const OVERRIDE;

  // Demuxer interface.
  std::string GetDisplayName() const OVERRIDE;
  void Initialize(DemuxerHost* host, const PipelineStatusCB& status_cb,
                  bool enable_text_tracks) OVERRIDE;
  void StartWaitingForSeek(base::TimeDelta seek_time) OVERRIDE;
  void CancelPendingSeek(base::TimeDelta seek_time) OVERRIDE;
  void Seek(base::TimeDelta time, const PipelineStatusCB& status_cb) OVERRIDE;
  void Stop() OVERRIDE;
  void AbortPendingReads() OVERRIDE;
  base::TimeDelta GetStartTime() const OVERRIDE;
  base::Time GetTimelineOffset() const OVERRIDE;
  int64_t GetMemoryUsage() const OVERRIDE;
  void OnEnabledAudioTracksChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta currTime) OVERRIDE;
  void OnSelectedVideoTrackChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta currTime) OVERRIDE;

 private:
  GURL url_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MediaUrlDemuxer);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_MEDIA_URL_DEMUXER_H_
