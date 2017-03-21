// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BLINK_MULTIBUFFER_READER_H_
#define COBALT_MEDIA_BLINK_MULTIBUFFER_READER_H_

#include <stdint.h>

#include <limits>
#include <map>
#include <set>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/media/blink/media_blink_export.h"
#include "cobalt/media/blink/multibuffer.h"

namespace media {

// Wrapper for MultiBuffer that offers a simple byte-reading
// interface with prefetch.
class MEDIA_BLINK_EXPORT MultiBufferReader
    : NON_EXPORTED_BASE(public MultiBuffer::Reader) {
 public:
  // Note that |progress_callback| is guaranteed to be called if
  // a redirect happens and the url_data is updated. Otherwise
  // origin checks will become insecure.
  // Users probably want to call SetMaxBuffer & SetPreload after
  // creating the a MultiBufferReader.
  // The progress callback will be called when the "available range"
  // changes. (The number of bytes available for reading before and
  // after the current position.) The arguments for the progress
  // callback is the first byte available (from beginning of file)
  // and the last byte available + 1. Note that there may be other
  // regions of available data in the cache as well.
  // If |end| is not known, use -1.
  MultiBufferReader(
      MultiBuffer* multibuffer, int64_t start, int64_t end,
      const base::Callback<void(int64_t, int64_t)>& progress_callback);

  ~MultiBufferReader() OVERRIDE;

  // Returns number of bytes available for reading. When the rest of the file
  // is available, the number returned will be greater than the number
  // or readable bytes. If an error occurs, -1 is returned.
  int64_t Available() const;

  // Seek to a different position.
  // If there is a pending Wait(), it will be cancelled.
  void Seek(int64_t pos);

  // Returns the current position.
  int64_t Tell() const { return pos_; }

  // Tries to read |len| bytes and advance position.
  // Returns number of bytes read.
  // If there is a pending Wait(), it will be cancelled.
  int64_t TryRead(uint8_t* data, int64_t len);

  // Wait until |len| bytes are available for reading.
  // Returns net::OK if |len| bytes are already available, otherwise it will
  // return net::ERR_IO_PENDING and call |cb| at some point later.
  // |len| must be smaller or equal to max_buffer_forward.
  int Wait(int64_t len, const base::Closure& cb);

  // Set how much data we try to preload into the cache ahead of our current
  // location. Normally, we preload until we have preload_high bytes, then
  // stop until we fall below preload_low bytes. Note that preload can be
  // set higher than max_buffer_forward, but the result is usually that
  // some blocks will be freed between the current position and the preload
  // position.
  void SetPreload(int64_t preload_high, int64_t preload_low);

  // Change how much data we pin to the cache.
  // The range [current_position - backward ... current_position + forward)
  // will be locked in the cache. Calling Wait() or TryRead() with values
  // larger than |forward| is not supported.
  void SetPinRange(int64_t backward, int64_t forward);

  // Set how much memory usage we target. This memory is added to the global
  // LRU and shared between all multibuffers. We may end up using more memory
  // if no memory can be freed due to pinning.
  void SetMaxBuffer(int64_t bytes);

  // Returns true if we are currently loading data.
  bool IsLoading() const;

  // Reader implementation.
  void NotifyAvailableRange(const Interval<MultiBufferBlockId>& range) OVERRIDE;

  // Getters
  int64_t preload_high() const { return preload_high_; }
  int64_t preload_low() const { return preload_low_; }

 private:
  // Returns the block for a particular byte position.
  MultiBufferBlockId block(int64_t byte_pos) const {
    return byte_pos >> multibuffer_->block_size_shift();
  }

  // Returns the block for a particular byte position, rounding up.
  MultiBufferBlockId block_ceil(int64_t byte_pos) const {
    return block(byte_pos + (1LL << multibuffer_->block_size_shift()) - 1);
  }

  // Unpin previous range, then pin the new range.
  void PinRange(MultiBuffer::BlockId begin, MultiBuffer::BlockId end);

  // Check if wait operation can complete now.
  void CheckWait();

  // Recalculate preload_pos_ and update our entry in the multibuffer
  // reader index. Also call CheckWait(). This function is basically
  // called anything changes, like when we get more data or seek to
  // a new position.
  void UpdateInternalState();

  // Update end_ if p-1 contains an end-of-stream block.
  void UpdateEnd(MultiBufferBlockId p);

  // Indirection function used to call callbacks. When we post a callback
  // we indirect it through a weak_ptr and this function to make sure we
  // don't call any callbacks after this object has been destroyed.
  void Call(const base::Closure& cb) const;

  // The multibuffer we're wrapping, not owned.
  MultiBuffer* multibuffer_;

  // We're not interested in reading past this position.
  int64_t end_;

  // Defer reading once we have this much data.
  int64_t preload_high_;
  // Stop deferring once we have this much data.
  int64_t preload_low_;

  // Pin this much data in the cache from the current position.
  int64_t max_buffer_forward_;
  int64_t max_buffer_backward_;

  // The amount of buffer we've added to the global LRU.
  int64_t current_buffer_size_;

  // Currently pinned range.
  Interval<MultiBuffer::BlockId> pinned_range_;

  // Current position in bytes.
  int64_t pos_;

  // [block(pos_)..preload_pos_) are known to be in the cache.
  // preload_pos_ is only allowed to point to a filled
  // cache position if it is equal to end_ or pos_+preload_.
  // This is a pointer to a slot in the cache, so the unit is
  // blocks.
  MultiBufferBlockId preload_pos_;

  // True if we've requested data from the cache by calling WaitFor().
  bool loading_;

  // When Available() > current_wait_size_ we call cb_.
  int64_t current_wait_size_;
  base::Closure cb_;

  // Progress callback.
  base::Callback<void(int64_t, int64_t)> progress_callback_;

  base::WeakPtrFactory<MultiBufferReader> weak_factory_;
};

}  // namespace media

#endif  // COBALT_MEDIA_BLINK_MULTIBUFFER_READER_H_
