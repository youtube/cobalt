// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FILTER_COLLECTION_H_
#define MEDIA_BASE_FILTER_COLLECTION_H_

#include <list>

#include "base/ref_counted.h"
#include "media/base/filters.h"
#include "media/base/filter_factories.h"

namespace media {

// This is a collection of Filter objects used to form a media playback
// pipeline. See src/media/base/pipeline.h for more information.
class FilterCollection {
 public:
  FilterCollection();
  ~FilterCollection();

  // DataSourceFactory accessor methods.
  // FilterCollection takes ownership of the factory here.
  void SetDataSourceFactory(DataSourceFactory* factory);
  DataSourceFactory* GetDataSourceFactory();

  // Adds a filter to the collection.
  void AddDemuxer(Demuxer* filter);
  void AddVideoDecoder(VideoDecoder* filter);
  void AddAudioDecoder(AudioDecoder* filter);
  void AddVideoRenderer(VideoRenderer* filter);
  void AddAudioRenderer(AudioRenderer* filter);

  // Is the collection empty?
  bool IsEmpty() const;

  // Remove remaining filters.
  void Clear();

  // Selects a filter of the specified type from the collection.
  // If the required filter cannot be found, NULL is returned.
  // If a filter is returned it is removed from the collection.
  void SelectDemuxer(scoped_refptr<Demuxer>* filter_out);
  void SelectVideoDecoder(scoped_refptr<VideoDecoder>* filter_out);
  void SelectAudioDecoder(scoped_refptr<AudioDecoder>* filter_out);
  void SelectVideoRenderer(scoped_refptr<VideoRenderer>* filter_out);
  void SelectAudioRenderer(scoped_refptr<AudioRenderer>* filter_out);

 private:
  // Identifies the type of filter implementation. Each filter has to be one of
  // the following types. This is used to mark, identify, and support
  // downcasting of different filter types stored in the filters_ list.
  enum FilterType {
    DEMUXER,
    AUDIO_DECODER,
    VIDEO_DECODER,
    AUDIO_RENDERER,
    VIDEO_RENDERER,
  };

  // List of filters managed by this collection.
  typedef std::pair<FilterType, scoped_refptr<Filter> > FilterListElement;
  typedef std::list<FilterListElement> FilterList;
  FilterList filters_;
  scoped_ptr<DataSourceFactory> data_source_factory_;

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
