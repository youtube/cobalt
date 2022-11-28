// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_DATA_SOURCE_H_
#define COBALT_MEDIA_BASE_DATA_SOURCE_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "cobalt/media/base/media_export.h"
#include "starboard/types.h"

namespace cobalt {
namespace media {

class MEDIA_EXPORT DataSource {
 public:
  typedef base::Callback<void(int64_t, int64_t)> StatusCallback;
  typedef base::Callback<void(int)> ReadCB;
  typedef base::Callback<void(bool)> DownloadingStatusCB;

  enum { kReadError = -1, kAborted = -2 };

  DataSource();
  virtual ~DataSource();

  // Reads |size| bytes from |position| into |data|. And when the read is done
  // or failed, |read_cb| is called with the number of bytes read or
  // kReadError in case of error.
  virtual void Read(int64_t position, int size, uint8_t* data,
                    const DataSource::ReadCB& read_cb) = 0;

  // Stops the DataSource. Once this is called all future Read() calls will
  // return an error.
  virtual void Stop() = 0;

  // Similar to Stop(), but only aborts current reads and not future reads.
  virtual void Abort() = 0;

  // Returns true and the file size, false if the file size could not be
  // retrieved.
  virtual bool GetSize(int64_t* size_out) = 0;

  // Sets a callback to receive downloading status.
  virtual void SetDownloadingStatusCB(
      const DownloadingStatusCB& downloading_status_cb) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DataSource);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_DATA_SOURCE_H_
