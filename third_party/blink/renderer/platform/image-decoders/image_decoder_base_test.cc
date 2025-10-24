// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/image-decoders/image_decoder_base_test.h"

#include <stddef.h>

#include <memory>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"

// Uncomment this to recalculate the MD5 sums; see header comments.
// #define CALCULATE_MD5_SUMS

namespace {

const int kFirstFrameIndex = 0;

// Determine if we should test with file specified by |path| based
// on |file_selection| and the |threshold| for the file size.
bool ShouldSkipFile(const base::FilePath& path,
                    blink::ImageDecoderBaseTest::FileSelection file_selection,
                    const int64_t threshold) {
  if (file_selection == blink::ImageDecoderBaseTest::FileSelection::kAll) {
    return false;
  }

  int64_t image_size = base::GetFileSize(path).value_or(0);
  return (file_selection ==
          blink::ImageDecoderBaseTest::FileSelection::kSmaller) ==
         (image_size > threshold);
}

void ReadFileToVector(const base::FilePath& path, Vector<char>* contents) {
  std::string raw_image_data;
  base::ReadFileToString(path, &raw_image_data);
  contents->resize(raw_image_data.size());
  memcpy(&contents->front(), raw_image_data.data(), raw_image_data.size());
}

base::MD5Digest ComputeMD5Sum(const blink::ImageFrame& frame_buffer) {
  SkBitmap bitmap = frame_buffer.Bitmap();
  base::MD5Digest digest;
  base::MD5Sum(base::span(static_cast<const uint8_t*>(bitmap.getPixels()),
                          bitmap.computeByteSize()),
               &digest);
  return digest;
}

#if defined(CALCULATE_MD5_SUMS)
void SaveMD5Sum(const base::FilePath& path,
                const blink::ImageFrame* frame_buffer) {
  // Calculate MD5 sum.
  ASSERT_TRUE(frame_buffer);
  base::MD5Digest digest = ComputeMD5Sum(*frame_buffer);

  // Write sum to disk.
  ASSERT_TRUE(base::WriteFile(path, base::byte_span_from_ref(digest)));
}
#endif

#if !defined(CALCULATE_MD5_SUMS)
void VerifyImage(blink::ImageDecoder& decoder,
                 const base::FilePath& path,
                 const base::FilePath& md5_sum_path,
                 size_t frame_index) {
  SCOPED_TRACE(path.value());
  // Make sure decoding can complete successfully.
  EXPECT_TRUE(decoder.IsSizeAvailable());
  EXPECT_GE(decoder.FrameCount(), frame_index);
  blink::ImageFrame* const frame_buffer =
      decoder.DecodeFrameBufferAtIndex(frame_index);
  ASSERT_TRUE(frame_buffer);
  EXPECT_EQ(blink::ImageFrame::kFrameComplete, frame_buffer->GetStatus());
  EXPECT_FALSE(decoder.Failed());

  // Calculate MD5 sum.
  base::MD5Digest actual_digest = ComputeMD5Sum(*frame_buffer);

  // Read the MD5 sum off disk.
  std::string file_bytes;
  base::ReadFileToString(md5_sum_path, &file_bytes);
  base::MD5Digest expected_digest;
  ASSERT_EQ(sizeof expected_digest, file_bytes.size());
  memcpy(&expected_digest, file_bytes.data(), sizeof expected_digest);

  // Verify that the sums are the same.
  EXPECT_EQ(0, memcmp(&expected_digest, &actual_digest, sizeof actual_digest));
}
#endif

}  // namespace

namespace blink {

void ImageDecoderBaseTest::SetUp() {
  base::FilePath data_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &data_dir));
  data_dir_ = data_dir.AppendASCII("webkit").AppendASCII("data").AppendASCII(
      format_.Utf8() + "_decoder");
}

base::FilePath ImageDecoderBaseTest::GetMD5SumPath(const base::FilePath& path) {
  static const base::FilePath::StringType kDecodedDataExtension(
      FILE_PATH_LITERAL(".md5sum"));
  return base::FilePath(path.value() + kDecodedDataExtension);
}

Vector<base::FilePath> ImageDecoderBaseTest::GetImageFiles() const {
  Vector<base::FilePath> image_files;
  if (!base::PathExists(data_dir_)) {
    return image_files;
  }
  std::string pattern = "*." + format_.Utf8();
  base::FileEnumerator enumerator(data_dir_, false,
                                  base::FileEnumerator::FILES);
  for (base::FilePath next_file_name = enumerator.Next();
       !next_file_name.empty(); next_file_name = enumerator.Next()) {
    base::FilePath base_name = next_file_name.BaseName();
    std::string base_name_ascii = base_name.MaybeAsASCII();
    if (base::MatchPattern(base_name_ascii, pattern)) {
      image_files.push_back(next_file_name);
    }
  }

  return image_files;
}

bool ImageDecoderBaseTest::ShouldImageFail(const base::FilePath& path) const {
  const base::FilePath::StringType kBadSuffix(FILE_PATH_LITERAL(".bad."));
  return (path.value().length() > (kBadSuffix.length() + format_.length()) &&
          !path.value().compare(
              path.value().length() - format_.length() - kBadSuffix.length(),
              kBadSuffix.length(), kBadSuffix));
}

void ImageDecoderBaseTest::TestDecoding(
    blink::ImageDecoderBaseTest::FileSelection file_selection,
    const int64_t threshold) {
  const Vector<base::FilePath> image_files = GetImageFiles();
  if (image_files.empty()) {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    VLOG(0) << "TestDecoding() in " << test_info->test_suite_name() << "."
            << test_info->name()
            << " not running because test data wasn't found.";
    return;
  }
  for (const base::FilePath& file : image_files) {
    if (!ShouldSkipFile(file, file_selection, threshold)) {
      TestImageDecoder(file, GetMD5SumPath(file), kFirstFrameIndex);
    }
  }
}

void ImageDecoderBaseTest::TestImageDecoder(const base::FilePath& image_path,
                                            const base::FilePath& md5_sum_path,
                                            int desired_frame_index) const {
#if defined(CALCULATE_MD5_SUMS)
  // If we're just calculating the MD5 sums, skip failing images quickly.
  if (ShouldImageFail(image_path)) {
    return;
  }
#endif

  CHECK(base::PathExists(image_path)) << image_path;
  CHECK(ShouldImageFail(image_path) || base::PathExists(md5_sum_path))
      << md5_sum_path;
  Vector<char> image_contents;
  ReadFileToVector(image_path, &image_contents);
  EXPECT_TRUE(image_contents.size());
  std::unique_ptr<ImageDecoder> decoder(CreateImageDecoder());
  EXPECT_FALSE(decoder->Failed());

#if !defined(CALCULATE_MD5_SUMS)
  // Test chunking file into half.
  const size_t partial_size = image_contents.size() / 2;

  scoped_refptr<SharedBuffer> partial_data =
      SharedBuffer::Create(base::span(image_contents).first(partial_size));

  // Make Sure the image decoder doesn't fail when we ask for the frame
  // buffer for this partial image.
  // NOTE: We can't check that frame 0 is non-NULL, because if this is an
  // ICO and we haven't yet supplied enough data to read the directory,
  // there is no framecount and thus no first frame.
  decoder->SetData(partial_data, false);
  EXPECT_FALSE(decoder->Failed()) << image_path.value();
#endif

  // Make sure passing the complete image results in successful decoding.
  scoped_refptr<SharedBuffer> data = SharedBuffer::Create(image_contents);
  decoder->SetData(data, true);
  if (ShouldImageFail(image_path)) {
    blink::ImageFrame* const frame_buffer =
        decoder->DecodeFrameBufferAtIndex(kFirstFrameIndex);
    if (kFirstFrameIndex < decoder->FrameCount()) {
      EXPECT_TRUE(frame_buffer);
      EXPECT_NE(blink::ImageFrame::kFrameComplete, frame_buffer->GetStatus());
    } else {
      EXPECT_FALSE(frame_buffer);
    }
    EXPECT_TRUE(decoder->Failed());
  } else {
    EXPECT_FALSE(decoder->Failed()) << image_path.value();
#if defined(CALCULATE_MD5_SUMS)
    SaveMD5Sum(md5_sum_path,
               decoder->DecodeFrameBufferAtIndex(desired_frame_index));
#else
    VerifyImage(*decoder, image_path, md5_sum_path, desired_frame_index);
#endif
  }
}

}  // namespace blink
