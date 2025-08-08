// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PPB_BUFFER_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PPB_BUFFER_IMPL_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/containers/checked_iterators.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_buffer_api.h"

namespace content {

class PPB_Buffer_Impl : public ppapi::Resource,
                        public ppapi::thunk::PPB_Buffer_API {
 public:
  static PP_Resource Create(PP_Instance instance, uint32_t size);
  static scoped_refptr<PPB_Buffer_Impl> CreateResource(PP_Instance instance,
                                                       uint32_t size);

  PPB_Buffer_Impl(const PPB_Buffer_Impl&) = delete;
  PPB_Buffer_Impl& operator=(const PPB_Buffer_Impl&) = delete;

  virtual PPB_Buffer_Impl* AsPPB_Buffer_Impl();

  const base::UnsafeSharedMemoryRegion& shared_memory() const {
    return shared_memory_;
  }
  uint32_t size() const { return size_; }

  // Resource overrides.
  ppapi::thunk::PPB_Buffer_API* AsPPB_Buffer_API() override;

  // PPB_Buffer_API implementation.
  PP_Bool Describe(uint32_t* size_in_bytes) override;
  PP_Bool IsMapped() override;
  void* Map() override;
  void Unmap() override;

  // Trusted.
  int32_t GetSharedMemory(base::UnsafeSharedMemoryRegion** shm) override;

 private:
  ~PPB_Buffer_Impl() override;

  explicit PPB_Buffer_Impl(PP_Instance instance);
  bool Init(uint32_t size);

  base::UnsafeSharedMemoryRegion shared_memory_;
  base::WritableSharedMemoryMapping shared_mapping_;
  uint32_t size_;
  int map_count_;
};

// Ensures that the given buffer is mapped, and returns it to its previous
// mapped state in the destructor.
class BufferAutoMapper {
 public:
  using iterator = base::CheckedContiguousIterator<const uint8_t>;

  explicit BufferAutoMapper(ppapi::thunk::PPB_Buffer_API* api);

  BufferAutoMapper(const BufferAutoMapper&) = delete;
  BufferAutoMapper& operator=(const BufferAutoMapper&) = delete;

  ~BufferAutoMapper();

  // Will be NULL on failure to map.
  const uint8_t* data() const { return data_; }
  size_t size() const { return size_; }

  // Iterate buffer as bytes up to the end of its logical size.
  iterator begin() const {
    // SAFETY: The implementer of `PPB_Buffer_API` is responsible for
    // guaranteeing that when `Map()` returns a non-null pointer, it points to
    // at least the number of bytes returned in `Describe()`'s outparam. This
    // memory must also remain valid for the lifetime of this mapper.
    return UNSAFE_BUFFERS(iterator(data(), data() + size()));
  }
  iterator end() const {
    // SAFETY: As in `begin()` above.
    return UNSAFE_BUFFERS(iterator(data(), data() + size(), data() + size()));
  }

 private:
  raw_ptr<ppapi::thunk::PPB_Buffer_API> api_;

  bool needs_unmap_;

  raw_ptr<const uint8_t> data_;
  uint32_t size_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PPB_BUFFER_IMPL_H_
