// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FILTER_COLLECTION_H_
#define MEDIA_BASE_FILTER_COLLECTION_H_

#include <list>

#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"

namespace media {

class AudioDecoder;
class AudioRenderer;
class Demuxer;
class VideoDecoder;
class VideoRenderer;

// This is a collection of Filter objects used to form a media playback
// pipeline. See src/media/base/pipeline.h for more information.
class MEDIA_EXPORT FilterCollection {
 public:
  FilterCollection();
  ~FilterCollection();

  // Demuxer accessor methods.
  void SetDemuxer(const scoped_refptr<Demuxer>& demuxer);
  const scoped_refptr<Demuxer>& GetDemuxer();

  // Adds a filter to the collection.
  void AddAudioDecoder(AudioDecoder* audio_decoder);
  void AddVideoDecoder(VideoDecoder* video_decoder);
  void AddAudioRenderer(AudioRenderer* audio_renderer);
  void AddVideoRenderer(VideoRenderer* video_renderer);

  // Is the collection empty?
  bool IsEmpty() const;

  // Remove remaining filters.
  void Clear();

  // Selects a filter of the specified type from the collection.
  // If the required filter cannot be found, NULL is returned.
  // If a filter is returned it is removed from the collection.
  // Filters are selected in FIFO order.
  void SelectAudioDecoder(scoped_refptr<AudioDecoder>* out);
  void SelectVideoDecoder(scoped_refptr<VideoDecoder>* out);
  void SelectAudioRenderer(scoped_refptr<AudioRenderer>* out);
  void SelectVideoRenderer(scoped_refptr<VideoRenderer>* out);

 private:
  scoped_refptr<Demuxer> demuxer_;
  std::list<scoped_refptr<AudioDecoder> > audio_decoders_;
  std::list<scoped_refptr<VideoDecoder> > video_decoders_;
  std::list<scoped_refptr<AudioRenderer> > audio_renderers_;
  std::list<scoped_refptr<VideoRenderer> > video_renderers_;

  DISALLOW_COPY_AND_ASSIGN(FilterCollection);
};

}  // namespace media

#endif  // MEDIA_BASE_FILTER_COLLECTION_H_
