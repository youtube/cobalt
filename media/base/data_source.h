// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DATA_SOURCE_H_
#define MEDIA_BASE_DATA_SOURCE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"
#include "media/base/preload.h"

namespace media {

class MEDIA_EXPORT DataSourceHost {
 public:
  virtual ~DataSourceHost();

  // Set the total size of the media file.
  virtual void SetTotalBytes(int64 total_bytes) = 0;

  // Sets the total number of bytes that are buffered on the client and ready to
  // be played.
  virtual void SetBufferedBytes(int64 buffered_bytes) = 0;

  // Sets the flag to indicate current network activity.
  virtual void SetNetworkActivity(bool is_downloading_data) = 0;
};

class MEDIA_EXPORT DataSource : public base::RefCountedThreadSafe<DataSource> {
 public:
  typedef base::Callback<void(int64, int64)> StatusCallback;
  typedef base::Callback<void(size_t)> ReadCallback;
  static const size_t kReadError;

  DataSource();

  virtual void set_host(DataSourceHost* host);

  // Reads |size| bytes from |position| into |data|. And when the read is done
  // or failed, |read_callback| is called with the number of bytes read or
  // kReadError in case of error.
  // TODO(hclam): should change |size| to int! It makes the code so messy
  // with size_t and int all over the place..
  virtual void Read(int64 position, size_t size,
                    uint8* data,
                    const DataSource::ReadCallback& read_callback) = 0;

  // Notifies the DataSource of a change in the current playback rate.
  virtual void SetPlaybackRate(float playback_rate);

  // Stops the DataSource. Once this is called all future Read() calls will
  // return an error.
  virtual void Stop(const base::Closure& callback) = 0;

  // Returns true and the file size, false if the file size could not be
  // retrieved.
  virtual bool GetSize(int64* size_out) = 0;

  // Returns true if we are performing streaming. In this case seeking is
  // not possible.
  virtual bool IsStreaming() = 0;

  // Alert the DataSource that the video preload value has been changed.
  virtual void SetPreload(Preload preload) = 0;

  // Notify the DataSource of the bitrate of the media.
  // Values of |bitrate| <= 0 are invalid and should be ignored.
  virtual void SetBitrate(int bitrate) = 0;

 protected:
  // Only allow scoped_refptr<> to delete DataSource.
  friend class base::RefCountedThreadSafe<DataSource>;
  virtual ~DataSource();

  DataSourceHost* host();

 private:
  DataSourceHost* host_;

  DISALLOW_COPY_AND_ASSIGN(DataSource);
};

}  // namespace media

#endif  // MEDIA_BASE_DATA_SOURCE_H_
