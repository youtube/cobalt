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

#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkStream_cobalt.h"

#include <stdio.h>

#include <algorithm>
#include <limits>
#include <vector>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkOSFile_cobalt.h"

SkFileMemoryChunkStreamManager::SkFileMemoryChunkStreamManager(
    const std::string& name, int cache_capacity_in_bytes)
    : available_chunk_count_(cache_capacity_in_bytes /
                             SkFileMemoryChunk::kSizeInBytes),
      cache_capacity_in_bytes_(StringPrintf("Memory.%s.Capacity", name.c_str()),
                               cache_capacity_in_bytes,
                               "The byte capacity of the cache."),
      cache_size_in_bytes_(
          StringPrintf("Memory.%s.Size", name.c_str()), 0,
          "Total number of bytes currently used by the cache.") {}

SkFileMemoryChunkStreamProvider*
SkFileMemoryChunkStreamManager::GetStreamProvider(
    const std::string& file_path) {
  SkAutoMutexAcquire scoped_mutex(stream_provider_mutex_);

  // Return a pre-existing stream provider if it exists.
  SkFileMemoryChunkStreamProviderMap::iterator iter =
      stream_provider_map_.find(file_path);
  if (iter != stream_provider_map_.end()) {
    return iter->second;
  }

  // Otherwise, create a new stream provider, add it to the containers, and
  // return it. This stream provider will be returned for any subsequent
  // requests specifying this file, ensuring that memory chunks will be shared.
  stream_provider_array_.push_back(
      new SkFileMemoryChunkStreamProvider(file_path, this));
  SkFileMemoryChunkStreamProvider* stream_provider =
      *stream_provider_array_.rbegin();
  stream_provider_map_[file_path] = stream_provider;
  return stream_provider;
}

void SkFileMemoryChunkStreamManager::PurgeUnusedMemoryChunks() {
  SkAutoMutexAcquire scoped_mutex(stream_provider_mutex_);
  for (ScopedVector<SkFileMemoryChunkStreamProvider>::iterator iter =
           stream_provider_array_.begin();
       iter != stream_provider_array_.end(); ++iter) {
    (*iter)->PurgeUnusedMemoryChunks();
  }
}

bool SkFileMemoryChunkStreamManager::TryReserveMemoryChunk() {
  // First check to see if the count is already 0. If it is, then there's no
  // available memory chunk to try to reserve. Simply return failure.
  if (base::subtle::NoBarrier_Load(&available_chunk_count_) <= 0) {
    return false;
  }

  // Decrement the available count behind a memory barrier. If the return value
  // is less than 0, then another requester reserved the last available memory
  // chunk first. In that case, restore the chunk to the count and return
  // failure.
  if (base::subtle::Barrier_AtomicIncrement(&available_chunk_count_, -1) < 0) {
    base::subtle::NoBarrier_AtomicIncrement(&available_chunk_count_, 1);
    return false;
  }

  // The chunk was successfully reserved. Add the reserved chunk to the used
  // cache size.
  cache_size_in_bytes_ += SkFileMemoryChunk::kSizeInBytes;
  return true;
}

void SkFileMemoryChunkStreamManager::ReleaseReservedMemoryChunks(size_t count) {
  base::subtle::NoBarrier_AtomicIncrement(&available_chunk_count_, count);
  cache_size_in_bytes_ -= count * SkFileMemoryChunk::kSizeInBytes;
}

SkFileMemoryChunkStreamProvider::SkFileMemoryChunkStreamProvider(
    const std::string& file_path, SkFileMemoryChunkStreamManager* manager)
    : file_path_(file_path), manager_(manager) {}

SkFileMemoryChunkStream* SkFileMemoryChunkStreamProvider::OpenStream() const {
  return new SkFileMemoryChunkStream(
      const_cast<SkFileMemoryChunkStreamProvider*>(this));
}

scoped_ptr<const SkFileMemoryChunks>
SkFileMemoryChunkStreamProvider::CreateMemoryChunksSnapshot() {
  SkAutoMutexAcquire scoped_mutex(memory_chunks_mutex_);
  return make_scoped_ptr<const SkFileMemoryChunks>(
      new SkFileMemoryChunks(memory_chunks_));
}

void SkFileMemoryChunkStreamProvider::PurgeUnusedMemoryChunks() {
  size_t purge_count = 0;

  // Scope the logic that accesses the memory chunks so that releasing reserved
  // memory chunks does not happen within the lock.
  {
    SkAutoMutexAcquire scoped_mutex(memory_chunks_mutex_);

    // Walk the memory chunks, adding any with a single ref. These are not
    // externally referenced and can be purged.
    std::vector<size_t> purge_indices;
    for (SkFileMemoryChunks::iterator iter = memory_chunks_.begin();
         iter != memory_chunks_.end(); ++iter) {
      if (iter->second && iter->second->HasOneRef()) {
        purge_indices.push_back(iter->first);
      }
    }

    // If the memory chunks and purge indices have the same size, then every
    // memory chunk is being purged and they can simply be cleared. Otherwise,
    // the purge indices must individually be removed from the memory chunks.
    if (memory_chunks_.size() == purge_indices.size()) {
      memory_chunks_.clear();
    } else {
      for (std::vector<size_t>::iterator iter = purge_indices.begin();
           iter != purge_indices.end(); ++iter) {
        memory_chunks_.erase(*iter);
      }
    }

    purge_count = purge_indices.size();
  }

  // Make any memory chunks that were purged available again.
  if (purge_count > 0) {
    manager_->ReleaseReservedMemoryChunks(purge_count);
  }
}

scoped_refptr<const SkFileMemoryChunk>
SkFileMemoryChunkStreamProvider::TryGetMemoryChunk(
    size_t index, SkFileMemoryChunkStream* stream) {
  // Scope the logic that initially accesses the memory chunks. This allows the
  // creation of a new memory chunk and the read from the stream into its memory
  // to occur outside of a lock.
  {
    SkAutoMutexAcquire scoped_mutex(memory_chunks_mutex_);
    SkFileMemoryChunks::const_iterator iter = memory_chunks_.find(index);
    if (iter != memory_chunks_.end()) {
      return iter->second;
    }
  }

  // Verify that the new memory chunk can be reserved before attempting to
  // create it.
  if (!manager_->TryReserveMemoryChunk()) {
    return NULL;
  }

  // Create the memory chunk and attempt to read from the stream into it. If
  // this fails, clear out |new_chunk|, which causes it to be deleted; however,
  // do not return. Subsequent attempts will also fail, so the map is populated
  // with a NULL value for this index to prevent the attempts from occurring.
  scoped_refptr<SkFileMemoryChunk> new_chunk(new SkFileMemoryChunk);
  if (!stream->ReadIndexIntoMemoryChunk(index, new_chunk)) {
    new_chunk = NULL;
  }

  // Re-lock the mutex. It's time to potentially add the newly created chunk to
  // |memory_chunks_|.
  SkAutoMutexAcquire scoped_mutex(memory_chunks_mutex_);
  scoped_refptr<const SkFileMemoryChunk>& chunk = memory_chunks_[index];
  // If there isn't a pre-existing chunk (this can occur if there was a race
  // between two calls to TryGetMemoryChunk() and the other call won), and the
  // new chunk exists, then set the reference so that it'll be available for
  // subsequent calls.
  if (chunk == NULL && new_chunk != NULL) {
    chunk = new_chunk;
  } else {
    // The new chunk will not be retained, so make the reserved chunk
    // available again.
    manager_->ReleaseReservedMemoryChunks(1);
  }

  return chunk;
}

SkFileMemoryChunkStream::SkFileMemoryChunkStream(
    SkFileMemoryChunkStreamProvider* stream_provider)
    : stream_provider_(stream_provider),
      file_(sk_fopen(stream_provider->file_path().c_str(), kRead_SkFILE_Flag)),
      file_position_(0),
      stream_position_(0) {
  file_length_ = file_ ? sk_fgetsize(file_) : 0;
}

SkFileMemoryChunkStream::~SkFileMemoryChunkStream() {
  if (file_) {
    sk_fclose(file_);
  }
}

size_t SkFileMemoryChunkStream::read(void* buffer, size_t size) {
  if (!file_) {
    return 0;
  }

  size_t start_stream_position = stream_position_;
  size_t end_stream_position = stream_position_ + size;

  size_t start_index = start_stream_position / SkFileMemoryChunk::kSizeInBytes;
  size_t end_index = end_stream_position / SkFileMemoryChunk::kSizeInBytes;

  // Iterate through all of the chunk indices included within the read. Each is
  // read into the buffer separately.
  for (size_t current_index = start_index; current_index <= end_index;
       ++current_index) {
    size_t current_index_end_stream_position =
        current_index == end_index
            ? end_stream_position
            : ((current_index + 1) * SkFileMemoryChunk::kSizeInBytes);
    size_t index_desired_read_size =
        current_index_end_stream_position - stream_position_;

    // Look for the chunk within the stream's cached chunks. If it isn't there,
    // then attempt to retrieve it from the stream provider and cache the
    // result.
    scoped_refptr<const SkFileMemoryChunk> memory_chunk;
    SkFileMemoryChunks::const_iterator iter =
        memory_chunks_.find(current_index);
    if (iter == memory_chunks_.end()) {
      memory_chunk = memory_chunks_[current_index] =
          stream_provider_->TryGetMemoryChunk(current_index, this);
    } else {
      memory_chunk = iter->second;
    }

    // It's time to read the data specified by the index into the buffer. If
    // the memory chunk exists, then the data can be directly copied from its
    // memory; otherwise, the data needs to be read from the stream's file.
    size_t index_actual_read_size;
    if (memory_chunk) {
      size_t index_start_position =
          current_index * SkFileMemoryChunk::kSizeInBytes;
      memcpy(buffer,
             memory_chunk->memory + (stream_position_ - index_start_position),
             index_desired_read_size);
      index_actual_read_size = index_desired_read_size;
    } else {
      // Ensure that the file position matches the stream's position. If the
      // seek fails, then set file position to an invalid value to ensure that
      // the next read triggers a new seek, and break out.
      if (file_position_ != stream_position_ &&
          !sk_fseek(file_, stream_position_)) {
        file_position_ = std::numeric_limits<size_t>::max();
        break;
      }

      // Note that by using |sk_fread| vs. |sk_qread|, additional seeks are
      // avoided.  This is because |sk_qread|'s implementation does multiple
      // seeks to ensure that the file cursor is at the same position after the
      // |sk_qread| operation is done.
      index_actual_read_size = sk_fread(buffer, index_desired_read_size, file_);
      file_position_ = stream_position_ + index_actual_read_size;
    }

    stream_position_ += index_actual_read_size;

    // Verify that the read succeeded. If it failed, then break out because
    // nothing more can be processed; otherwise, if there are additional indices
    // to process, update the buffer's pointer so that they will write to the
    // correct memory location.
    if (index_desired_read_size != index_actual_read_size) {
      break;
    } else if (current_index < end_index) {
      buffer = reinterpret_cast<uint8_t*>(buffer) + index_actual_read_size;
    }
  }

  return stream_position_ - start_stream_position;
}

bool SkFileMemoryChunkStream::isAtEnd() const {
  return stream_position_ == file_length_;
}

bool SkFileMemoryChunkStream::rewind() {
  stream_position_ = 0;
  return true;
}

SkFileMemoryChunkStream* SkFileMemoryChunkStream::duplicate() const {
  return stream_provider_->OpenStream();
}

size_t SkFileMemoryChunkStream::getPosition() const { return stream_position_; }

bool SkFileMemoryChunkStream::seek(size_t position) {
  stream_position_ = std::min(position, file_length_);
  return true;
}

bool SkFileMemoryChunkStream::move(long offset) {
  // Rewind back to the start of the stream if the offset would move it to a
  // negative position.
  if (offset < 0 && std::abs(offset) > stream_position_) {
    return rewind();
  }
  return seek(stream_position_ + offset);
}

SkFileMemoryChunkStream* SkFileMemoryChunkStream::fork() const {
  scoped_ptr<SkFileMemoryChunkStream> that(duplicate());
  that->seek(stream_position_);
  return that.release();
}

size_t SkFileMemoryChunkStream::getLength() const { return file_length_; }

bool SkFileMemoryChunkStream::ReadIndexIntoMemoryChunk(
    size_t index, SkFileMemoryChunk* chunk) {
  size_t index_position = index * SkFileMemoryChunk::kSizeInBytes;

  // Ensure that the file position matches the index's position. If the seek
  // fails, then set file position to an invalid value to ensure that the next
  // read triggers a new seek, and return false.
  if (file_position_ != index_position && !sk_fseek(file_, index_position)) {
    file_position_ = std::numeric_limits<size_t>::max();
    return false;
  }

  const size_t kChunkMaxReadSize = SkFileMemoryChunk::kSizeInBytes;
  size_t desired_read_size =
      std::min(file_length_ - index_position, kChunkMaxReadSize);

  // Note that by using |sk_fread| vs. |sk_qread|, additional seeks are
  // avoided.  This is because |sk_qread|'s implementation does multiple
  // seeks to ensure that the file cursor is at the same position after the
  // |sk_qread| operation is done.
  size_t actual_read_size = sk_fread(chunk->memory, desired_read_size, file_);
  file_position_ = index_position + actual_read_size;

  return desired_read_size == actual_read_size;
}
