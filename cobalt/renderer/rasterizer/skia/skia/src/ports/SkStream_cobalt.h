// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKSTREAM_COBALT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKSTREAM_COBALT_H_

#include <string>

#include "SkMutex.h"
#include "SkStream.h"
#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/containers/small_map.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "cobalt/base/c_val.h"

// The SkFileMemoryChunkStream classes provide a stream type that mixes features
// of both file streams and memory streams. While the stream initially reads
// from its file like a file stream, it lazily caches newly encountered 32KB
// chunks of the file into memory, enabling it to avoid reading sections of the
// file more than once--subsequent reads into those cached sections are made
// directly from memory. This caching occurs until the manager's cache limit is
// exhausted. After this, reads into uncached sections of the stream are always
// made to the file and not cached, functioning similarly to a file stream.
// Reads into the already cached sections are still handled in memory.
//
// This stream type provides much of the speed benefits of memory streams, while
// also guaranteeing that memory will not be wasted on sections of files that
// are never used and that the combined memory usage of the streams will never
// exceed the specified limit. If the cache limit is set to 0, then the streams
// do no caching and fully function like file streams.

class SkFileMemoryChunk;
class SkFileMemoryChunkStream;
class SkFileMemoryChunkStreamProvider;

typedef base::SmallMap<
    base::hash_map<size_t, scoped_refptr<const SkFileMemoryChunk> >, 8>
    SkFileMemoryChunks;
typedef base::hash_map<std::string, SkFileMemoryChunkStreamProvider*>
    SkFileMemoryChunkStreamProviderMap;

class SkFileMemoryChunk : public base::RefCountedThreadSafe<SkFileMemoryChunk> {
 public:
  static const size_t kSizeInBytes = 32 * 1024;
  uint8_t memory[kSizeInBytes];
};

// SkFileMemoryChunkStreamManager is a thread-safe class that generates
// file-specific SkFileMemoryChunkStreamProvider objects, which can be
// used to create streams for those files. It also handles tracking the memory
// available for memory chunks within the cache and determining whether or not
// additional chunks can be created.
class SkFileMemoryChunkStreamManager {
 public:
  SkFileMemoryChunkStreamManager(const std::string& name,
                                 int cache_capacity_in_bytes);

  // Returns the stream provider associated with the file path. If it does not
  // exist then it is created.
  SkFileMemoryChunkStreamProvider* GetStreamProvider(
      const std::string& file_path);

  // Purges unused memory chunks from all existing stream providers.
  void PurgeUnusedMemoryChunks();

 private:
  friend SkFileMemoryChunkStreamProvider;

  // Attempts to reserve an available memory chunk and returns true if
  // successful. On success, |available_chunk_count_| is decremented by one.
  bool TryReserveMemoryChunk();
  // Adds the specified count to |available_chunk_count_|.
  void ReleaseReservedMemoryChunks(size_t count);

  SkMutex stream_provider_mutex_;
  ScopedVector<SkFileMemoryChunkStreamProvider> stream_provider_array_;
  SkFileMemoryChunkStreamProviderMap stream_provider_map_;

  base::subtle::Atomic32 available_chunk_count_;

  const base::CVal<base::cval::SizeInBytes, base::CValPublic>
      cache_capacity_in_bytes_;
  base::CVal<base::cval::SizeInBytes, base::CValPublic> cache_size_in_bytes_;

  DISALLOW_COPY_AND_ASSIGN(SkFileMemoryChunkStreamManager);
};

// SkFileMemoryChunkStreamProvider is a thread-safe class that handles creating
// streams for its specified file and processes requests from those streams for
// memory chunks. This processing work includes verifying that additional memory
// chunks are available with the manager, creating and caching new memory
// chunks, and sharing cached memory chunks between all of the streams that it
// created.
class SkFileMemoryChunkStreamProvider {
 public:
  const std::string& file_path() const { return file_path_; }

  // Returns a newly created SkFileMemoryChunkStream. It is the caller's
  // responsibility to destroy it.
  SkFileMemoryChunkStream* OpenStream() const;

  // Returns a newly created snapshot of the provider's current memory chunks.
  // While the snapshot exists, it retains references to all of those memory
  // chunks, guaranteeing that they will be retained when
  // PurgeUnusedMemoryChunks() is called.
  scoped_ptr<const SkFileMemoryChunks> CreateMemoryChunksSnapshot();

  // Purges all of the provider's memory chunks that are only referenced by the
  // provider.
  void PurgeUnusedMemoryChunks();

 private:
  friend SkFileMemoryChunkStream;
  friend SkFileMemoryChunkStreamManager;

  SkFileMemoryChunkStreamProvider(const std::string& file_path,
                                  SkFileMemoryChunkStreamManager* manager);

  // Returns the specified memory chunk if it exists. If it does not, attempts
  // to create the memory chunk and populate it with data from the stream at the
  // specified index. On success, the memory chunk is returned; otherwise, NULL
  // is returned.
  scoped_refptr<const SkFileMemoryChunk> TryGetMemoryChunk(
      size_t index, SkFileMemoryChunkStream* stream);

  const std::string file_path_;
  SkFileMemoryChunkStreamManager* const manager_;

  // The provider maintains a cache of all memory chunks that have been
  // requested by any streams. This enables it share the memory chunks between
  // all of the streams that it creates. The cache must be accessed behind a
  // lock because the stream requests may come from multiple threads.
  SkMutex memory_chunks_mutex_;
  SkFileMemoryChunks memory_chunks_;

  DISALLOW_COPY_AND_ASSIGN(SkFileMemoryChunkStreamProvider);
};

// SkFileMemoryChunkStream is a non-thread safe stream used within Skia, which
// opens a file handle to the file specified by its provider. It can read data
// from either memory chunks returned by its provider or from the file itself.
// While it maintains an internal cache to previously retrieved memory chunks,
// it relies on its provider for memory chunks that it has not already cached.
// If a memory chunk is available for a location, the stream reads that location
// directly from memory; otherwise, it reads the location from the file.
//
// NOTE: Although the provider handles creating new memory chunks, it does not
// directly access the file and uses the requesting stream to read the data into
// memory.
class SkFileMemoryChunkStream : public SkStreamAsset {
 public:
  ~SkFileMemoryChunkStream();

  // Required by SkStream
  virtual size_t read(void* buffer, size_t size) SK_OVERRIDE;
  virtual bool isAtEnd() const SK_OVERRIDE;

  // Required by SkStreamRewindable
  virtual bool rewind() SK_OVERRIDE;
  virtual SkFileMemoryChunkStream* duplicate() const SK_OVERRIDE;

  // Required by SkStreamSeekable
  virtual size_t getPosition() const SK_OVERRIDE;
  virtual bool seek(size_t position) SK_OVERRIDE;
  virtual bool move(long offset) SK_OVERRIDE;
  virtual SkFileMemoryChunkStream* fork() const SK_OVERRIDE;

  // Required by SkStreamAsset
  virtual size_t getLength() const SK_OVERRIDE;

 private:
  friend SkFileMemoryChunkStreamProvider;

  explicit SkFileMemoryChunkStream(
      SkFileMemoryChunkStreamProvider* stream_provider);

  // Attempts to read the file position specified by the index into the chunk's
  // memory. Returns true if the read was successful.
  bool ReadIndexIntoMemoryChunk(size_t index, SkFileMemoryChunk* chunk);

  SkFileMemoryChunkStreamProvider* const stream_provider_;

  SkFile* const file_;
  size_t file_length_;
  size_t file_position_;

  size_t stream_position_;

  // The stream maintains its own references to any memory chunks that it has
  // requested from the provider. This allows it faster access to them, as it
  // does not need to worry about thread safety within its locally cached
  // chunks and can directly access them, whereas a lock is required when
  // the provider accesses its own memory chunks. The stream's memory chunk
  // references also enable the provider to know which memory chunks are being
  // used and which can be purged.
  SkFileMemoryChunks memory_chunks_;

  DISALLOW_COPY_AND_ASSIGN(SkFileMemoryChunkStream);
};

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKSTREAM_COBALT_H_
