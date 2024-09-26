// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>

#include "components/reporting/resources/resource_managed_buffer.h"

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"

#include "components/reporting/util/status.h"

namespace reporting {

ResourceManagedBuffer::ResourceManagedBuffer(
    scoped_refptr<ResourceManager> memory_resource)
    : memory_resource_(memory_resource) {}

ResourceManagedBuffer::~ResourceManagedBuffer() {
  Clear();
}

Status ResourceManagedBuffer::Allocate(size_t size) {
  // Lose whatever was allocated before (if any).
  Clear();
  // Register with resource management.
  if (!memory_resource_->Reserve(size)) {
    return Status(error::RESOURCE_EXHAUSTED,
                  "Not enough memory for the buffer");
  }
  // Commit memory allocation.
  size_ = size;
  buffer_ = std::make_unique<char[]>(size);
  return Status::StatusOK();
}

void ResourceManagedBuffer::Clear() {
  if (buffer_) {
    buffer_.reset();
  }
  if (size_ > 0) {
    memory_resource_->Discard(size_);
    size_ = 0;
  }
}

char* ResourceManagedBuffer::at(size_t pos) {
  DCHECK_LT(pos, size_);
  return buffer_.get() + pos;
}

size_t ResourceManagedBuffer::size() const {
  return size_;
}

bool ResourceManagedBuffer::empty() const {
  return size_ == 0;
}

}  // namespace reporting
