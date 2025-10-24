// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted_memory.h"

#include <utility>

#include "base/check_op.h"
#include "base/memory/read_only_shared_memory_region.h"

namespace base {

bool RefCountedMemory::Equals(
    const scoped_refptr<RefCountedMemory>& other) const {
  return other.get() && size() == other->size() &&
         (size() == 0 || (memcmp(front(), other->front(), size()) == 0));
}

RefCountedMemory::RefCountedMemory() = default;

RefCountedMemory::~RefCountedMemory() = default;

const unsigned char* RefCountedStaticMemory::front() const {
  return data_;
}

size_t RefCountedStaticMemory::size() const {
  return length_;
}

RefCountedStaticMemory::~RefCountedStaticMemory() = default;

RefCountedBytes::RefCountedBytes() = default;

RefCountedBytes::RefCountedBytes(const std::vector<unsigned char>& initializer)
    : data_(initializer) {}

RefCountedBytes::RefCountedBytes(base::span<const unsigned char> initializer)
    : data_(initializer.begin(), initializer.end()) {}

RefCountedBytes::RefCountedBytes(const unsigned char* p, size_t size)
    : data_(p, p + size) {}

RefCountedBytes::RefCountedBytes(size_t size) : data_(size, 0) {}

scoped_refptr<RefCountedBytes> RefCountedBytes::TakeVector(
    std::vector<unsigned char>* to_destroy) {
  auto bytes = MakeRefCounted<RefCountedBytes>();
  bytes->data_.swap(*to_destroy);
  return bytes;
}

const unsigned char* RefCountedBytes::front() const {
  // STL will assert if we do front() on an empty vector, but calling code
  // expects a NULL.
  return size() ? &data_.front() : nullptr;
}

size_t RefCountedBytes::size() const {
  return data_.size();
}

RefCountedBytes::~RefCountedBytes() = default;

RefCountedString::RefCountedString() = default;

RefCountedString::~RefCountedString() = default;

RefCountedString::RefCountedString(std::string str) : data_(std::move(str)) {}

const unsigned char* RefCountedString::front() const {
  return data_.empty() ? nullptr
                       : reinterpret_cast<const unsigned char*>(data_.data());
}

size_t RefCountedString::size() const {
  return data_.size();
}

RefCountedString16::RefCountedString16() = default;

RefCountedString16::~RefCountedString16() = default;

RefCountedString16::RefCountedString16(std::u16string str)
    : data_(std::move(str)) {}

const unsigned char* RefCountedString16::front() const {
  return reinterpret_cast<const unsigned char*>(data_.data());
}

size_t RefCountedString16::size() const {
  return data_.size() * sizeof(char16_t);
}

RefCountedSharedMemoryMapping::RefCountedSharedMemoryMapping(
    ReadOnlySharedMemoryMapping mapping)
    : mapping_(std::move(mapping)), size_(mapping_.size()) {
  DCHECK_GT(size_, 0U);
}

RefCountedSharedMemoryMapping::~RefCountedSharedMemoryMapping() = default;

const unsigned char* RefCountedSharedMemoryMapping::front() const {
  return static_cast<const unsigned char*>(mapping_.memory());
}

size_t RefCountedSharedMemoryMapping::size() const {
  return size_;
}

// static
scoped_refptr<RefCountedSharedMemoryMapping>
RefCountedSharedMemoryMapping::CreateFromWholeRegion(
    const ReadOnlySharedMemoryRegion& region) {
  ReadOnlySharedMemoryMapping mapping = region.Map();
  if (!mapping.IsValid())
    return nullptr;
  return MakeRefCounted<RefCountedSharedMemoryMapping>(std::move(mapping));
}

}  //  namespace base
