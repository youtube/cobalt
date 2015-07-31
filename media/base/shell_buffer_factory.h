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
#include "media/base/buffers.h"

namespace media {

// LB_SHELL is designed to leverage hardware audio and video decoders, which
// have specific byte-alignment for buffers. They also have limited memory,
// so we define a per-platform limit to the total size of the media buffering
// pool from which all ShellBuffers are allocated.
#if defined(__LB_ANDROID__)
// TODO: Determine more appropriate values for Android
static const int kShellBufferAlignment = 16;
static const int kShellBufferSpaceSize = 128 * 1024 * 1024;
#elif defined(__LB_LINUX__)
static const int kShellBufferAlignment = 16;
static const int kShellBufferSpaceSize = 128 * 1024 * 1024;
#elif defined(__LB_PS3__)
static const int kShellBufferAlignment = 128;
static const size_t kShellBufferSpaceSize = 48 * 1024 * 1024;
#elif defined(__LB_PS4__)
// TODO: Determine more appropriate values for the PS4
static const int kShellBufferAlignment = 128;
static const int kShellBufferSpaceSize = 128 * 1024 * 1024;
#elif defined(__LB_WIIU__)
static const int kShellBufferAlignment = kUnsegmentedMemoryAlignment;
static const int kShellBufferSpaceSize = kUnsegmentedMemorySize;
#elif defined(__LB_XB1__)
static const int kShellBufferAlignment = 16;
static const int kShellBufferSpaceSize = 33 * 1024 * 1024;
#elif defined(__LB_XB360__)
// TODO(***REMOVED***): Determine more appropriate values for the Xbox 360
static const int kShellBufferAlignment = 128;
static const int kShellBufferSpaceSize = 48 * 1024 * 1024;
#else
#error please define ShellBuffer constants for your platform.
#endif

static const size_t kShellMaxBufferSize = kShellBufferSpaceSize / 4;
static const size_t kShellMaxArraySize = 1024 * 1024;

class DecryptConfig;
class ShellFilterGraphLog;
class ShellBufferFactory;

// A simple scoped array class designed to re-use the memory allocated by
// ShellBufferFactory. If needed would be trivial to make generic.
class MEDIA_EXPORT ShellScopedArray :
    public base::RefCountedThreadSafe<ShellScopedArray> {
 public:
  uint8* Get() { return array_; }
  size_t Size() { return size_; }
  scoped_refptr<ShellFilterGraphLog> filter_graph_log();

 private:
  friend class base::RefCountedThreadSafe<ShellScopedArray>;
  friend class ShellBufferFactory;
  // Should only be called by ShellBufferFactory, consumers should use
  // ShellBufferFactory::AllocateArray to allocate a ShellScopedArray
  ShellScopedArray(uint8* resuable_buffer,
                   size_t size,
                   scoped_refptr<ShellFilterGraphLog> filter_graph_log);
  virtual ~ShellScopedArray();
  uint8* array_;
  size_t size_;
  scoped_refptr<ShellFilterGraphLog> filter_graph_log_;
  scoped_refptr<ShellBufferFactory> buffer_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ShellScopedArray);
};


// A media buffer object designed to use the recycled memory allocated
// by ShellBufferFactory.
class MEDIA_EXPORT ShellBuffer : public Buffer {
 public:
  // Create a ShellBuffer indicating we've reached end of stream or an error.
  // GetData() and GetWritableData() return NULL and GetDataSize() returns 0.
  static scoped_refptr<ShellBuffer> CreateEOSBuffer(
      base::TimeDelta timestamp,
      scoped_refptr<ShellFilterGraphLog> filter_graph_log);

  // Buffer implementation.
  virtual const uint8* GetData() const OVERRIDE { return buffer_; }
  // Data size can be less than allocated size after ShrinkTo is called.
  virtual int GetDataSize() const OVERRIDE { return size_; }
  int GetAllocatedSize() const { return allocated_size_; }
  // This is used for video AUs because we cannot know the exact size before
  // read it in.
  void ShrinkTo(int size);

  // Returns a read-write pointer to the buffer data.
  virtual uint8* GetWritableData() { return buffer_; }

  // Returns a flag indicating whether or not the buffer has been decrypted
  // in-place.  If so, a CDM should avoid decrypting it again after a seek.
  bool IsAlreadyDecrypted() { return is_decrypted_; }
  void SetAlreadyDecrypted(bool value) { is_decrypted_ = value; }

  scoped_refptr<ShellFilterGraphLog> filter_graph_log();

  const DecryptConfig* GetDecryptConfig() const;
  void SetDecryptConfig(scoped_ptr<DecryptConfig> decrypt_config);

 protected:
  friend class ShellBufferFactory;
  // Should only be called by ShellBufferFactory, consumers should use
  // ShellBufferFactory::AllocateBuffer to make a ShellBuffer.
  explicit ShellBuffer(uint8* reusable_buffer,
                       size_t size,
                       scoped_refptr<ShellFilterGraphLog> filter_graph_log);
  // For deferred allocation create a shell buffer with buffer_ NULL but a
  // non-zero size. Then we use the SetBuffer() method below to actually
  // set the reusable buffer pointer when it becomes available
  void SetBuffer(uint8* reusable_buffer);

  virtual ~ShellBuffer();
  uint8* buffer_;
  size_t size_;
  size_t allocated_size_;
  scoped_refptr<ShellFilterGraphLog> filter_graph_log_;
  scoped_refptr<ShellBufferFactory> buffer_factory_;
  scoped_ptr<DecryptConfig> decrypt_config_;
  bool is_decrypted_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ShellBuffer);
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
  // Returns the provided size aligned to kShellBufferAlignment.
  static inline size_t SizeAlign(size_t size) {
      return((size + kShellBufferAlignment - 1) / kShellBufferAlignment) *
          kShellBufferAlignment;
  }

  typedef base::Callback<void(scoped_refptr<ShellBuffer>)> AllocCB;
  // Returns false if the allocator will never be able to allocate a buffer
  // of the requested size. Note that if memory is currently available this
  // function will call the callback provided _before_ returning true.
  // Provide a filter_graph_log object for the factory to log creation and
  // destruction of this buffer in.
  bool AllocateBuffer(size_t size,
                      AllocCB cb,
                      scoped_refptr<ShellFilterGraphLog> filter_graph_log);
  // Returns true if a ShellBuffer of this size could be allocated without
  // waiting for some other buffer to be released.
  bool HasRoomForBufferNow(size_t size);
  // This function tries to allocate a ShellBuffer immediately. It returns NULL
  // on failure.
  // Provide a filter_graph_log object for the factory to log creation and
  // destruction of this buffer in.
  scoped_refptr<ShellBuffer> AllocateBufferNow(
      size_t size, scoped_refptr<ShellFilterGraphLog> filter_graph_log);
  // Returns a newly allocated byte field if there's room for it, or NULL if
  // there isn't. Note that this raw allocation method provides no guarantee
  // that ShellBufferFactory will still exist when the memory is to be freed.
  // If that is important please retain a reference to the buffer factory
  // (using Instance()) until the memory is to be reclaimed.
  uint8* AllocateNow(size_t size);
  // BLOCKS THE CALLING THREAD until an array of size is available and can be
  // allocated. We only allow one thread to block on an array allocation at a
  // time, all subsequents calls on other threads to AllocateArray will assert
  // and return NULL.  Provide a filter_graph_log for the factory to log this
  // array in.
  scoped_refptr<ShellScopedArray> AllocateArray(
      size_t size,
      scoped_refptr<ShellFilterGraphLog> filter_graph_log);

  // Only called by ShellBuffer and ShellScopedArray, informs the factory that
  // these objects have gone out of scoped and we can reclaim the memory
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
                              scoped_refptr<ShellBuffer> > > AllocList;
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
