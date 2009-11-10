// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_IO_BUFFER_H_
#define NET_BASE_IO_BUFFER_H_

#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

namespace net {

// This is a simple wrapper around a buffer that provides ref counting for
// easier asynchronous IO handling.
class IOBuffer : public base::RefCountedThreadSafe<IOBuffer> {
 public:
  IOBuffer() : data_(NULL) {}
  explicit IOBuffer(int buffer_size);

  char* data() { return data_; }

 protected:
  friend class base::RefCountedThreadSafe<IOBuffer>;

  // Only allow derived classes to specify data_.
  // In all other cases, we own data_, and must delete it at destruction time.
  explicit IOBuffer(char* data) : data_(data) {}

  virtual ~IOBuffer() {
    delete[] data_;
  }

  char* data_;
};

// This version stores the size of the buffer so that the creator of the object
// doesn't have to keep track of that value.
// NOTE: This doesn't mean that we want to stop sending the size as an explicit
// argument to IO functions. Please keep using IOBuffer* for API declarations.
class IOBufferWithSize : public IOBuffer {
 public:
  explicit IOBufferWithSize(int size) : IOBuffer(size), size_(size) {}

  int size() const { return size_; }

 private:
  ~IOBufferWithSize() {}

  int size_;
};

// This is a read only IOBuffer.  The data is stored in a string and
// the IOBuffer interface does not provide a proper way to modify it.
class StringIOBuffer : public IOBuffer {
 public:
  explicit StringIOBuffer(const std::string& s)
      : IOBuffer(static_cast<char*>(NULL)),
        string_data_(s) {
    data_ = const_cast<char*>(string_data_.data());
  }

  int size() const { return string_data_.size(); }

 private:
  ~StringIOBuffer() {
    // We haven't allocated the buffer, so remove it before the base class
    // destructor tries to delete[] it.
    data_ = NULL;
  }

  std::string string_data_;
};

// This version wraps an existing IOBuffer and provides convenient functions
// to progressively read all the data.
class DrainableIOBuffer : public IOBuffer {
 public:
  DrainableIOBuffer(IOBuffer* base, int size)
      : IOBuffer(base->data()), base_(base), size_(size), used_(0) {}

  // DidConsume() changes the |data_| pointer so that |data_| always points
  // to the first unconsumed byte.
  void DidConsume(int bytes) { SetOffset(used_ + bytes); }

  // Returns the number of unconsumed bytes.
  int BytesRemaining() const { return size_ - used_; }

  // Returns the number of consumed bytes.
  int BytesConsumed() const { return used_; }

  // Seeks to an arbitrary point in the buffer. The notion of bytes consumed
  // and remaining are updated appropriately.
  void SetOffset(int bytes);

  int size() const { return size_; }

 private:
  ~DrainableIOBuffer() {
    // The buffer is owned by the |base_| instance.
    data_ = NULL;
  }

  scoped_refptr<IOBuffer> base_;
  int size_;
  int used_;
};

// This version provides a resizable buffer and a changeable offset.
class GrowableIOBuffer : public IOBuffer {
 public:
  GrowableIOBuffer() : IOBuffer(), capacity_(0), offset_(0) {}

  // realloc memory to the specified capacity.
  void SetCapacity(int capacity);
  int capacity() { return capacity_; }

  // |offset| moves the |data_| pointer, allowing "seeking" in the data.
  void set_offset(int offset);
  int offset() { return offset_; }

  int RemainingCapacity() { return capacity_ - offset_; }
  char* StartOfBuffer() { return real_data_.get(); }

 private:
  ~GrowableIOBuffer() { data_ = NULL; }

  scoped_ptr_malloc<char> real_data_;
  int capacity_;
  int offset_;
};

// This class allows the creation of a temporary IOBuffer that doesn't really
// own the underlying buffer. Please use this class only as a last resort.
// A good example is the buffer for a synchronous operation, where we can be
// sure that nobody is keeping an extra reference to this object so the lifetime
// of the buffer can be completely managed by its intended owner.
class WrappedIOBuffer : public IOBuffer {
 public:
  explicit WrappedIOBuffer(const char* data)
      : IOBuffer(const_cast<char*>(data)) {}

 protected:
  ~WrappedIOBuffer() {
    data_ = NULL;
  }
};

}  // namespace net

#endif  // NET_BASE_IO_BUFFER_H_
