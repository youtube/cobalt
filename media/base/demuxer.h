// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DEMUXER_H_
#define MEDIA_BASE_DEMUXER_H_

#include "base/memory/ref_counted.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"

// TODO(acolwell): Remove include once DataSource & Preload are moved
// out of "media/base/filters.h" and Stop() is converted to use new callbacks.
#include "media/base/filters.h"
#include "media/base/pipeline_status.h"

namespace media {

class FilterHost;

class MEDIA_EXPORT Demuxer
    : public base::RefCountedThreadSafe<Demuxer> {
 public:
  // Sets the private member |host_|. This is the first method called by
  // the FilterHost after a demuxer is created.  The host holds a strong
  // reference to the demuxer.  The reference held by the host is guaranteed
  // to be released before the host object is destroyed by the pipeline.
  //
  // TODO(acolwell): Change this to a narrow DemuxerHost interface.
  virtual void set_host(FilterHost* host);

  // The pipeline playback rate has been changed.  Demuxers may implement this
  // method if they need to respond to this call.
  virtual void SetPlaybackRate(float playback_rate);

  // Carry out any actions required to seek to the given time, executing the
  // callback upon completion.
  virtual void Seek(base::TimeDelta time, const PipelineStatusCB& callback);

  // The pipeline is being stopped either as a result of an error or because
  // the client called Stop().
  virtual void Stop(const base::Closure& callback);

  // This method is called from the pipeline when the audio renderer
  // is disabled. Demuxers can ignore the notification if they do not
  // need to react to this event.
  //
  // TODO(acolwell): Change to generic DisableStream(DemuxerStream::Type).
  virtual void OnAudioRendererDisabled();

  // Returns the given stream type, or NULL if that type is not present.
  virtual scoped_refptr<DemuxerStream> GetStream(DemuxerStream::Type type) = 0;

  // Alert the Demuxer that the video preload value has been changed.
  virtual void SetPreload(Preload preload) = 0;

  // Returns the starting time for the media file.
  virtual base::TimeDelta GetStartTime() const = 0;

  // Returns the content bitrate. May be obtained from container or
  // approximated. Returns 0 if it is unknown.
  virtual int GetBitrate() = 0;

 protected:
  Demuxer();
  FilterHost* host() { return host_; }

  friend class base::RefCountedThreadSafe<Demuxer>;
  virtual ~Demuxer();

 private:
  FilterHost* host_;

  DISALLOW_COPY_AND_ASSIGN(Demuxer);
};

}  // namespace media

#endif  // MEDIA_BASE_DEMUXER_H_
