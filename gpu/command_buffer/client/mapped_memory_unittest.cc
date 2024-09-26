// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/mapped_memory.h"

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/client/command_buffer_direct_locked.h"
#include "gpu/command_buffer/service/mocks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

using testing::Return;
using testing::Mock;
using testing::Truly;
using testing::Sequence;
using testing::DoAll;
using testing::Invoke;
using testing::_;

class MappedMemoryTestBase : public testing::Test {
 protected:
  static const unsigned int kBufferSize = 1024;

  void SetUp() override {
    command_buffer_ = std::make_unique<CommandBufferDirectLocked>();
    api_mock_ = std::make_unique<AsyncAPIMock>(true, command_buffer_.get(),
                                               command_buffer_->service());

    // ignore noops in the mock - we don't want to inspect the internals of the
    // helper.
    EXPECT_CALL(*api_mock_, DoCommand(cmd::kNoop, 0, _))
        .WillRepeatedly(Return(error::kNoError));
    // Forward the SetToken calls to the engine
    EXPECT_CALL(*api_mock_.get(), DoCommand(cmd::kSetToken, 1, _))
        .WillRepeatedly(DoAll(Invoke(api_mock_.get(), &AsyncAPIMock::SetToken),
                              Return(error::kNoError)));

    helper_ = std::make_unique<CommandBufferHelper>(command_buffer_.get());
    helper_->Initialize(kBufferSize);
  }

  int32_t GetToken() { return command_buffer_->GetLastState().token; }

  std::unique_ptr<CommandBufferDirectLocked> command_buffer_;
  std::unique_ptr<AsyncAPIMock> api_mock_;
  std::unique_ptr<CommandBufferHelper> helper_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

const unsigned int MappedMemoryTestBase::kBufferSize;

// Test fixture for MemoryChunk test - Creates a MemoryChunk, using a
// CommandBufferHelper with a mock AsyncAPIInterface for its interface (calling
// it directly, not through the RPC mechanism), making sure Noops are ignored
// and SetToken are properly forwarded to the engine.
class MemoryChunkTest : public MappedMemoryTestBase {
 protected:
  static const int32_t kShmId = 123;
  void SetUp() override {
    MappedMemoryTestBase::SetUp();
    base::UnsafeSharedMemoryRegion shared_memory_region =
        base::UnsafeSharedMemoryRegion::Create(kBufferSize);
    base::WritableSharedMemoryMapping shared_memory_mapping =
        shared_memory_region.Map();
    buffer_ = MakeBufferFromSharedMemory(std::move(shared_memory_region),
                                         std::move(shared_memory_mapping));
    chunk_ = std::make_unique<MemoryChunk>(kShmId, buffer_, helper_.get());
  }

  void TearDown() override {
    // If the CommandExecutor posts any tasks, this forces them to run.
    base::RunLoop().RunUntilIdle();

    MappedMemoryTestBase::TearDown();
  }

  uint8_t* buffer_memory() { return static_cast<uint8_t*>(buffer_->memory()); }

  std::unique_ptr<MemoryChunk> chunk_;
  scoped_refptr<gpu::Buffer> buffer_;
};

const int32_t MemoryChunkTest::kShmId;

TEST_F(MemoryChunkTest, Basic) {
  const unsigned int kSize = 16;
  EXPECT_EQ(kShmId, chunk_->shm_id());
  EXPECT_EQ(kBufferSize, chunk_->GetLargestFreeSizeWithoutWaiting());
  EXPECT_EQ(kBufferSize, chunk_->GetLargestFreeSizeWithWaiting());
  EXPECT_EQ(kBufferSize, chunk_->GetSize());
  void *pointer = chunk_->Alloc(kSize);
  ASSERT_TRUE(pointer);
  EXPECT_LE(buffer_->memory(), static_cast<uint8_t*>(pointer));
  EXPECT_GE(kBufferSize,
            static_cast<uint8_t*>(pointer) - buffer_memory() + kSize);
  EXPECT_EQ(kBufferSize - kSize, chunk_->GetLargestFreeSizeWithoutWaiting());
  EXPECT_EQ(kBufferSize - kSize, chunk_->GetLargestFreeSizeWithWaiting());
  EXPECT_EQ(kBufferSize, chunk_->GetSize());

  chunk_->Free(pointer);
  EXPECT_EQ(kBufferSize, chunk_->GetLargestFreeSizeWithoutWaiting());
  EXPECT_EQ(kBufferSize, chunk_->GetLargestFreeSizeWithWaiting());

  uint8_t* pointer_char = static_cast<uint8_t*>(chunk_->Alloc(kSize));
  ASSERT_TRUE(pointer_char);
  EXPECT_LE(buffer_memory(), pointer_char);
  EXPECT_GE(buffer_memory() + kBufferSize, pointer_char + kSize);
  EXPECT_EQ(kBufferSize - kSize, chunk_->GetLargestFreeSizeWithoutWaiting());
  EXPECT_EQ(kBufferSize - kSize, chunk_->GetLargestFreeSizeWithWaiting());
  chunk_->Free(pointer_char);
  EXPECT_EQ(kBufferSize, chunk_->GetLargestFreeSizeWithoutWaiting());
  EXPECT_EQ(kBufferSize, chunk_->GetLargestFreeSizeWithWaiting());
}

class MappedMemoryManagerTest : public MappedMemoryTestBase {
 public:
  MappedMemoryManager* manager() const {
    return manager_.get();
  }

 protected:
  void SetUp() override {
    MappedMemoryTestBase::SetUp();
    manager_ = std::make_unique<MappedMemoryManager>(
        helper_.get(), MappedMemoryManager::kNoLimit);
  }

  void TearDown() override {
    // If the CommandExecutor posts any tasks, this forces them to run.
    base::RunLoop().RunUntilIdle();
    manager_.reset();
    MappedMemoryTestBase::TearDown();
  }

  std::unique_ptr<MappedMemoryManager> manager_;
};

TEST_F(MappedMemoryManagerTest, Basic) {
  const unsigned int kSize = 1024;
  // Check we can alloc.
  int32_t id1 = -1;
  unsigned int offset1 = 0xFFFFFFFFU;
  void* mem1 = manager_->Alloc(kSize, &id1, &offset1);
  ASSERT_TRUE(mem1);
  EXPECT_NE(-1, id1);
  EXPECT_EQ(0u, offset1);
  // Check if we free and realloc the same size we get the same memory
  int32_t id2 = -1;
  unsigned int offset2 = 0xFFFFFFFFU;
  manager_->Free(mem1);
  void* mem2 = manager_->Alloc(kSize, &id2, &offset2);
  EXPECT_EQ(mem1, mem2);
  EXPECT_EQ(id1, id2);
  EXPECT_EQ(offset1, offset2);
  // Check if we allocate again we get different shared memory
  int32_t id3 = -1;
  unsigned int offset3 = 0xFFFFFFFFU;
  void* mem3 = manager_->Alloc(kSize, &id3, &offset3);
  ASSERT_TRUE(mem3 != nullptr);
  EXPECT_NE(mem2, mem3);
  EXPECT_NE(id2, id3);
  EXPECT_EQ(0u, offset3);
  // Free 3 and allocate 2 half size blocks.
  manager_->Free(mem3);
  int32_t id4 = -1;
  int32_t id5 = -1;
  unsigned int offset4 = 0xFFFFFFFFU;
  unsigned int offset5 = 0xFFFFFFFFU;
  void* mem4 = manager_->Alloc(kSize / 2, &id4, &offset4);
  void* mem5 = manager_->Alloc(kSize / 2, &id5, &offset5);
  ASSERT_TRUE(mem4 != nullptr);
  ASSERT_TRUE(mem5 != nullptr);
  EXPECT_EQ(id3, id4);
  EXPECT_EQ(id4, id5);
  EXPECT_EQ(0u, offset4);
  EXPECT_EQ(kSize / 2u, offset5);
  manager_->Free(mem4);
  manager_->Free(mem2);
  manager_->Free(mem5);
}

TEST_F(MappedMemoryManagerTest, FreePendingToken) {
  const unsigned int kSize = 128;
  const unsigned int kAllocCount = (kBufferSize / kSize) * 2;
  CHECK(kAllocCount * kSize == kBufferSize * 2);

  // Allocate several buffers across multiple chunks.
  void *pointers[kAllocCount];
  for (unsigned int i = 0; i < kAllocCount; ++i) {
    int32_t id = -1;
    unsigned int offset = 0xFFFFFFFFu;
    pointers[i] = manager_->Alloc(kSize, &id, &offset);
    EXPECT_TRUE(pointers[i]);
    EXPECT_NE(id, -1);
    EXPECT_NE(offset, 0xFFFFFFFFu);
  }

  // Free one successful allocation, pending fence.
  int32_t token = helper_.get()->InsertToken();
  manager_->FreePendingToken(pointers[0], token);

  // The way we hooked up the helper and engine, it won't process commands
  // until it has to wait for something. Which means the token shouldn't have
  // passed yet at this point.
  EXPECT_GT(token, GetToken());
  // Force it to read up to the token
  helper_->Finish();
  // Check that the token has indeed passed.
  EXPECT_LE(token, GetToken());

  // This allocation should use the spot just freed above.
  int32_t new_id = -1;
  unsigned int new_offset = 0xFFFFFFFFu;
  void* new_ptr = manager_->Alloc(kSize, &new_id, &new_offset);
  EXPECT_TRUE(new_ptr);
  EXPECT_EQ(new_ptr, pointers[0]);
  EXPECT_NE(new_id, -1);
  EXPECT_NE(new_offset, 0xFFFFFFFFu);

  // Free up everything.
  manager_->Free(new_ptr);
  for (unsigned int i = 1; i < kAllocCount; ++i) {
    manager_->Free(pointers[i]);
  }
}

TEST_F(MappedMemoryManagerTest, FreeUnused) {
  command_buffer_->LockFlush();
  int32_t id = -1;
  unsigned int offset = 0xFFFFFFFFU;
  const unsigned int kAllocSize = 2048;
  manager_->set_chunk_size_multiple(kAllocSize * 2);

  void* m1 = manager_->Alloc(kAllocSize, &id, &offset);
  void* m2 = manager_->Alloc(kAllocSize, &id, &offset);
  ASSERT_TRUE(m1 != nullptr);
  ASSERT_TRUE(m2 != nullptr);
  // m1 and m2 fit in one chunk
  EXPECT_EQ(1u, manager_->num_chunks());

  void* m3 = manager_->Alloc(kAllocSize, &id, &offset);
  ASSERT_TRUE(m3 != nullptr);
  // m3 needs another chunk
  EXPECT_EQ(2u, manager_->num_chunks());

  // Nothing to free, both chunks are in-use.
  manager_->FreeUnused();
  EXPECT_EQ(2u, manager_->num_chunks());

  manager_->Free(m3);
  EXPECT_EQ(2u, manager_->num_chunks());
  // The second chunk is no longer in use, we can remove.
  manager_->FreeUnused();
  EXPECT_EQ(1u, manager_->num_chunks());

  int32_t token = helper_->InsertToken();
  manager_->FreePendingToken(m1, token);
  // The way we hooked up the helper and engine, it won't process commands
  // until it has to wait for something. Which means the token shouldn't have
  // passed yet at this point.
  EXPECT_GT(token, GetToken());
  EXPECT_EQ(1u, manager_->num_chunks());

  int old_flush_count = command_buffer_->FlushCount();
  // The remaining chunk is still busy, can't free it.
  manager_->FreeUnused();
  EXPECT_EQ(1u, manager_->num_chunks());
  // This should not have caused a Flush or a Finish.
  EXPECT_GT(token, GetToken());
  EXPECT_EQ(old_flush_count, command_buffer_->FlushCount());

  manager_->Free(m2);
  EXPECT_EQ(1u, manager_->num_chunks());
  // The remaining chunk is free pending token, we can release the shared
  // memory.
  manager_->FreeUnused();
  EXPECT_EQ(0u, manager_->num_chunks());
  // This should have triggered a Flush, but not forced a Finish, i.e. the token
  // shouldn't have passed yet.
  EXPECT_EQ(old_flush_count + 1, command_buffer_->FlushCount());
  EXPECT_GT(token, GetToken());
}

TEST_F(MappedMemoryManagerTest, ChunkSizeMultiple) {
  const unsigned int kSize = 1024;
  manager_->set_chunk_size_multiple(kSize *  2);
  // Check if we allocate less than the chunk size multiple we get
  // chunks arounded up.
  int32_t id1 = -1;
  unsigned int offset1 = 0xFFFFFFFFU;
  void* mem1 = manager_->Alloc(kSize, &id1, &offset1);
  int32_t id2 = -1;
  unsigned int offset2 = 0xFFFFFFFFU;
  void* mem2 = manager_->Alloc(kSize, &id2, &offset2);
  int32_t id3 = -1;
  unsigned int offset3 = 0xFFFFFFFFU;
  void* mem3 = manager_->Alloc(kSize, &id3, &offset3);
  ASSERT_TRUE(mem1);
  ASSERT_TRUE(mem2);
  ASSERT_TRUE(mem3);
  EXPECT_NE(-1, id1);
  EXPECT_EQ(id1, id2);
  EXPECT_NE(id2, id3);
  EXPECT_EQ(0u, offset1);
  EXPECT_EQ(kSize, offset2);
  EXPECT_EQ(0u, offset3);

  manager_->Free(mem1);
  manager_->Free(mem2);
  manager_->Free(mem3);
}

TEST_F(MappedMemoryManagerTest, UnusedMemoryLimit) {
  const unsigned int kChunkSize = 2048;
  // Reset the manager with a memory limit.
  manager_ = std::make_unique<MappedMemoryManager>(helper_.get(), kChunkSize);
  manager_->set_chunk_size_multiple(kChunkSize);

  // Allocate one chunk worth of memory.
  int32_t id1 = -1;
  unsigned int offset1 = 0xFFFFFFFFU;
  void* mem1 = manager_->Alloc(kChunkSize, &id1, &offset1);
  ASSERT_TRUE(mem1);
  EXPECT_NE(-1, id1);
  EXPECT_EQ(0u, offset1);

  // Allocate half a chunk worth of memory again.
  // The same chunk will be used.
  int32_t id2 = -1;
  unsigned int offset2 = 0xFFFFFFFFU;
  void* mem2 = manager_->Alloc(kChunkSize, &id2, &offset2);
  ASSERT_TRUE(mem2);
  EXPECT_NE(-1, id2);
  EXPECT_EQ(0u, offset2);

  // Expect two chunks to be allocated, exceeding the limit,
  // since all memory is in use.
  EXPECT_EQ(2 * kChunkSize, manager_->allocated_memory());

  manager_->Free(mem1);
  manager_->Free(mem2);
}

TEST_F(MappedMemoryManagerTest, MemoryLimitWithReuse) {
  const unsigned int kSize = 1024;
  // Reset the manager with a memory limit.
  manager_ = std::make_unique<MappedMemoryManager>(helper_.get(), kSize);
  const unsigned int kChunkSize = 2 * 1024;
  manager_->set_chunk_size_multiple(kChunkSize);

  // Allocate half a chunk worth of memory.
  int32_t id1 = -1;
  unsigned int offset1 = 0xFFFFFFFFU;
  void* mem1 = manager_->Alloc(kSize, &id1, &offset1);
  ASSERT_TRUE(mem1);
  EXPECT_NE(-1, id1);
  EXPECT_EQ(0u, offset1);

  // Allocate half a chunk worth of memory again.
  // The same chunk will be used.
  int32_t id2 = -1;
  unsigned int offset2 = 0xFFFFFFFFU;
  void* mem2 = manager_->Alloc(kSize, &id2, &offset2);
  ASSERT_TRUE(mem2);
  EXPECT_NE(-1, id2);
  EXPECT_EQ(kSize, offset2);

  // Free one successful allocation, pending fence.
  int32_t token = helper_.get()->InsertToken();
  manager_->FreePendingToken(mem2, token);

  // The way we hooked up the helper and engine, it won't process commands
  // until it has to wait for something. Which means the token shouldn't have
  // passed yet at this point.
  EXPECT_GT(token, GetToken());

  // Since we didn't call helper_.finish() the token did not pass.
  // We won't be able to claim the free memory without waiting and
  // as we've already met the memory limit we'll have to wait
  // on the token.
  int32_t id3 = -1;
  unsigned int offset3 = 0xFFFFFFFFU;
  void* mem3 = manager_->Alloc(kSize, &id3, &offset3);
  ASSERT_TRUE(mem3);
  EXPECT_NE(-1, id3);
  // It will reuse the space from the second allocation just freed.
  EXPECT_EQ(kSize, offset3);

  // Expect one chunk to be allocated
  EXPECT_EQ(1 * kChunkSize, manager_->allocated_memory());

  manager_->Free(mem1);
  manager_->Free(mem3);
}

TEST_F(MappedMemoryManagerTest, MaxAllocationTest) {
  const unsigned int kSize = 1024;
  // Reset the manager with a memory limit.
  manager_ = std::make_unique<MappedMemoryManager>(helper_.get(), kSize);

  const size_t kLimit = 512;
  manager_->set_chunk_size_multiple(kLimit);

  // Allocate twice the limit worth of memory (currently unbounded).
  int32_t id1 = -1;
  unsigned int offset1 = 0xFFFFFFFFU;
  void* mem1 = manager_->Alloc(kLimit, &id1, &offset1);
  ASSERT_TRUE(mem1);
  EXPECT_NE(-1, id1);
  EXPECT_EQ(0u, offset1);

  int32_t id2 = -1;
  unsigned int offset2 = 0xFFFFFFFFU;
  void* mem2 = manager_->Alloc(kLimit, &id2, &offset2);
  ASSERT_TRUE(mem2);
  EXPECT_NE(-1, id2);
  EXPECT_EQ(0u, offset2);

  manager_->set_max_allocated_bytes(kLimit);

  // A new allocation should now fail.
  int32_t id3 = -1;
  unsigned int offset3 = 0xFFFFFFFFU;
  void* mem3 = manager_->Alloc(kLimit, &id3, &offset3);
  ASSERT_FALSE(mem3);
  EXPECT_EQ(-1, id3);
  EXPECT_EQ(0xFFFFFFFFU, offset3);

  manager_->Free(mem2);

  // New allocation is over the limit but should reuse allocated space
  int32_t id4 = -1;
  unsigned int offset4 = 0xFFFFFFFFU;
  void* mem4 = manager_->Alloc(kLimit, &id4, &offset4);
  ASSERT_TRUE(mem4);
  EXPECT_EQ(id2, id4);
  EXPECT_EQ(offset2, offset4);

  manager_->Free(mem1);
  manager_->Free(mem4);
}

}  // namespace gpu
