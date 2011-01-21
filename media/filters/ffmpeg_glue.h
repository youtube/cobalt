// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// FFmpegGlue is an adapter for FFmpeg's URLProtocol interface that allows us to
// use a DataSource implementation with FFmpeg. For convenience we use FFmpeg's
// av_open_input_file function, which analyzes the filename given to it and
// automatically initializes the appropriate URLProtocol.
//
// Since the DataSource is already open by time we call av_open_input_file, we
// need a way for av_open_input_file to find the correct DataSource instance.
// The solution is to maintain a map of "filenames" to DataSource instances,
// where filenames are actually just a unique identifier.  For simplicity,
// FFmpegGlue is registered as an HTTP handler and generates filenames based on
// the memory address of the DataSource, i.e., http://0xc0bf4870.  Since there
// may be multiple FFmpegDemuxers active at one time, FFmpegGlue is a
// thread-safe singleton.
//
// Usage: FFmpegDemuxer adds the DataSource to FFmpegGlue's map and is given a
// filename to pass to av_open_input_file.  FFmpegDemuxer calls
// av_open_input_file with the filename, which results in FFmpegGlue returning
// the DataSource as a URLProtocol instance to FFmpeg.  Since FFmpegGlue is only
// needed for opening files, when av_open_input_file returns FFmpegDemuxer
// removes the DataSource from FFmpegGlue's map.

#ifndef MEDIA_FILTERS_FFMPEG_GLUE_H_
#define MEDIA_FILTERS_FFMPEG_GLUE_H_

#include <map>
#include <string>

#include "base/singleton.h"
#include "base/synchronization/lock.h"

namespace media {

class FFmpegURLProtocol {
 public:
  FFmpegURLProtocol() {
  }

  virtual ~FFmpegURLProtocol() {
  }

  // Read the given amount of bytes into data, returns the number of bytes read
  // if successful, kReadError otherwise.
  virtual int Read(int size, uint8* data) = 0;

  // Returns true and the current file position for this file, false if the
  // file position could not be retrieved.
  virtual bool GetPosition(int64* position_out) = 0;

  // Returns true if the file position could be set, false otherwise.
  virtual bool SetPosition(int64 position) = 0;

  // Returns true and the file size, false if the file size could not be
  // retrieved.
  virtual bool GetSize(int64* size_out) = 0;

  // Returns false if this protocol supports random seeking.
  virtual bool IsStreaming() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FFmpegURLProtocol);
};

class FFmpegGlue {
 public:
  // Returns the singleton instance.
  static FFmpegGlue* GetInstance();

  // Adds a FFmpegProtocol to the FFmpeg glue layer and returns a unique string
  // that can be passed to FFmpeg to identify the data source.
  std::string AddProtocol(FFmpegURLProtocol* protocol);

  // Removes a FFmpegProtocol from the FFmpeg glue layer.  Using strings from
  // previously added FFmpegProtocols will no longer work.
  void RemoveProtocol(FFmpegURLProtocol* protocol);

  // Assigns the FFmpegProtocol identified with by the given key to
  // |protocol|, or assigns NULL if no such FFmpegProtocol could be found.
  void GetProtocol(const std::string& key,
                   FFmpegURLProtocol** protocol);

 private:
  // Only allow Singleton to create and delete FFmpegGlue.
  friend struct DefaultSingletonTraits<FFmpegGlue>;
  FFmpegGlue();
  virtual ~FFmpegGlue();

  // Returns the unique key for this data source, which can be passed to
  // av_open_input_file as the filename.
  std::string GetProtocolKey(FFmpegURLProtocol* protocol);

  // Mutual exclusion while adding/removing items from the map.
  base::Lock lock_;

  // Map between keys and FFmpegProtocol references.
  typedef std::map<std::string, FFmpegURLProtocol*> ProtocolMap;
  ProtocolMap protocols_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegGlue);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_GLUE_H_
