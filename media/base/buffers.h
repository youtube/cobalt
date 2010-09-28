// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines various types of timestamped media buffers used for transporting
// data between filters.  Every buffer contains a timestamp in microseconds
// describing the relative position of the buffer within the media stream, and
// the duration in microseconds for the length of time the buffer will be
// rendered.
//
// Timestamps are derived directly from the encoded media file and are commonly
// known as the presentation timestamp (PTS).  Durations are a best-guess and
// are usually derived from the sample/frame rate of the media file.
//
// Due to encoding and transmission errors, it is not guaranteed that timestamps
// arrive in a monotonically increasing order nor that the next timestamp will
// be equal to the previous timestamp plus the duration.
//
// In the ideal scenario for a 25fps movie, buffers are timestamped as followed:
//
//               Buffer0      Buffer1      Buffer2      ...      BufferN
// Timestamp:        0us      40000us      80000us      ...   (N*40000)us
// Duration*:    40000us      40000us      40000us      ...       40000us
//
//  *25fps = 0.04s per frame = 40000us per frame

#ifndef MEDIA_BASE_BUFFERS_H_
#define MEDIA_BASE_BUFFERS_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/time.h"

namespace media {

class StreamSample : public base::RefCountedThreadSafe<StreamSample> {
 public:
  // Constant timestamp value to indicate an invalid or missing timestamp.
  static const base::TimeDelta kInvalidTimestamp;

  // Returns the timestamp of this buffer in microseconds.
  base::TimeDelta GetTimestamp() const {
    return timestamp_;
  }

  // Returns the duration of this buffer in microseconds.
  base::TimeDelta GetDuration() const {
    return duration_;
  }

  // Indicates that the sample is the last one in the stream. This method is
  // pure virtual so implementors can decide when to declare end of stream
  // depending on specific data.
  virtual bool IsEndOfStream() const = 0;

  // Indicates that this sample is discontinuous from the previous one, for
  // example, following a seek.
  bool IsDiscontinuous() const {
    return discontinuous_;
  }

  // Sets the timestamp of this buffer in microseconds.
  void SetTimestamp(const base::TimeDelta& timestamp) {
    timestamp_ = timestamp;
  }

  // Sets the duration of this buffer in microseconds.
  void SetDuration(const base::TimeDelta& duration) {
    duration_ = duration;
  }

  // Sets the value returned by IsDiscontinuous().
  void SetDiscontinuous(bool discontinuous) {
    discontinuous_ = discontinuous;
  }

 protected:
  friend class base::RefCountedThreadSafe<StreamSample>;
  StreamSample();
  virtual ~StreamSample();

  base::TimeDelta timestamp_;
  base::TimeDelta duration_;
  bool discontinuous_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StreamSample);
};


class Buffer : public StreamSample {
 public:
  // Returns a read only pointer to the buffer data.
  virtual const uint8* GetData() const = 0;

  // Returns the size of valid data in bytes.
  virtual size_t GetDataSize() const = 0;

  // If there's no data in this buffer, it represents end of stream.
  virtual bool IsEndOfStream() const { return GetData() == NULL; }

 protected:
  virtual ~Buffer() {}
};


class WritableBuffer : public Buffer  {
 public:
  // Returns a read-write pointer to the buffer data.
  virtual uint8* GetWritableData() = 0;

  // Updates the size of valid data in bytes, which must be less than or equal
  // to GetBufferSize().
  virtual void SetDataSize(size_t data_size) = 0;

  // Returns the size of the underlying buffer.
  virtual size_t GetBufferSize() const = 0;

 protected:
  virtual ~WritableBuffer() {}
};

}  // namespace media

#endif  // MEDIA_BASE_BUFFERS_H_
