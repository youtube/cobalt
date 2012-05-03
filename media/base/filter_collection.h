// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FILTER_COLLECTION_H_
#define MEDIA_BASE_FILTER_COLLECTION_H_

#include <list>

#include "base/memory/ref_counted.h"
#include "media/base/demuxer.h"
#include "media/base/filters.h"

namespace media {

class AudioDecoder;
class VideoDecoder;

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
  void AddAudioRenderer(AudioRenderer* filter);
  void AddVideoRenderer(VideoRenderer* filter);

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
  void SelectAudioRenderer(scoped_refptr<AudioRenderer>* filter_out);
  void SelectVideoRenderer(scoped_refptr<VideoRenderer>* filter_out);

 private:
  // Identifies the type of filter implementation. Each filter has to be one of
  // the following types. This is used to mark, identify, and support
  // downcasting of different filter types stored in the filters_ list.
  enum FilterType {
    AUDIO_RENDERER,
    VIDEO_RENDERER,
  };

  // List of filters managed by this collection.
  typedef std::pair<FilterType, scoped_refptr<Filter> > FilterListElement;
  typedef std::list<FilterListElement> FilterList;
  FilterList filters_;
  scoped_refptr<Demuxer> demuxer_;
  std::list<scoped_refptr<AudioDecoder> > audio_decoders_;
  std::list<scoped_refptr<VideoDecoder> > video_decoders_;

  // Helper function that adds a filter to the filter list.
  void AddFilter(FilterType filter_type, Filter* filter);

  // Helper function for SelectXXX() methods. It manages the
  // downcasting and mapping between FilterType and Filter class.
  template<FilterType filter_type, typename F>
  void SelectFilter(scoped_refptr<F>* filter_out);

  // Helper function that searches the filters list for a specific filter type.
  void SelectFilter(FilterType filter_type, scoped_refptr<Filter>* filter_out);

  DISALLOW_COPY_AND_ASSIGN(FilterCollection);
};

}  // namespace media

#endif  // MEDIA_BASE_FILTER_COLLECTION_H_
