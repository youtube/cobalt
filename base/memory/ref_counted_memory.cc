// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted_memory.h"

RefCountedMemory::RefCountedMemory() {
}

RefCountedMemory::~RefCountedMemory() {
}

const unsigned char* RefCountedStaticMemory::front() const {
  return data_;
}

size_t RefCountedStaticMemory::size() const {
  return length_;
}

RefCountedBytes::RefCountedBytes() {
}

RefCountedBytes::RefCountedBytes(const std::vector<unsigned char>& initializer)
    : data(initializer) {
}

RefCountedBytes* RefCountedBytes::TakeVector(
    std::vector<unsigned char>* to_destroy) {
  RefCountedBytes* bytes = new RefCountedBytes;
  bytes->data.swap(*to_destroy);
  return bytes;
}

const unsigned char* RefCountedBytes::front() const {
  // STL will assert if we do front() on an empty vector, but calling code
  // expects a NULL.
  return size() ? &data.front() : NULL;
}

size_t RefCountedBytes::size() const {
  return data.size();
}

RefCountedBytes::~RefCountedBytes() {
}
