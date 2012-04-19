// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_FRAME_READER_H_
#define NET_SPDY_SPDY_FRAME_READER_H_
#pragma once

#include "base/basictypes.h"
#include "base/string_piece.h"
#include "net/base/net_export.h"

namespace net {

// Used for reading SPDY frames. Though there isn't really anything terribly
// SPDY-specific here, it's a helper class that's useful when doing SPDY
// framing.
//
// To use, simply construct a SpdyFramerReader using the underlying buffer that
// you'd like to read fields from, then call one of the Read*() methods to
// actually do some reading.
//
// This class keeps an internal iterator to keep track of what's already been
// read and each successive Read*() call automatically increments said iterator
// on success. On failure, internal state of the SpdyFrameReader should not be
// trusted and it is up to the caller to throw away the failed instance and
// handle the error as appropriate. None of the Read*() methods should ever be
// called after failure, as they will also fail immediately.
class NET_EXPORT_PRIVATE SpdyFrameReader {
 public:
  // Caller must provide an underlying buffer to work on.
  SpdyFrameReader(const char* data, const size_t len);

  // Empty destructor.
  ~SpdyFrameReader() {}

  // Reads a 16-bit unsigned integer into the given output parameter.
  // Forwards the internal iterater on success.
  // Returns true on success, false otherwise.
  bool ReadUInt16(uint16* result);

  // Reads a 32-bit unsigned integer into the given output parameter.
  // Forwards the internal iterater on success.
  // Returns true on success, false otherwise.
  bool ReadUInt32(uint32* result);

  // Reads a string prefixed with 16-bit length into the given output parameter.
  //
  // NOTE: Does not copy but rather references strings in the underlying buffer.
  // This should be kept in mind when handling memory management!
  //
  // Forwards the internal iterater on success.
  // Returns true on success, false otherwise.
  bool ReadStringPiece16(base::StringPiece* result);

  // Reads a string prefixed with 32-bit length into the given output parameter.
  //
  // NOTE: Does not copy but rather references strings in the underlying buffer.
  // This should be kept in mind when handling memory management!
  //
  // Forwards the internal iterater on success.
  // Returns true on success, false otherwise.
  bool ReadStringPiece32(base::StringPiece* result);

  // Reads a given number of bytes into the given buffer. The buffer
  // must be of adequate size.
  // Forwards the internal iterater on success.
  // Returns true on success, false otherwise.
  bool ReadBytes(void* result, size_t size);

  // Returns true if the entirety of the underlying buffer has been read via
  // Read*() calls.
  bool IsDoneReading() const;

 private:
  // Returns true if the underlying buffer has enough room to read the given
  // amount of bytes.
  bool CanRead(size_t bytes) const;

  // To be called when a read fails for any reason.
  void OnFailure();

  // The data buffer that we're reading from.
  const char* data_;

  // The length of the data buffer that we're reading from.
  const size_t len_;

  // The location of the next read from our data buffer.
  size_t ofs_;
};

}  // namespace net

#endif  // NET_SPDY_SPDY_FRAME_READER_H_
