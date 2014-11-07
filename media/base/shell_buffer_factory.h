/*
 * Copyright 2012 Google Inc. All Rights Reserved.
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

#ifndef MEDIA_BASE_SHELL_BUFFER_FACTORY_H_
#define MEDIA_BASE_SHELL_BUFFER_FACTORY_H_

#include <list>
#include <map>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "lb_shell/lb_shell_constants.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_export.h"

namespace media {

static const size_t kShellMaxArraySize = 1024 * 1024;

class DecoderBuffer;
class DecryptConfig;
class ShellBufferFactory;

// A simple scoped array class designed to re-use the memory allocated by
// ShellBufferFactory. If needed would be trivial to make generic.
class MEDIA_EXPORT ShellScopedArray :
    public base::RefCountedThreadSafe<ShellScopedArray> {
 public:
  uint8* Get() { return array_; }
  size_t Size() { return size_; }

 private:
  friend class base::RefCountedThreadSafe<ShellScopedArray>;
  friend class ShellBufferFactory;
  // Should only be called by ShellBufferFactory, consumers should use
  // ShellBufferFactory::AllocateArray to allocate a ShellScopedArray
  ShellScopedArray(uint8* resuable_buffer, size_t size);
  virtual ~ShellScopedArray();
  uint8* array_;
  size_t size_;
  scoped_refptr<ShellBufferFactory> buffer_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ShellScopedArray);
};

// Singleton instance class for the management and recycling of media-related
// buffers. It is assumed that the usage pattern of these buffers is that they
// are used for only a short amount of time, and their allocation is roughly
// cyclical. All buffers are allocated using the alignment constants defined
// above, and all buffers are allocated from the shared pool of memory of
// size defined above.
class MEDIA_EXPORT ShellBufferFactory
    : public base::RefCountedThreadSafe<ShellBufferFactory> {
 public:
  static void Initialize();
  static inline scoped_refptr<ShellBufferFactory> Instance() {
    return instance_;
  }

  typedef base::Callback<void(scoped_refptr<DecoderBuffer>)> AllocCB;
  // Returns false if the allocator will never be able to allocate a buffer
  // of the requested size. Note that if memory is currently available this
  // function will call the callback provided _before_ returning true.
  bool AllocateBuffer(size_t size, AllocCB cb);
  // Returns true if a DecoderBuffer of this size could be allocated without
  // waiting for some other buffer to be released.
  bool HasRoomForBufferNow(size_t size);
  // This function tries to allocate a DecoderBuffer immediately. It returns
  // NULL on failure.
  scoped_refptr<DecoderBuffer> AllocateBufferNow(size_t size);
  // Returns a newly allocated byte field if there's room for it, or NULL if
  // there isn't. Note that this raw allocation method provides no guarantee
  // that ShellBufferFactory will still exist when the memory is to be freed.
  // If that is important please retain a reference to the buffer factory
  // (using Instance()) until the memory is to be reclaimed.
  uint8* AllocateNow(size_t size);
  // BLOCKS THE CALLING THREAD until an array of size is available and can be
  // allocated. We only allow one thread to block on an array allocation at a
  // time, all subsequents calls on other threads to AllocateArray will assert
  // and return NULL.
  scoped_refptr<ShellScopedArray> AllocateArray(size_t size);

  // Only called by DecoderBuffer and ShellScopedArray, informs the factory
  // that these objects have gone out of scoped and we can reclaim the memory
  void Reclaim(uint8* p);

  static void Terminate();

 private:
  friend class base::RefCountedThreadSafe<ShellBufferFactory>;
  ShellBufferFactory();
  ~ShellBufferFactory();
  uint8* AllocateLockAcquired(size_t aligned_size);
  size_t LargestFreeSpace_Locked() const;
  // given a size and an address, search the holes_ map for the specific
  // instance of this pair and remove it from the map.
  void FillHole_Locked(size_t size, uint8* address);

  static scoped_refptr<ShellBufferFactory> instance_;
  uint8* buffer_;

  // protects all following members.
  base::Lock lock_;

  // sorted map of pointers within buffer_ to sizes occupied at that offset.
  typedef std::map<uint8*, size_t> AllocMap;
  AllocMap allocs_;
  // sorted map of size of free spaces => pointer to that space. As there
  // can be multiple instances of holes of the same size, it's a multimap.
  typedef std::multimap<size_t, uint8*> HoleMap;
  HoleMap holes_;

  // queue of pending buffer allocation requests and their sizes
  typedef std::list<std::pair<AllocCB,
                              scoped_refptr<DecoderBuffer> > > AllocList;
  AllocList pending_allocs_;

  // event used for blocking calls for array allocation
  base::WaitableEvent array_allocation_event_;
  // set to 0 when no thread is blocking on an array allocation
  size_t array_requested_size_;
  // set to an allocation address when allocation has succeeded
  uint8* array_allocation_;
};

}  // namespace media

#endif  // MEDIA_BASE_SHELL_BUFFER_FACTORY_H_
