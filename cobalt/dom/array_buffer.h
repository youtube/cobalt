/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_DOM_ARRAY_BUFFER_H_
#define COBALT_DOM_ARRAY_BUFFER_H_

#include <vector>

#include "base/memory/scoped_ptr.h"  // For scoped_array
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class ArrayBuffer : public script::Wrappable {
 public:
  // To explicitly express that a specific ArrayBuffer should be allocated from
  // the heap.
  enum AllocationType { kFromHeap };

  class Cache;

  // Optional Allocator to be used to allocate/free memory for ArrayBuffer.
  // ArrayBuffer will allocate from the heap if an Allocator is not provided.
  // This is because ArrayBuffer allocates its memory on the heap by default and
  // ArrayBuffers may occupy a lot of memory.  It is possible to provide an
  // allocator on some platforms so ArrayBuffer can possibly use memory that is
  // not part of the heap.
  class Allocator {
   public:
    virtual ~Allocator() {}
    virtual void* Allocate(size_t size) = 0;
    virtual void Free(void* p) = 0;
  };

  // This class manages the internal buffer of an ArrayBuffer.  It deals the
  // fact that the buffer can be allocated from an Allocator or from the heap.
  class Data {
   public:
    Data(script::EnvironmentSettings* settings, size_t size);
    Data(script::EnvironmentSettings* settings, const uint8* data, size_t size);
    Data(scoped_array<uint8> data, size_t size);

    ~Data();

    uint8* data() const;
    size_t size() const { return size_; }
    // Move the ArrayBuffer allocated on the heap to memory allocated using the
    // provided allocator.  It returns true if such move is successful or if the
    // ArrayBuffer has already been offloaded.
    bool Offload();
    bool offloaded() const { return offloaded_; }

   private:
    void Initialize(script::EnvironmentSettings* settings, size_t size);

    Allocator* allocator_;
    Cache* cache_;
    // True only if |data_| is allocated by |allocator_|.
    bool offloaded_;
    uint8* data_;
    size_t size_;

    DISALLOW_COPY_AND_ASSIGN(Data);
  };

  class Cache {
   public:
    explicit Cache(size_t maximum_size_in_main_memory);

    void Register(Data* data);
    void Unregister(Data* data);
    void ReportUsage(const Data* data);

   private:
    void TryToOffload();

    size_t total_size_in_main_memory_;
    size_t maximum_size_in_main_memory_;
    std::vector<Data*> datas_;
  };

  ArrayBuffer(script::EnvironmentSettings* settings, uint32 length);
  ArrayBuffer(script::EnvironmentSettings* settings, const uint8* data,
              uint32 length);
  // This is for use by AudioBuffer as we do want to ensure decoded audio data
  // stay in main memory.
  ArrayBuffer(script::EnvironmentSettings* settings,
              AllocationType allocation_type, scoped_array<uint8> data,
              uint32 length);

  uint32 byte_length() const { return static_cast<uint32>(data_.size()); }
  scoped_refptr<ArrayBuffer> Slice(script::EnvironmentSettings* settings,
                                   int begin) const {
    return Slice(settings, begin, static_cast<int>(byte_length()));
  }
  scoped_refptr<ArrayBuffer> Slice(script::EnvironmentSettings* settings,
                                   int begin, int end) const;

  uint8* data() { return data_.data(); }
  const uint8* data() const { return data_.data(); }

  // Utility function for restricting begin/end offsets to an appropriate
  // range as defined by the spec. Negative start or end values refer
  // to offsets from the end of the array rather than the beginning.
  // If the length of the resulting range would be < 0, the length
  // is clamped to 0.
  static void ClampRange(int start, int end, int source_length,
                         int* clamped_start, int* clamped_end);

  DEFINE_WRAPPABLE_TYPE(ArrayBuffer);

 private:
  ~ArrayBuffer();

  Data data_;

  DISALLOW_COPY_AND_ASSIGN(ArrayBuffer);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_ARRAY_BUFFER_H_
