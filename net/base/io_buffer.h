// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_IO_BUFFER_H_
#define NET_BASE_IO_BUFFER_H_
#pragma once

#include <string>

#include "base/pickle.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

namespace net {

// This is a simple wrapper around a buffer that provides ref counting for
// easier asynchronous IO handling.
class IOBuffer : public base::RefCountedThreadSafe<IOBuffer> {
 public:
  IOBuffer();
  explicit IOBuffer(int buffer_size);

  char* data() { return data_; }

 protected:
  friend class base::RefCountedThreadSafe<IOBuffer>;

  // Only allow derived classes to specify data_.
  // In all other cases, we own data_, and must delete it at destruction time.
  explicit IOBuffer(char* data);

  virtual ~IOBuffer();

  char* data_;
};

// This version stores the size of the buffer so that the creator of the object
// doesn't have to keep track of that value.
// NOTE: This doesn't mean that we want to stop sending the size as an explicit
// argument to IO functions. Please keep using IOBuffer* for API declarations.
class IOBufferWithSize : public IOBuffer {
 public:
  explicit IOBufferWithSize(int size);

  int size() const { return size_; }

 private:
  virtual ~IOBufferWithSize();

  int size_;
};

// This is a read only IOBuffer.  The data is stored in a string and
// the IOBuffer interface does not provide a proper way to modify it.
class StringIOBuffer : public IOBuffer {
 public:
  explicit StringIOBuffer(const std::string& s);

  int size() const { return string_data_.size(); }

 private:
  virtual ~StringIOBuffer();

  std::string string_data_;
};

// This version wraps an existing IOBuffer and provides convenient functions
// to progressively read all the data.
class DrainableIOBuffer : public IOBuffer {
 public:
  DrainableIOBuffer(IOBuffer* base, int size);

  // DidConsume() changes the |data_| pointer so that |data_| always points
  // to the first unconsumed byte.
  void DidConsume(int bytes);

  // Returns the number of unconsumed bytes.
  int BytesRemaining() const;

  // Returns the number of consumed bytes.
  int BytesConsumed() const;

  // Seeks to an arbitrary point in the buffer. The notion of bytes consumed
  // and remaining are updated appropriately.
  void SetOffset(int bytes);

  int size() const { return size_; }

 private:
  virtual ~DrainableIOBuffer();

  scoped_refptr<IOBuffer> base_;
  int size_;
  int used_;
};

// This version provides a resizable buffer and a changeable offset.
class GrowableIOBuffer : public IOBuffer {
 public:
  GrowableIOBuffer();

  // realloc memory to the specified capacity.
  void SetCapacity(int capacity);
  int capacity() { return capacity_; }

  // |offset| moves the |data_| pointer, allowing "seeking" in the data.
  void set_offset(int offset);
  int offset() { return offset_; }

  int RemainingCapacity();
  char* StartOfBuffer();

 private:
  virtual ~GrowableIOBuffer();

  scoped_ptr_malloc<char> real_data_;
  int capacity_;
  int offset_;
};

// This versions allows a pickle to be used as the storage for a write-style
// operation, avoiding an extra data copy.
class PickledIOBuffer : public IOBuffer {
 public:
  PickledIOBuffer();

  Pickle* pickle() { return &pickle_; }

  // Signals that we are done writing to the picke and we can use it for a
  // write-style IO operation.
  void Done();

 private:
  virtual ~PickledIOBuffer();

  Pickle pickle_;
};

// This class allows the creation of a temporary IOBuffer that doesn't really
// own the underlying buffer. Please use this class only as a last resort.
// A good example is the buffer for a synchronous operation, where we can be
// sure that nobody is keeping an extra reference to this object so the lifetime
// of the buffer can be completely managed by its intended owner.
class WrappedIOBuffer : public IOBuffer {
 public:
  explicit WrappedIOBuffer(const char* data);

 protected:
  virtual ~WrappedIOBuffer();
};

}  // namespace net

#endif  // NET_BASE_IO_BUFFER_H_
