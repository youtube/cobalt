// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/xhr/fetch_buffer_pool.h"

#include <vector>

#include "cobalt/script/array_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace xhr {
namespace {

void FillTestData(std::vector<uint8_t>* buffer) {
  for (size_t i = 0; i < buffer->size(); ++i) {
    buffer->at(i) = static_cast<char>(i);
  }
}

void AppendBuffersTo(
    const std::vector<script::PreallocatedArrayBufferData>& sources,
    std::vector<uint8_t>* destination) {
  ASSERT_NE(destination, nullptr);

  for (const auto& buffer : sources) {
    destination->insert(destination->end(), buffer.data(),
                        buffer.data() + buffer.byte_length());
  }
}

TEST(FetchBufferPoolTest, Empty) {
  FetchBufferPool empty_buffer_pool;
  EXPECT_EQ(empty_buffer_pool.GetSize(), 0);

  std::vector<script::PreallocatedArrayBufferData> buffers;

  empty_buffer_pool.ResetAndReturnAsArrayBuffers(false, &buffers);
  EXPECT_TRUE(buffers.empty());

  empty_buffer_pool.ResetAndReturnAsArrayBuffers(true, &buffers);
  EXPECT_TRUE(buffers.empty());

  empty_buffer_pool.Write(nullptr, 0);  // still empty
  EXPECT_EQ(empty_buffer_pool.GetSize(), 0);

  empty_buffer_pool.ResetAndReturnAsArrayBuffers(false, &buffers);
  EXPECT_TRUE(buffers.empty());

  empty_buffer_pool.ResetAndReturnAsArrayBuffers(true, &buffers);
  EXPECT_TRUE(buffers.empty());
}

TEST(FetchBufferPoolTest, SunnyDay) {
  FetchBufferPool fetch_buffer_pool;

  std::vector<uint8_t> source_buffer(1024 * 1024 + 1023);
  FillTestData(&source_buffer);

  std::vector<uint8_t> reference_buffer;
  for (int i = 0; i < 10; ++i) {
    fetch_buffer_pool.Write(source_buffer.data(),
                            static_cast<int>(source_buffer.size()));
    reference_buffer.insert(reference_buffer.end(), source_buffer.begin(),
                            source_buffer.end());
  }

  std::vector<script::PreallocatedArrayBufferData> buffers;
  fetch_buffer_pool.ResetAndReturnAsArrayBuffers(true, &buffers);
  ASSERT_FALSE(buffers.empty());
  ASSERT_EQ(fetch_buffer_pool.GetSize(), 0);

  std::vector<uint8_t> collected_buffer;
  AppendBuffersTo(buffers, &collected_buffer);
  ASSERT_EQ(collected_buffer, reference_buffer);
}

TEST(FetchBufferPoolTest, Clear) {
  FetchBufferPool fetch_buffer_pool;

  std::vector<uint8_t> source_buffer(1024);
  FillTestData(&source_buffer);

  fetch_buffer_pool.Write(source_buffer.data(),
                          static_cast<int>(source_buffer.size()));
  EXPECT_EQ(fetch_buffer_pool.GetSize(), source_buffer.size());

  fetch_buffer_pool.Clear();

  EXPECT_EQ(fetch_buffer_pool.GetSize(), 0);

  std::vector<script::PreallocatedArrayBufferData> buffers;
  fetch_buffer_pool.ResetAndReturnAsArrayBuffers(false, &buffers);
  ASSERT_TRUE(buffers.empty());
  fetch_buffer_pool.ResetAndReturnAsArrayBuffers(true, &buffers);
  ASSERT_TRUE(buffers.empty());
}

TEST(FetchBufferPoolTest, IncrementalWriteAndRead) {
  FetchBufferPool fetch_buffer_pool;

  std::vector<uint8_t> source_buffer(4097);
  FillTestData(&source_buffer);

  std::vector<uint8_t> reference_buffer;
  std::vector<uint8_t> collected_buffer;
  while (reference_buffer.size() < 1024 * 1024 * 10) {
    int bytes_to_write =
        static_cast<int>((reference_buffer.size() + 1) % source_buffer.size());
    fetch_buffer_pool.Write(source_buffer.data(), bytes_to_write);
    reference_buffer.insert(reference_buffer.end(), source_buffer.begin(),
                            source_buffer.begin() + bytes_to_write);

    std::vector<script::PreallocatedArrayBufferData> buffers;
    fetch_buffer_pool.ResetAndReturnAsArrayBuffers(false, &buffers);
    ASSERT_FALSE(buffers.empty());
    AppendBuffersTo(buffers, &collected_buffer);
  }

  std::vector<script::PreallocatedArrayBufferData> buffers;
  fetch_buffer_pool.ResetAndReturnAsArrayBuffers(true, &buffers);
  ASSERT_EQ(fetch_buffer_pool.GetSize(), 0);
  AppendBuffersTo(buffers, &collected_buffer);

  ASSERT_EQ(collected_buffer, reference_buffer);
}

TEST(FetchBufferPoolTest, SingleLargeWrite) {
  FetchBufferPool fetch_buffer_pool;

  std::vector<uint8_t> large_source_buffer(1024 * 1024 * 16 + 1023);
  FillTestData(&large_source_buffer);

  {
    std::vector<uint8_t> reference_buffer;
    std::vector<uint8_t> collected_buffer;

    fetch_buffer_pool.Write(large_source_buffer.data(),
                            static_cast<int>(large_source_buffer.size()));
    reference_buffer.insert(reference_buffer.end(), large_source_buffer.begin(),
                            large_source_buffer.end());

    std::vector<script::PreallocatedArrayBufferData> buffers;
    fetch_buffer_pool.ResetAndReturnAsArrayBuffers(true, &buffers);
    ASSERT_FALSE(buffers.empty());
    ASSERT_EQ(fetch_buffer_pool.GetSize(), 0);

    AppendBuffersTo(buffers, &collected_buffer);
    ASSERT_EQ(collected_buffer, reference_buffer);
  }
}

TEST(FetchBufferPoolTest, MixedSizeWrites) {
  FetchBufferPool fetch_buffer_pool;

  std::vector<uint8_t> small_source_buffer(1023);
  std::vector<uint8_t> large_source_buffer(1024 * 1024 * 16 + 1023);

  FillTestData(&small_source_buffer);
  FillTestData(&large_source_buffer);

  {
    // Small then large
    std::vector<uint8_t> reference_buffer;
    std::vector<uint8_t> collected_buffer;

    fetch_buffer_pool.Write(small_source_buffer.data(),
                            static_cast<int>(small_source_buffer.size()));
    reference_buffer.insert(reference_buffer.end(), small_source_buffer.begin(),
                            small_source_buffer.end());

    fetch_buffer_pool.Write(large_source_buffer.data(),
                            static_cast<int>(large_source_buffer.size()));
    reference_buffer.insert(reference_buffer.end(), large_source_buffer.begin(),
                            large_source_buffer.end());

    std::vector<script::PreallocatedArrayBufferData> buffers;
    fetch_buffer_pool.ResetAndReturnAsArrayBuffers(false, &buffers);
    ASSERT_FALSE(buffers.empty());
    AppendBuffersTo(buffers, &collected_buffer);

    fetch_buffer_pool.ResetAndReturnAsArrayBuffers(true, &buffers);
    ASSERT_EQ(fetch_buffer_pool.GetSize(), 0);
    AppendBuffersTo(buffers, &collected_buffer);

    ASSERT_EQ(collected_buffer, reference_buffer);
  }

  {
    // Large then small
    std::vector<uint8_t> reference_buffer;
    std::vector<uint8_t> collected_buffer;

    fetch_buffer_pool.Write(large_source_buffer.data(),
                            static_cast<int>(large_source_buffer.size()));
    reference_buffer.insert(reference_buffer.end(), large_source_buffer.begin(),
                            large_source_buffer.end());

    fetch_buffer_pool.Write(small_source_buffer.data(),
                            static_cast<int>(small_source_buffer.size()));
    reference_buffer.insert(reference_buffer.end(), small_source_buffer.begin(),
                            small_source_buffer.end());

    std::vector<script::PreallocatedArrayBufferData> buffers;
    fetch_buffer_pool.ResetAndReturnAsArrayBuffers(false, &buffers);
    ASSERT_FALSE(buffers.empty());
    AppendBuffersTo(buffers, &collected_buffer);

    fetch_buffer_pool.ResetAndReturnAsArrayBuffers(true, &buffers);
    ASSERT_EQ(fetch_buffer_pool.GetSize(), 0);
    AppendBuffersTo(buffers, &collected_buffer);

    ASSERT_EQ(collected_buffer, reference_buffer);
  }
}

}  // namespace
}  // namespace xhr
}  // namespace cobalt
