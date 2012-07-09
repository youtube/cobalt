// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/bit_reader.h"

namespace media {

BitReader::BitReader()
    : data_(NULL),
      bytes_left_(0),
      position_(0),
      curr_byte_(0),
      num_remaining_bits_in_curr_byte_(0) {
}

BitReader::BitReader(const uint8* data, off_t size) {
  Initialize(data, size);
}

BitReader::~BitReader() {}

void BitReader::Initialize(const uint8* data, off_t size) {
  DCHECK(data != NULL || size == 0);  // Data cannot be NULL if size is not 0.

  data_ = data;
  bytes_left_ = size;
  position_ = 0;
  num_remaining_bits_in_curr_byte_ = 0;

  UpdateCurrByte();
}

void BitReader::UpdateCurrByte() {
  DCHECK_EQ(num_remaining_bits_in_curr_byte_, 0);

  if (bytes_left_ < 1)
    return;

  // Load a new byte and advance pointers.
  curr_byte_ = *data_;
  ++data_;
  --bytes_left_;
  num_remaining_bits_in_curr_byte_ = 8;
}

bool BitReader::SkipBits(int num_bits) {
  int dummy;
  const int kDummySize = static_cast<int>(sizeof(dummy)) * 8;

  while (num_bits > kDummySize) {
    if (ReadBits(kDummySize, &dummy)) {
      num_bits -= kDummySize;
    } else {
      return false;
    }
  }

  return ReadBits(num_bits, &dummy);
}

off_t BitReader::Tell() const {
  return position_;
}

off_t BitReader::NumBitsLeft() const {
  return (num_remaining_bits_in_curr_byte_ + bytes_left_ * 8);
}

bool BitReader::HasMoreData() const {
  return num_remaining_bits_in_curr_byte_ != 0;
}

}  // namespace media
