// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/fuchsia/fuchsia_video_decoder.h"

#include <fuchsia/sysmem/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/process/process_handle.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/test/test_context_support.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/config/gpu_feature_info.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

namespace {

class TestBufferCollection {
 public:
  explicit TestBufferCollection(zx::channel collection_token) {
    sysmem_allocator_ = base::ComponentContextForProcess()
                            ->svc()
                            ->Connect<fuchsia::sysmem::Allocator>();
    sysmem_allocator_.set_error_handler([](zx_status_t status) {
      ZX_LOG(FATAL, status)
          << "The fuchsia.sysmem.Allocator channel was terminated.";
    });
    sysmem_allocator_->SetDebugClientInfo("CrTestBufferCollection",
                                          base::GetCurrentProcId());

    sysmem_allocator_->BindSharedCollection(
        fidl::InterfaceHandle<fuchsia::sysmem::BufferCollectionToken>(
            std::move(collection_token)),
        buffers_collection_.NewRequest());

    fuchsia::sysmem::BufferCollectionConstraints buffer_constraints;
    buffer_constraints.usage.cpu = fuchsia::sysmem::cpuUsageRead;
    zx_status_t status = buffers_collection_->SetConstraints(
        /*has_constraints=*/true, std::move(buffer_constraints));
    ZX_CHECK(status == ZX_OK, status) << "BufferCollection::SetConstraints()";
  }

  TestBufferCollection(const TestBufferCollection&) = delete;
  TestBufferCollection& operator=(const TestBufferCollection&) = delete;

  ~TestBufferCollection() { buffers_collection_->Close(); }

  size_t GetNumBuffers() {
    if (!buffer_collection_info_) {
      zx_status_t wait_status;
      fuchsia::sysmem::BufferCollectionInfo_2 info;
      zx_status_t status =
          buffers_collection_->WaitForBuffersAllocated(&wait_status, &info);
      ZX_CHECK(status == ZX_OK, status)
          << "BufferCollection::WaitForBuffersAllocated()";
      ZX_CHECK(wait_status == ZX_OK, wait_status)
          << "BufferCollection::WaitForBuffersAllocated()";
      buffer_collection_info_ = std::move(info);
    }
    return buffer_collection_info_->buffer_count;
  }

 private:
  fuchsia::sysmem::AllocatorPtr sysmem_allocator_;
  fuchsia::sysmem::BufferCollectionSyncPtr buffers_collection_;

  absl::optional<fuchsia::sysmem::BufferCollectionInfo_2>
      buffer_collection_info_;
};

class TestSharedImageInterface : public gpu::SharedImageInterface {
 public:
  TestSharedImageInterface() = default;
  ~TestSharedImageInterface() override = default;

  gpu::Mailbox CreateSharedImage(viz::ResourceFormat format,
                                 const gfx::Size& size,
                                 const gfx::ColorSpace& color_space,
                                 GrSurfaceOrigin surface_origin,
                                 SkAlphaType alpha_type,
                                 uint32_t usage,
                                 gpu::SurfaceHandle surface_handle) override {
    ADD_FAILURE();
    return gpu::Mailbox();
  }

  gpu::Mailbox CreateSharedImage(
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      base::span<const uint8_t> pixel_data) override {
    ADD_FAILURE();
    return gpu::Mailbox();
  }

  gpu::Mailbox CreateSharedImage(
      gfx::GpuMemoryBuffer* gpu_memory_buffer,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      gfx::BufferPlane plane,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage) override {
    gfx::GpuMemoryBufferHandle handle = gpu_memory_buffer->CloneHandle();
    CHECK_EQ(handle.type, gfx::GpuMemoryBufferType::NATIVE_PIXMAP);

    auto collection_it = sysmem_buffer_collections_.find(
        handle.native_pixmap_handle.buffer_collection_id);
    CHECK(collection_it != sysmem_buffer_collections_.end());
    CHECK_LT(handle.native_pixmap_handle.buffer_index,
             collection_it->second->GetNumBuffers());

    auto result = gpu::Mailbox::Generate();
    mailboxes_.insert(result);
    return result;
  }

  void UpdateSharedImage(const gpu::SyncToken& sync_token,
                         const gpu::Mailbox& mailbox) override {
    ADD_FAILURE();
  }
  void UpdateSharedImage(const gpu::SyncToken& sync_token,
                         std::unique_ptr<gfx::GpuFence> acquire_fence,
                         const gpu::Mailbox& mailbox) override {
    ADD_FAILURE();
  }

  void DestroySharedImage(const gpu::SyncToken& sync_token,
                          const gpu::Mailbox& mailbox) override {
    CHECK_EQ(mailboxes_.erase(mailbox), 1U);
  }

  SwapChainMailboxes CreateSwapChain(viz::ResourceFormat format,
                                     const gfx::Size& size,
                                     const gfx::ColorSpace& color_space,
                                     GrSurfaceOrigin surface_origin,
                                     SkAlphaType alpha_type,
                                     uint32_t usage) override {
    ADD_FAILURE();
    return SwapChainMailboxes();
  }
  void PresentSwapChain(const gpu::SyncToken& sync_token,
                        const gpu::Mailbox& mailbox) override {
    ADD_FAILURE();
  }

  void RegisterSysmemBufferCollection(gfx::SysmemBufferCollectionId id,
                                      zx::channel token,
                                      gfx::BufferFormat format,
                                      gfx::BufferUsage usage,
                                      bool register_with_image_pipe) override {
    EXPECT_EQ(format, gfx::BufferFormat::YUV_420_BIPLANAR);
    EXPECT_EQ(usage, gfx::BufferUsage::GPU_READ);
    std::unique_ptr<TestBufferCollection>& collection =
        sysmem_buffer_collections_[id];
    EXPECT_FALSE(collection);
    collection = std::make_unique<TestBufferCollection>(std::move(token));
  }
  void ReleaseSysmemBufferCollection(
      gfx::SysmemBufferCollectionId id) override {
    EXPECT_EQ(sysmem_buffer_collections_.erase(id), 1U);
  }

  gpu::SyncToken GenVerifiedSyncToken() override {
    ADD_FAILURE();
    return gpu::SyncToken();
  }
  gpu::SyncToken GenUnverifiedSyncToken() override {
    return gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                          gpu::CommandBufferId(33), 1);
  }

  void WaitSyncToken(const gpu::SyncToken& sync_token) override {
    ADD_FAILURE();
  }

  void Flush() override { ADD_FAILURE(); }

  scoped_refptr<gfx::NativePixmap> GetNativePixmap(
      const gpu::Mailbox& mailbox) override {
    return nullptr;
  }

 private:
  base::flat_map<gfx::SysmemBufferCollectionId,
                 std::unique_ptr<TestBufferCollection>>
      sysmem_buffer_collections_;

  base::flat_set<gpu::Mailbox> mailboxes_;
};

class TestRasterContextProvider
    : public base::RefCountedThreadSafe<TestRasterContextProvider>,
      public viz::RasterContextProvider {
 public:
  TestRasterContextProvider() {}

  TestRasterContextProvider(TestRasterContextProvider&) = delete;
  TestRasterContextProvider& operator=(TestRasterContextProvider&) = delete;

  void SetOnDestroyedClosure(base::OnceClosure on_destroyed) {
    on_destroyed_ = std::move(on_destroyed);
  }

  // viz::RasterContextProvider implementation;
  void AddRef() const override {
    base::RefCountedThreadSafe<TestRasterContextProvider>::AddRef();
  }
  void Release() const override {
    base::RefCountedThreadSafe<TestRasterContextProvider>::Release();
  }
  gpu::ContextResult BindToCurrentThread() override {
    ADD_FAILURE();
    return gpu::ContextResult::kFatalFailure;
  }
  void AddObserver(viz::ContextLostObserver* obs) override { ADD_FAILURE(); }
  void RemoveObserver(viz::ContextLostObserver* obs) override { ADD_FAILURE(); }
  base::Lock* GetLock() override {
    ADD_FAILURE();
    return nullptr;
  }
  viz::ContextCacheController* CacheController() override {
    ADD_FAILURE();
    return nullptr;
  }
  gpu::ContextSupport* ContextSupport() override {
    return &gpu_context_support_;
  }
  class GrDirectContext* GrContext() override {
    ADD_FAILURE();
    return nullptr;
  }
  gpu::SharedImageInterface* SharedImageInterface() override {
    return &shared_image_interface_;
  }
  const gpu::Capabilities& ContextCapabilities() const override {
    ADD_FAILURE();
    static gpu::Capabilities dummy_caps;
    return dummy_caps;
  }
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override {
    ADD_FAILURE();
    static gpu::GpuFeatureInfo dummy_feature_info;
    return dummy_feature_info;
  }
  gpu::gles2::GLES2Interface* ContextGL() override {
    ADD_FAILURE();
    return nullptr;
  }
  gpu::raster::RasterInterface* RasterInterface() override {
    ADD_FAILURE();
    return nullptr;
  }

 private:
  friend class base::RefCountedThreadSafe<TestRasterContextProvider>;

  ~TestRasterContextProvider() override {
    if (on_destroyed_)
      std::move(on_destroyed_).Run();
  }

  TestSharedImageInterface shared_image_interface_;
  viz::TestContextSupport gpu_context_support_;

  base::OnceClosure on_destroyed_;
};

}  // namespace

class FuchsiaVideoDecoderTest : public testing::Test {
 public:
  FuchsiaVideoDecoderTest()
      : raster_context_provider_(
            base::MakeRefCounted<TestRasterContextProvider>()),
        decoder_(
            FuchsiaVideoDecoder::CreateForTests(raster_context_provider_.get(),
                                                /*enable_sw_decoding=*/true)) {}

  FuchsiaVideoDecoderTest(const FuchsiaVideoDecoderTest&) = delete;
  FuchsiaVideoDecoderTest& operator=(const FuchsiaVideoDecoderTest&) = delete;

  ~FuchsiaVideoDecoderTest() override = default;

  bool InitializeDecoder(VideoDecoderConfig config) WARN_UNUSED_RESULT {
    base::RunLoop run_loop;
    bool init_cb_result = false;
    decoder_->Initialize(
        config, true, /*cdm_context=*/nullptr,
        base::BindRepeating(
            [](bool* init_cb_result, base::RunLoop* run_loop, Status status) {
              *init_cb_result = status.is_ok();
              run_loop->Quit();
            },
            &init_cb_result, &run_loop),
        base::BindRepeating(&FuchsiaVideoDecoderTest::OnVideoFrame,
                            weak_factory_.GetWeakPtr()),
        base::DoNothing());

    run_loop.Run();
    return init_cb_result;
  }

  void ResetDecoder() {
    base::RunLoop run_loop;
    decoder_->Reset(base::BindRepeating(
        [](base::RunLoop* run_loop) { run_loop->Quit(); }, &run_loop));
    run_loop.Run();
  }

  void OnVideoFrame(scoped_refptr<VideoFrame> frame) {
    num_output_frames_++;
    CHECK(frame->HasTextures());
    output_frames_.push_back(std::move(frame));
    while (output_frames_.size() > frames_to_keep_) {
      output_frames_.pop_front();
    }
    if (run_loop_)
      run_loop_->Quit();
  }

  void DecodeBuffer(scoped_refptr<DecoderBuffer> buffer) {
    decoder_->Decode(
        buffer,
        base::BindRepeating(&FuchsiaVideoDecoderTest::OnFrameDecoded,
                            weak_factory_.GetWeakPtr(), num_input_buffers_));
    num_input_buffers_ += 1;
  }

  void ReadAndDecodeFrame(const std::string& name) {
    DecodeBuffer(ReadTestDataFile(name));
  }

  void OnFrameDecoded(size_t frame_pos, Status status) {
    EXPECT_EQ(frame_pos, num_decoded_buffers_);
    num_decoded_buffers_ += 1;
    last_decode_status_ = std::move(status);
    if (run_loop_)
      run_loop_->Quit();
  }

  // Waits until all pending decode requests are finished.
  void WaitDecodeDone() {
    size_t target_pos = num_input_buffers_;
    while (num_decoded_buffers_ < target_pos) {
      base::RunLoop run_loop;
      run_loop_ = &run_loop;
      run_loop.Run();
      run_loop_ = nullptr;
      ASSERT_TRUE(last_decode_status_.is_ok());
    }
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  scoped_refptr<TestRasterContextProvider> raster_context_provider_;

  std::unique_ptr<VideoDecoder> decoder_;

  size_t num_input_buffers_ = 0;

  // Number of frames for which DecodeCB has been called. That doesn't mean
  // we've received corresponding output frames.
  size_t num_decoded_buffers_ = 0;

  std::list<scoped_refptr<VideoFrame>> output_frames_;
  size_t num_output_frames_ = 0;

  Status last_decode_status_;
  base::RunLoop* run_loop_ = nullptr;

  // Number of frames that OnVideoFrame() should keep in |output_frames_|.
  size_t frames_to_keep_ = 2;

  base::WeakPtrFactory<FuchsiaVideoDecoderTest> weak_factory_{this};
};

scoped_refptr<DecoderBuffer> GetH264Frame(size_t frame_num) {
  static scoped_refptr<DecoderBuffer> frames[] = {
      ReadTestDataFile("h264-320x180-frame-0"),
      ReadTestDataFile("h264-320x180-frame-1"),
      ReadTestDataFile("h264-320x180-frame-2"),
      ReadTestDataFile("h264-320x180-frame-3")};
  CHECK_LT(frame_num, base::size(frames));
  return frames[frame_num];
}

TEST_F(FuchsiaVideoDecoderTest, CreateAndDestroy) {}

TEST_F(FuchsiaVideoDecoderTest, CreateInitDestroy) {
  EXPECT_TRUE(InitializeDecoder(TestVideoConfig::NormalH264()));
}

TEST_F(FuchsiaVideoDecoderTest, DISABLED_VP9) {
  ASSERT_TRUE(InitializeDecoder(TestVideoConfig::Normal(VideoCodec::kVP9)));

  DecodeBuffer(ReadTestDataFile("vp9-I-frame-320x240"));
  DecodeBuffer(DecoderBuffer::CreateEOSBuffer());
  ASSERT_NO_FATAL_FAILURE(WaitDecodeDone());

  EXPECT_EQ(num_output_frames_, 1U);
}

TEST_F(FuchsiaVideoDecoderTest, H264) {
  ASSERT_TRUE(InitializeDecoder(TestVideoConfig::NormalH264()));

  DecodeBuffer(GetH264Frame(0));
  DecodeBuffer(GetH264Frame(1));
  ASSERT_NO_FATAL_FAILURE(WaitDecodeDone());

  DecodeBuffer(GetH264Frame(2));
  DecodeBuffer(GetH264Frame(3));
  DecodeBuffer(DecoderBuffer::CreateEOSBuffer());
  ASSERT_NO_FATAL_FAILURE(WaitDecodeDone());

  EXPECT_EQ(num_output_frames_, 4U);
}

// Verify that the decoder can be re-initialized while there pending decode
// requests.
TEST_F(FuchsiaVideoDecoderTest, ReinitializeH264) {
  ASSERT_TRUE(InitializeDecoder(TestVideoConfig::NormalH264()));
  DecodeBuffer(GetH264Frame(0));
  DecodeBuffer(GetH264Frame(1));
  ASSERT_NO_FATAL_FAILURE(WaitDecodeDone());

  num_output_frames_ = 0;

  // Re-initialize decoder and send the same data again.
  ASSERT_TRUE(InitializeDecoder(TestVideoConfig::NormalH264()));
  DecodeBuffer(GetH264Frame(0));
  DecodeBuffer(GetH264Frame(1));
  DecodeBuffer(GetH264Frame(2));
  DecodeBuffer(GetH264Frame(3));
  DecodeBuffer(DecoderBuffer::CreateEOSBuffer());
  ASSERT_NO_FATAL_FAILURE(WaitDecodeDone());

  EXPECT_EQ(num_output_frames_, 4U);
}

// Verify that the decoder can be re-initialized after Reset().
TEST_F(FuchsiaVideoDecoderTest, ResetAndReinitializeH264) {
  ASSERT_TRUE(InitializeDecoder(TestVideoConfig::NormalH264()));
  DecodeBuffer(GetH264Frame(0));
  DecodeBuffer(GetH264Frame(1));
  ASSERT_NO_FATAL_FAILURE(WaitDecodeDone());

  ResetDecoder();

  num_output_frames_ = 0;

  // Re-initialize decoder and send the same data again.
  ASSERT_TRUE(InitializeDecoder(TestVideoConfig::NormalH264()));
  DecodeBuffer(GetH264Frame(0));
  DecodeBuffer(GetH264Frame(1));
  DecodeBuffer(GetH264Frame(2));
  DecodeBuffer(GetH264Frame(3));
  DecodeBuffer(DecoderBuffer::CreateEOSBuffer());
  ASSERT_NO_FATAL_FAILURE(WaitDecodeDone());

  EXPECT_EQ(num_output_frames_, 4U);
}

// Verifies that the decoder keeps reference to the RasterContextProvider.
TEST_F(FuchsiaVideoDecoderTest, RasterContextLifetime) {
  bool context_destroyed = false;
  raster_context_provider_->SetOnDestroyedClosure(base::BindLambdaForTesting(
      [&context_destroyed]() { context_destroyed = true; }));
  ASSERT_TRUE(InitializeDecoder(TestVideoConfig::NormalH264()));
  ASSERT_FALSE(context_destroyed);

  // Decoder should keep reference to RasterContextProvider.
  raster_context_provider_.reset();
  ASSERT_FALSE(context_destroyed);

  // Feed some frames to decoder to get decoded video frames.
  for (int i = 0; i < 4; ++i) {
    DecodeBuffer(GetH264Frame(i));
  }
  ASSERT_NO_FATAL_FAILURE(WaitDecodeDone());

  // Destroy the decoder. RasterContextProvider will not be destroyed since
  // it's still referenced by frames in |output_frames_|.
  decoder_.reset();
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(context_destroyed);

  // RasterContextProvider reference should be dropped once all frames are
  // dropped.
  output_frames_.clear();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(context_destroyed);
}

}  // namespace media
