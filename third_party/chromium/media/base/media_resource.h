// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_RESOURCE_H_
#define MEDIA_BASE_MEDIA_RESOURCE_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/media_url_params.h"

namespace media {

// Abstract class that defines how to retrieve "media resources" in
// DemuxerStream form (for most cases) or URL form (for the MediaPlayerRenderer
// case).
//
// The derived classes must return a non-null value for the getter method
// associated with their type, and return a null/empty value for other getters.
class MEDIA_EXPORT MediaResource {
 public:
  enum Type {
    STREAM,  // Indicates GetAllStreams() or GetFirstStream() should be used
    URL,     // Indicates GetUrl() should be used
  };

  MediaResource();

  MediaResource(const MediaResource&) = delete;
  MediaResource& operator=(const MediaResource&) = delete;

  virtual ~MediaResource();

  virtual MediaResource::Type GetType() const;

  // For Type::STREAM:
  // Returns a collection of available DemuxerStream objects.
  //   NOTE: Once a DemuxerStream pointer is returned from GetStream it is
  //   guaranteed to stay valid for as long as the Demuxer/MediaResource
  //   is alive. But make no assumption that once GetStream returned a non-null
  //   pointer for some stream type then all subsequent calls will also return
  //   non-null pointer for the same stream type. In MSE Javascript code can
  //   remove SourceBuffer from a MediaSource at any point and this will make
  //   some previously existing streams inaccessible/unavailable.
  virtual std::vector<DemuxerStream*> GetAllStreams() = 0;

  // A helper function that return the first stream of the given `type` if one
  // exists or a null pointer if there is no streams of that type.
  DemuxerStream* GetFirstStream(DemuxerStream::Type type);

  // For Type::URL:
  //   Returns the URL parameters of the media to play. Empty URLs are legal,
  //   and should be handled appropriately by the caller.
  // Other types:
  //   Should not be called.
  virtual const MediaUrlParams& GetMediaUrlParams() const;

  // This method is only used with the MediaUrlDemuxer, to propagate duration
  // changes coming from the MediaPlayerRendereClient.
  //
  // This method could be refactored if WMPI was aware of the concrete type of
  // Demuxer* it is dealing with.
  virtual void ForwardDurationChangeToDemuxerHost(base::TimeDelta duration);
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_RESOURCE_H_
