// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_MEMORY_STREAM_H_
#define OTS_MEMORY_STREAM_H_

#if !defined(STARBOARD)
#include <cstring>
#define MEMCPY_OTS_MEMORY_STREAM std::memcpy
#else
#include "starboard/memory.h"
#define MEMCPY_OTS_MEMORY_STREAM SbMemoryCopy
#endif

#include <limits>

#include "opentype-sanitiser.h"

namespace ots {

class MemoryStream : public OTSStream {
 public:
  MemoryStream(void *ptr, size_t length)
      : ptr_(ptr), length_(length), off_(0) {
  }

  virtual bool WriteRaw(const void *data, size_t length) {
    if ((off_ + length > length_) ||
        (length > std::numeric_limits<size_t>::max() - off_)) {
      return false;
    }
    MEMCPY_OTS_MEMORY_STREAM(static_cast<char*>(ptr_) + off_, data, length);
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4267)  // possible loss of data
#endif
    off_ += length;
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
    return true;
  }

  virtual bool Seek(off_t position) {
    if (position < 0) return false;
    if (static_cast<size_t>(position) > length_) return false;
    off_ = position;
    return true;
  }

  virtual off_t Tell() const {
    return off_;
  }

 private:
  void* const ptr_;
  size_t length_;
  off_t off_;
};

class ExpandingMemoryStream : public OTSStream {
 public:
  ExpandingMemoryStream(size_t initial, size_t limit)
      : length_(initial), limit_(limit), off_(0) {
    ptr_ = new uint8_t[length_];
  }

  ~ExpandingMemoryStream() {
    delete[] static_cast<uint8_t*>(ptr_);
  }

  void* get() const {
    return ptr_;
  }

  bool WriteRaw(const void *data, size_t length) {
    if ((off_ + length > length_) ||
        (length > std::numeric_limits<size_t>::max() - off_)) {
      if (length_ == limit_)
        return false;
      size_t new_length = (length_ + 1) * 2;
      if (new_length < length_)
        return false;
      if (new_length > limit_)
        new_length = limit_;
      uint8_t* new_buf = new uint8_t[new_length];
      MEMCPY_OTS_MEMORY_STREAM(new_buf, ptr_, length_);
      length_ = new_length;
      delete[] static_cast<uint8_t*>(ptr_);
      ptr_ = new_buf;
      return WriteRaw(data, length);
    }
    MEMCPY_OTS_MEMORY_STREAM(static_cast<char*>(ptr_) + off_, data, length);
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4267)  // possible loss of data
#endif
    off_ += length;
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
    return true;
  }

  bool Seek(off_t position) {
    if (position < 0) return false;
    if (static_cast<size_t>(position) > length_) return false;
    off_ = position;
    return true;
  }

  off_t Tell() const {
    return off_;
  }

 private:
  void* ptr_;
  size_t length_;
  const size_t limit_;
  off_t off_;
};

}  // namespace ots

#undef MEMCPY_OTS_MEMORY_STREAM

#endif  // OTS_MEMORY_STREAM_H_
