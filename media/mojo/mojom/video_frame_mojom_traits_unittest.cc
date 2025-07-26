// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/mojo/mojom/video_frame_mojom_traits.h"

#include <algorithm>
#include <array>

#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "media/base/color_plane_layout.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/mojo/mojom/traits_test_service.test-mojom.h"
#include "media/video/fake_gpu_memory_buffer.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

namespace {

class VideoFrameStructTraitsTest : public testing::Test,
                                   public media::mojom::TraitsTestService {
 public:
  VideoFrameStructTraitsTest() = default;

  VideoFrameStructTraitsTest(const VideoFrameStructTraitsTest&) = delete;
  VideoFrameStructTraitsTest& operator=(const VideoFrameStructTraitsTest&) =
      delete;

 protected:
  mojo::Remote<mojom::TraitsTestService> GetTraitsTestRemote() {
    mojo::Remote<mojom::TraitsTestService> remote;
    traits_test_receivers_.Add(this, remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  bool RoundTrip(scoped_refptr<VideoFrame>* frame) {
    scoped_refptr<VideoFrame> input = std::move(*frame);
    mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
    return remote->EchoVideoFrame(std::move(input), frame);
  }

 private:
  void EchoVideoFrame(const scoped_refptr<VideoFrame>& f,
                      EchoVideoFrameCallback callback) override {
    // Touch all data in the received frame to ensure that it is valid.
    if (f && f->IsMappable()) {
      base::MD5Context md5_context;
      base::MD5Init(&md5_context);
      VideoFrame::HashFrameForTesting(&md5_context, *f);
    }

    std::move(callback).Run(f);
  }

  base::test::TaskEnvironment task_environment_;
  mojo::ReceiverSet<TraitsTestService> traits_test_receivers_;
};

}  // namespace

TEST_F(VideoFrameStructTraitsTest, Null) {
  scoped_refptr<VideoFrame> frame;

  ASSERT_TRUE(RoundTrip(&frame));
  EXPECT_FALSE(frame);
}

TEST_F(VideoFrameStructTraitsTest, EOS) {
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateEOSFrame();

  ASSERT_TRUE(RoundTrip(&frame));
  ASSERT_TRUE(frame);
  EXPECT_TRUE(frame->metadata().end_of_stream);
}

TEST_F(VideoFrameStructTraitsTest, MappableVideoFrame) {
  constexpr VideoFrame::StorageType storage_types[] = {
      VideoFrame::STORAGE_SHMEM,
      VideoFrame::STORAGE_OWNED_MEMORY,
      VideoFrame::STORAGE_UNOWNED_MEMORY,
  };
  constexpr VideoPixelFormat formats[] = {PIXEL_FORMAT_I420, PIXEL_FORMAT_NV12};
  constexpr gfx::Size kCodedSize(100, 100);
  constexpr gfx::Rect kVisibleRect(kCodedSize);
  constexpr gfx::Size kNaturalSize = kCodedSize;
  constexpr double kFrameRate = 42.0;
  constexpr base::TimeDelta kTimestamp = base::Seconds(100);
  for (auto format : formats) {
    for (auto storage_type : storage_types) {
      scoped_refptr<media::VideoFrame> frame;
      base::MappedReadOnlyRegion region;
      if (storage_type == VideoFrame::STORAGE_OWNED_MEMORY) {
        frame = media::VideoFrame::CreateFrame(format, kCodedSize, kVisibleRect,
                                               kNaturalSize, kTimestamp);
      } else {
        std::vector<size_t> strides =
            VideoFrame::ComputeStrides(format, kCodedSize);
        size_t aggregate_size = 0;
        std::array<size_t, 3> sizes = {};
        for (size_t i = 0; i < strides.size(); ++i) {
          sizes[i] = media::VideoFrame::Rows(i, format, kCodedSize.height()) *
                     strides[i];
          aggregate_size += sizes[i];
        }
        region = base::ReadOnlySharedMemoryRegion::Create(aggregate_size);
        ASSERT_TRUE(region.IsValid());

        std::array<uint8_t*, 3> data = {};
        data[0] = const_cast<uint8_t*>(region.mapping.GetMemoryAs<uint8_t>());
        for (size_t i = 1; i < strides.size(); ++i) {
          data[i] = data[i - 1] + sizes[i - 1];
        }

        if (format == PIXEL_FORMAT_I420) {
          frame = media::VideoFrame::WrapExternalYuvData(
              format, kCodedSize, kVisibleRect, kNaturalSize, strides[0],
              strides[1], strides[2], data[0], data[1], data[2], kTimestamp);
        } else {
          frame = media::VideoFrame::WrapExternalYuvData(
              format, kCodedSize, kVisibleRect, kNaturalSize, strides[0],
              strides[1], data[0], data[1], kTimestamp);
        }
        if (storage_type == VideoFrame::STORAGE_SHMEM)
          frame->BackWithSharedMemory(&region.region);
      }

      ASSERT_TRUE(frame);
      frame->metadata().frame_rate = kFrameRate;
      ASSERT_EQ(frame->storage_type(), storage_type);
      ASSERT_TRUE(RoundTrip(&frame));
      ASSERT_TRUE(frame);
      EXPECT_FALSE(frame->metadata().end_of_stream);
      EXPECT_EQ(frame->format(), format);
      EXPECT_EQ(*frame->metadata().frame_rate, kFrameRate);
      EXPECT_EQ(frame->coded_size(), kCodedSize);
      EXPECT_EQ(frame->visible_rect(), kVisibleRect);
      EXPECT_EQ(frame->natural_size(), kNaturalSize);
      EXPECT_EQ(frame->timestamp(), kTimestamp);
      ASSERT_EQ(frame->storage_type(), VideoFrame::STORAGE_SHMEM);
      EXPECT_TRUE(frame->shm_region()->IsValid());
    }
  }
}

TEST_F(VideoFrameStructTraitsTest, InterleavedPlanes) {
  constexpr VideoFrame::StorageType storage_type = VideoFrame::STORAGE_SHMEM;
  constexpr VideoPixelFormat format = PIXEL_FORMAT_I420;
  constexpr gfx::Size kCodedSize(100, 100);
  constexpr gfx::Rect kVisibleRect(kCodedSize);
  constexpr gfx::Size kNaturalSize = kCodedSize;
  constexpr base::TimeDelta kTimestamp;

  scoped_refptr<media::VideoFrame> frame;

  std::vector<size_t> strides = VideoFrame::ComputeStrides(format, kCodedSize);
  ASSERT_EQ(strides[1], strides[2]);

  size_t aggregate_size = 0;
  std::array<size_t, 3> sizes = {};
  for (size_t i = 0; i < strides.size(); ++i) {
    sizes[i] =
        media::VideoFrame::Rows(i, format, kCodedSize.height()) * strides[i];
    aggregate_size += sizes[i];
  }
  auto region = base::WritableSharedMemoryRegion::Create(aggregate_size);
  ASSERT_TRUE(region.IsValid());
  auto mapping = region.MapAt(0, aggregate_size);

  auto [y_plane, uv_plane] =
      mapping.GetMemoryAsSpan<uint8_t>().split_at(sizes[0]);
  std::ranges::fill(y_plane, 1);

  // Setup memory layout where U and V planes occupy the same space, but have
  // interleaving U and V rows. This is achieved by doubling the stride.
  size_t normal_stride = strides[1];
  size_t uv_stride = normal_stride * 2;

  int yu_rows = media::VideoFrame::Rows(1, format, kCodedSize.height());
  auto uv_plane2 = uv_plane;  // Loop below is destructive.
  for (int i = 0; i < yu_rows; ++i) {
    const auto [u, v] = uv_plane2.take_first(uv_stride).split_at(normal_stride);
    std::ranges::fill(u, 2);
    std::ranges::fill(v, 3);
  }

  frame = media::VideoFrame::WrapExternalYuvData(
      format, kCodedSize, kVisibleRect, kNaturalSize, strides[0], uv_stride,
      uv_stride, y_plane.data(), uv_plane.data(),
      uv_plane.subspan(normal_stride).data(), kTimestamp);
  auto ro_region =
      base::WritableSharedMemoryRegion::ConvertToReadOnly(std::move(region));
  frame->BackWithSharedMemory(&ro_region);

  EXPECT_TRUE(frame);
  EXPECT_EQ(frame->storage_type(), storage_type);
  EXPECT_TRUE(RoundTrip(&frame));
  EXPECT_TRUE(frame);
  EXPECT_EQ(frame->format(), format);
  EXPECT_EQ(frame->coded_size(), kCodedSize);

  auto plane_1 = frame->GetVisiblePlaneData(1);
  auto plane_2 = frame->GetVisiblePlaneData(2);
  // Bytes between the visible edge and the full stride are not considered part
  // of the visible plane, and may not be accessible through the above spans.
  const size_t row_bytes_1 =
      VideoFrame::RowBytes(1, format, kCodedSize.width());
  const size_t row_bytes_2 =
      VideoFrame::RowBytes(2, format, kCodedSize.width());
  for (int i = 0; i < yu_rows; ++i) {
    const auto [u, v] = uv_plane.take_first(uv_stride).split_at(normal_stride);
    EXPECT_EQ(plane_1.subspan(i * frame->stride(1), row_bytes_1),
              u.first(row_bytes_1));
    EXPECT_EQ(plane_2.subspan(i * frame->stride(2), row_bytes_2),
              v.first(row_bytes_2));
  }
}

TEST_F(VideoFrameStructTraitsTest, InvalidOffsets) {
  constexpr auto kFormat = PIXEL_FORMAT_I420;

  // This test works by patching the outgoing mojo message, so choose a size
  // that's two primes to try and maximize the uniqueness of the values we're
  // scanning for in the message.
  constexpr gfx::Size kSize(127, 149);

  auto strides = VideoFrame::ComputeStrides(kFormat, kSize);
  size_t aggregate_size = 0;
  std::array<size_t, 3> sizes = {};
  for (size_t i = 0; i < strides.size(); ++i) {
    sizes[i] = VideoFrame::Rows(i, kFormat, kSize.height()) * strides[i];
    aggregate_size += sizes[i];
  }

  auto region = base::ReadOnlySharedMemoryRegion::Create(aggregate_size);
  ASSERT_TRUE(region.IsValid());

  std::array<uint8_t*, 3> data = {};
  data[0] = const_cast<uint8_t*>(region.mapping.GetMemoryAs<uint8_t>());
  for (size_t i = 1; i < strides.size(); ++i) {
    data[i] = data[i - 1] + sizes[i];
  }

  auto frame = VideoFrame::WrapExternalYuvData(
      kFormat, kSize, gfx::Rect(kSize), kSize, strides[0], strides[1],
      strides[2], data[0], data[1], data[2], base::Seconds(1));
  ASSERT_TRUE(frame);

  frame->BackWithSharedMemory(&region.region);

  auto message = mojom::VideoFrame::SerializeAsMessage(&frame);

  // Scan for the offsets array in the message body. It will start with an
  // array header and then have the three offsets matching our frame.
  base::span<uint32_t> body(
      reinterpret_cast<uint32_t*>(message.mutable_payload()),
      message.payload_num_bytes() / sizeof(uint32_t));
  std::vector<uint32_t> offsets = {
      static_cast<uint32_t>(data[0] - data[0]),  // offsets[0]
      static_cast<uint32_t>(data[1] - data[0]),  // offsets[1]
      static_cast<uint32_t>(data[2] - data[0]),  // offsets[2]
  };

  bool patched_offsets = false;
  for (size_t i = 0; i + 3 < body.size(); ++i) {
    if (body[i] == offsets[0] && body[i + 1] == offsets[1] &&
        body[i + 2] == offsets[2]) {
      body[i + 1] = 0xc01db33f;
      patched_offsets = true;
      break;
    }
  }
  ASSERT_TRUE(patched_offsets);

  // Required to pass base deserialize checks.
  mojo::ScopedMessageHandle handle = message.TakeMojoMessage();
  message = mojo::Message::CreateFromMessageHandle(&handle);

  // Ensure deserialization fails instead of crashing.
  scoped_refptr<VideoFrame> new_frame;
  EXPECT_FALSE(mojom::VideoFrame::DeserializeFromMessage(std::move(message),
                                                         &new_frame));
}

TEST_F(VideoFrameStructTraitsTest, HoleVideoFrame) {
  base::UnguessableToken overlay_plane_id = base::UnguessableToken::Create();
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateVideoHoleFrame(
      overlay_plane_id, gfx::Size(200, 100), base::Seconds(100));

  // Saves the VideoFrame metadata from the created frame. The test should not
  // assume these have any particular value.
  const VideoFrame::StorageType storage_type = frame->storage_type();
  const VideoPixelFormat format = frame->format();

  ASSERT_TRUE(RoundTrip(&frame));
  ASSERT_TRUE(frame);
  EXPECT_FALSE(frame->metadata().end_of_stream);
  EXPECT_EQ(frame->storage_type(), storage_type);
  EXPECT_EQ(frame->format(), format);
  EXPECT_EQ(frame->natural_size(), gfx::Size(200, 100));
  EXPECT_EQ(frame->timestamp(), base::Seconds(100));
  ASSERT_TRUE(frame->metadata().tracking_token.has_value());
  ASSERT_EQ(*frame->metadata().tracking_token, overlay_plane_id);
}

TEST_F(VideoFrameStructTraitsTest, TrackingTokenVideoFrame) {
  base::UnguessableToken tracking_token = base::UnguessableToken::Create();
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapTrackingToken(
      PIXEL_FORMAT_ARGB, tracking_token, gfx::Size(100, 100),
      gfx::Rect(10, 10, 80, 80), gfx::Size(200, 100), base::Seconds(100));

  ASSERT_TRUE(RoundTrip(&frame));
  ASSERT_TRUE(frame);
  EXPECT_FALSE(frame->metadata().end_of_stream);
  EXPECT_EQ(frame->format(), PIXEL_FORMAT_ARGB);
  EXPECT_EQ(frame->coded_size(), gfx::Size(100, 100));
  EXPECT_EQ(frame->visible_rect(), gfx::Rect(10, 10, 80, 80));
  EXPECT_EQ(frame->natural_size(), gfx::Size(200, 100));
  EXPECT_EQ(frame->timestamp(), base::Seconds(100));
  ASSERT_TRUE(frame->metadata().tracking_token.has_value());
  ASSERT_EQ(*frame->metadata().tracking_token, tracking_token);
}

TEST_F(VideoFrameStructTraitsTest, SharedImageVideoFrame) {
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      gpu::ClientSharedImage::CreateForTesting();
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapSharedImage(
      PIXEL_FORMAT_ARGB, shared_image, gpu::SyncToken(),
      VideoFrame::ReleaseMailboxCB(), gfx::Size(100, 100),
      gfx::Rect(10, 10, 80, 80), gfx::Size(200, 100), base::Seconds(100));

  ASSERT_TRUE(RoundTrip(&frame));
  ASSERT_TRUE(frame);
  EXPECT_FALSE(frame->metadata().end_of_stream);
  EXPECT_EQ(frame->format(), PIXEL_FORMAT_ARGB);
  EXPECT_EQ(frame->coded_size(), gfx::Size(100, 100));
  EXPECT_EQ(frame->visible_rect(), gfx::Rect(10, 10, 80, 80));
  EXPECT_EQ(frame->natural_size(), gfx::Size(200, 100));
  EXPECT_EQ(frame->timestamp(), base::Seconds(100));
  ASSERT_TRUE(frame->HasSharedImage());
  ASSERT_EQ(frame->shared_image()->mailbox(), shared_image->mailbox());
}

// BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) because
// media::FakeGpuMemoryBuffer supports NativePixmapHandle backed
// GpuMemoryBufferHandle only. !BUILDFLAG(IS_OZONE) so as to force
// GpuMemoryBufferSupport to select gfx::ClientNativePixmapFactoryDmabuf for
// gfx::ClientNativePixmapFactory.
// TODO(crbug.com/40286368): Allow this test without !BUILDFLAG(IS_OZONE)
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && !BUILDFLAG(IS_OZONE)
TEST_F(VideoFrameStructTraitsTest, GpuMemoryBufferSharedImageVideoFrame) {
  gfx::Size coded_size = gfx::Size(256, 256);
  gfx::Rect visible_rect(coded_size);
  auto timestamp = base::Milliseconds(1);
  std::unique_ptr<gfx::GpuMemoryBuffer> gmb =
      std::make_unique<FakeGpuMemoryBuffer>(
          coded_size, gfx::BufferFormat::YUV_420_BIPLANAR);
  gfx::BufferFormat expected_gmb_format = gmb->GetFormat();
  gfx::Size expected_gmb_size = gmb->GetSize();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      gpu::ClientSharedImage::CreateForTesting();
  auto frame = VideoFrame::WrapExternalGpuMemoryBuffer(
      visible_rect, visible_rect.size(), std::move(gmb), shared_image,
      gpu::SyncToken(), base::NullCallback(), timestamp);
  ASSERT_TRUE(RoundTrip(&frame));
  ASSERT_TRUE(frame);
  ASSERT_EQ(frame->storage_type(), VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
  EXPECT_TRUE(frame->HasMappableGpuBuffer());
  EXPECT_FALSE(frame->metadata().end_of_stream);
  EXPECT_EQ(frame->format(), PIXEL_FORMAT_NV12);
  EXPECT_EQ(frame->coded_size(), coded_size);
  EXPECT_EQ(frame->visible_rect(), visible_rect);
  EXPECT_EQ(frame->natural_size(), visible_rect.size());
  EXPECT_EQ(frame->timestamp(), timestamp);
  ASSERT_TRUE(frame->HasSharedImage());
  EXPECT_EQ(frame->mailbox_holder(0).mailbox, shared_image->mailbox());
  EXPECT_EQ(frame->GetGpuMemoryBufferForTesting()->GetFormat(),
            expected_gmb_format);
  EXPECT_EQ(frame->GetGpuMemoryBufferForTesting()->GetSize(),
            expected_gmb_size);
}
#endif  // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) &&
        // !BUILDFLAG(IS_OZONE)
}  // namespace media
