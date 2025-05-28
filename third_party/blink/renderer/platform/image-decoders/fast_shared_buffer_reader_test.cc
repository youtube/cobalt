// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/image-decoders/fast_shared_buffer_reader.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/image-decoders/rw_buffer.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/skia/include/core/SkData.h"

namespace blink {

namespace {

scoped_refptr<SegmentReader> CopyToROBufferSegmentReader(
    scoped_refptr<SegmentReader> input) {
  RWBuffer rw_buffer;
  size_t position = 0;
  for (base::span<const uint8_t> segment = input->GetSomeData(position);
       !segment.empty(); segment = input->GetSomeData(position)) {
    rw_buffer.Append(segment);
    position += segment.size();
  }
  return SegmentReader::CreateFromROBuffer(rw_buffer.MakeROBufferSnapshot());
}

scoped_refptr<SegmentReader> CopyToDataSegmentReader(
    scoped_refptr<SegmentReader> input) {
  return SegmentReader::CreateFromSkData(input->GetAsSkData());
}

struct SegmentReaders {
  scoped_refptr<SegmentReader> segment_readers[3];

  explicit SegmentReaders(scoped_refptr<SharedBuffer> input) {
    segment_readers[0] =
        SegmentReader::CreateFromSharedBuffer(std::move(input));
    segment_readers[1] = CopyToROBufferSegmentReader(segment_readers[0]);
    segment_readers[2] = CopyToDataSegmentReader(segment_readers[0]);
  }
};

}  // namespace

TEST(FastSharedBufferReaderTest, nonSequentialReads) {
  char reference_data[kDefaultTestSize];
  PrepareReferenceData(reference_data);
  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();
  data->Append(reference_data);

  SegmentReaders reader_struct(data);
  for (auto segment_reader : reader_struct.segment_readers) {
    FastSharedBufferReader reader(segment_reader);
    // Read size is prime such there will be a segment-spanning
    // read eventually.
    char temp_buffer[17];
    for (size_t data_position = 0;
         data_position + sizeof(temp_buffer) < sizeof(reference_data);
         data_position += sizeof(temp_buffer)) {
      const char* block = reader.GetConsecutiveData(
          data_position, sizeof(temp_buffer), temp_buffer);
      ASSERT_FALSE(
          memcmp(block, reference_data + data_position, sizeof(temp_buffer)));
    }
  }
}

TEST(FastSharedBufferReaderTest, readBackwards) {
  char reference_data[kDefaultTestSize];
  PrepareReferenceData(reference_data);
  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();
  data->Append(reference_data);

  SegmentReaders reader_struct(data);
  for (auto segment_reader : reader_struct.segment_readers) {
    FastSharedBufferReader reader(segment_reader);
    // Read size is prime such there will be a segment-spanning
    // read eventually.
    char temp_buffer[17];
    for (size_t data_offset = sizeof(temp_buffer);
         data_offset < sizeof(reference_data);
         data_offset += sizeof(temp_buffer)) {
      const char* block =
          reader.GetConsecutiveData(sizeof(reference_data) - data_offset,
                                    sizeof(temp_buffer), temp_buffer);
      ASSERT_FALSE(memcmp(block,
                          reference_data + sizeof(reference_data) - data_offset,
                          sizeof(temp_buffer)));
    }
  }
}

TEST(FastSharedBufferReaderTest, byteByByte) {
  char reference_data[kDefaultTestSize];
  PrepareReferenceData(reference_data);
  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();
  data->Append(reference_data);

  SegmentReaders reader_struct(data);
  for (auto segment_reader : reader_struct.segment_readers) {
    FastSharedBufferReader reader(segment_reader);
    for (size_t i = 0; i < sizeof(reference_data); ++i) {
      ASSERT_EQ(reference_data[i], reader.GetOneByte(i));
    }
  }
}

// Tests that a read from inside the penultimate segment to the very end of the
// buffer doesn't try to read off the end of the buffer.
TEST(FastSharedBufferReaderTest, readAllOverlappingLastSegmentBoundary) {
  const unsigned kDataSize = 2 * kDefaultSegmentTestSize;
  char reference_data[kDataSize];
  PrepareReferenceData(reference_data);
  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();
  data->Append(reference_data);

  SegmentReaders reader_struct(data);
  for (auto segment_reader : reader_struct.segment_readers) {
    FastSharedBufferReader reader(segment_reader);
    char buffer[kDataSize] = {};
    const char* result = reader.GetConsecutiveData(0, kDataSize, buffer);
    ASSERT_FALSE(memcmp(result, reference_data, kDataSize));
  }
}

// Verify that reading past the end of the buffer does not break future reads.
TEST(SegmentReaderTest, readPastEndThenRead) {
  const unsigned kDataSize = 2 * kDefaultSegmentTestSize;
  char reference_data[kDataSize];
  PrepareReferenceData(reference_data);
  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();
  data->Append(base::span(reference_data).first(kDefaultSegmentTestSize));
  data->Append(base::span(reference_data)
                   .subspan(kDefaultSegmentTestSize, kDefaultSegmentTestSize));

  SegmentReaders reader_struct(data);
  for (auto segment_reader : reader_struct.segment_readers) {
    base::span<const uint8_t> contents = segment_reader->GetSomeData(kDataSize);
    EXPECT_TRUE(contents.empty());

    contents = segment_reader->GetSomeData(0);
    EXPECT_LE(kDefaultSegmentTestSize, contents.size());
  }
}

TEST(SegmentReaderTest, getAsSkData) {
  const unsigned kDataSize = 4 * kDefaultSegmentTestSize;
  char reference_data[kDataSize];
  PrepareReferenceData(reference_data);
  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();
  for (size_t i = 0; i < 4; ++i) {
    data->Append(
        base::span(reference_data)
            .subspan(i * kDefaultSegmentTestSize, kDefaultSegmentTestSize));
  }
  SegmentReaders reader_struct(data);
  for (auto segment_reader : reader_struct.segment_readers) {
    sk_sp<SkData> skdata = segment_reader->GetAsSkData();
    EXPECT_EQ(data->size(), skdata->size());
    auto skdata_span = base::span(skdata->bytes(), skdata->size());

    size_t position = 0;
    for (base::span<const uint8_t> segment =
             segment_reader->GetSomeData(position);
         !segment.empty(); segment = segment_reader->GetSomeData(position)) {
      ASSERT_LE(position, skdata_span.size());
      ASSERT_LE(segment.size(), skdata_span.size() - position);
      EXPECT_EQ(segment, skdata_span.subspan(position, segment.size()));
      position += segment.size();
    }
    EXPECT_EQ(position, kDataSize);
  }
}

TEST(SegmentReaderTest, variableSegments) {
  const size_t kDataSize = 3.5 * kDefaultSegmentTestSize;
  char reference_data[kDataSize];
  PrepareReferenceData(reference_data);
  auto reference_data_span = base::as_byte_span(reference_data);

  scoped_refptr<SegmentReader> segment_reader;
  {
    // Create a SegmentReader with difference sized segments, to test that
    // the ROBuffer implementation works when two consecutive segments
    // are not the same size. This test relies on knowledge of the
    // internals of RWBuffer: it ensures that each segment is at least
    // 4096 (though the actual data may be smaller, if it has not been
    // written to yet), but when appending a larger amount it may create a
    // larger segment.
    RWBuffer rw_buffer;
    rw_buffer.Append(reference_data_span.first(kDefaultSegmentTestSize));
    rw_buffer.Append(reference_data_span.subspan(kDefaultSegmentTestSize,
                                                 2 * kDefaultSegmentTestSize));
    rw_buffer.Append(reference_data_span.subspan(3 * kDefaultSegmentTestSize,
                                                 kDefaultSegmentTestSize / 2));

    segment_reader =
        SegmentReader::CreateFromROBuffer(rw_buffer.MakeROBufferSnapshot());
  }

  size_t position = 0;
  size_t last_length = 0;
  for (base::span<const uint8_t> segment =
           segment_reader->GetSomeData(position);
       !segment.empty(); segment = segment_reader->GetSomeData(position)) {
    // It is not a bug to have consecutive segments of the same length, but
    // it does mean that the following test does not actually test what it
    // is intended to test.
    ASSERT_NE(segment.size(), last_length);
    last_length = segment.size();

    ASSERT_LE(position, reference_data_span.size());
    ASSERT_LE(segment.size(), reference_data_span.size() - position);
    EXPECT_EQ(segment, reference_data_span.subspan(position, segment.size()));
    position += segment.size();
  }
  EXPECT_EQ(position, kDataSize);
}

}  // namespace blink
