// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DATA_SOURCE_H_
#define MEDIA_BASE_DATA_SOURCE_H_

#include "base/callback.h"
#include "base/time.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT DataSourceHost {
 public:
  // Set the total size of the media file.
  virtual void SetTotalBytes(int64 total_bytes) = 0;

  // Notify the host that byte range [start,end] has been buffered.
  // TODO(fischman): remove this method when demuxing is push-based instead of
  // pull-based.  http://crbug.com/131444
  virtual void AddBufferedByteRange(int64 start, int64 end) = 0;

  // Notify the host that time range [start,end] has been buffered.
  virtual void AddBufferedTimeRange(base::TimeDelta start,
                                    base::TimeDelta end) = 0;

 protected:
  virtual ~DataSourceHost();
};

class MEDIA_EXPORT DataSource {
 public:
  typedef base::Callback<void(int64, int64)> StatusCallback;
  typedef base::Callback<void(int)> ReadCB;
  static const int kReadError;

  DataSource();

  virtual void set_host(DataSourceHost* host);

  // Reads |size| bytes from |position| into |data|. And when the read is done
  // or failed, |read_cb| is called with the number of bytes read or
  // kReadError in case of error.
  virtual void Read(int64 position, int size, uint8* data,
                    const DataSource::ReadCB& read_cb) = 0;

  // Notifies the DataSource of a change in the current playback rate.
  virtual void SetPlaybackRate(float playback_rate);

  // Stops the DataSource. Once this is called all future Read() calls will
  // return an error.
  virtual void Stop() = 0;

  // Returns true and the file size, false if the file size could not be
  // retrieved.
  virtual bool GetSize(int64* size_out) = 0;

  // Returns true if we are performing streaming. In this case seeking is
  // not possible.
  virtual bool IsStreaming() = 0;

  // Notify the DataSource of the bitrate of the media.
  // Values of |bitrate| <= 0 are invalid and should be ignored.
  virtual void SetBitrate(int bitrate) = 0;

 protected:
  virtual ~DataSource();

  DataSourceHost* host();

 private:
  DataSourceHost* host_;

  DISALLOW_COPY_AND_ASSIGN(DataSource);
};

}  // namespace media

#endif  // MEDIA_BASE_DATA_SOURCE_H_
