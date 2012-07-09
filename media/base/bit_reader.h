// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BIT_READER_H_
#define MEDIA_BASE_BIT_READER_H_

#include <sys/types.h>
#include <algorithm>
#include <climits>

#include "base/basictypes.h"
#include "base/logging.h"
#include "media/base/media_export.h"


namespace media {

// A class to read bit streams.
// Classes inherited this class can override its UpdateCurrByte function
// to support specific escape mechanism, which is widely used by streaming
// formats like H.264 Annex B.
class MEDIA_EXPORT BitReader {
 public:
  BitReader();
  BitReader(const uint8* data, off_t size);
  virtual ~BitReader();

  // Initialize the reader to start reading at |data|, |size| being size
  // of |data| in bytes.
  void Initialize(const uint8* data, off_t size);

  // Read |num_bits| next bits from stream and return in |*out|, first bit
  // from the stream starting at |num_bits| position in |*out|.
  // |num_bits| cannot be larger than the bits the type can hold.
  // Return false if the given number of bits cannot be read (not enough
  // bits in the stream), true otherwise. When return false, the stream will
  // enter a state where further ReadBits operations will always return false
  // unless |num_bits| is 0. The type |T| has to be a primitive integer type.
  template<typename T>
  bool ReadBits(int num_bits, T *out) {
    DCHECK(num_bits <= static_cast<int>(sizeof(T) * CHAR_BIT));

    *out = 0;
    position_ += num_bits;

    while (num_remaining_bits_in_curr_byte_ != 0 && num_bits != 0) {
      int bits_to_take = std::min(num_remaining_bits_in_curr_byte_, num_bits);
      *out = (*out << bits_to_take) +
          (curr_byte_ >> (num_remaining_bits_in_curr_byte_ - bits_to_take));
      num_bits -= bits_to_take;
      num_remaining_bits_in_curr_byte_ -= bits_to_take;
      curr_byte_ &= (1 << num_remaining_bits_in_curr_byte_) - 1;

      if (num_remaining_bits_in_curr_byte_ == 0)
        UpdateCurrByte();
    }

    if (num_bits == 0)
      return true;

    *out = 0;
    num_remaining_bits_in_curr_byte_ = 0;
    bytes_left_ = 0;

    return false;
  }

  bool SkipBits(int num_bits);

  // Return the current position in the stream in unit of bit.
  // This includes the skipped escape bytes if there are any.
  off_t Tell() const;

  // Return the number of bits left in the stream.
  // This doesn't take any escape sequence into account.
  off_t NumBitsLeft() const;

  bool HasMoreData() const;

 protected:
  // Advance to the next byte, loading it into curr_byte_.
  // This function can be overridden to support specific escape mechanism.
  // If the num_remaining_bits_in_curr_byte_ is 0 after this function returns,
  // the stream has reached the end.
  virtual void UpdateCurrByte();

  // Pointer to the next unread (not in curr_byte_) byte in the stream.
  const uint8* data_;

  // Bytes left in the stream (without the curr_byte_).
  off_t bytes_left_;

  // Current position in bits.
  off_t position_;

  // Contents of the current byte; first unread bit starting at position
  // 8 - num_remaining_bits_in_curr_byte_ from MSB.
  uint8 curr_byte_;

  // Number of bits remaining in curr_byte_
  int num_remaining_bits_in_curr_byte_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BitReader);
};

}  // namespace media

#endif  // MEDIA_BASE_BIT_READER_H_
