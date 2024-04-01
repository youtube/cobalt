// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <utility>

#include "base/callback_helpers.h"
#include "base/test/task_environment.h"
#include "media/gpu/windows/d3d11_copying_texture_wrapper.h"
#include "media/gpu/windows/d3d11_texture_wrapper.h"
#include "media/gpu/windows/d3d11_video_processor_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/hdr_metadata_helper_win.h"

using ::testing::_;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::Return;
using ::testing::Values;

namespace media {

class MockVideoProcessorProxy : public VideoProcessorProxy {
 public:
  MockVideoProcessorProxy() : VideoProcessorProxy(nullptr, nullptr) {}

  Status Init(uint32_t width, uint32_t height) override {
    return MockInit(width, height);
  }

  HRESULT CreateVideoProcessorOutputView(
      ID3D11Texture2D* output_texture,
      D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC* output_view_descriptor,
      ID3D11VideoProcessorOutputView** output_view) override {
    return MockCreateVideoProcessorOutputView();
  }

  HRESULT CreateVideoProcessorInputView(
      ID3D11Texture2D* input_texture,
      D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC* input_view_descriptor,
      ID3D11VideoProcessorInputView** input_view) override {
    return MockCreateVideoProcessorInputView();
  }

  void SetStreamColorSpace(const gfx::ColorSpace& color_space) override {
    last_stream_color_space_ = color_space;
  }

  void SetOutputColorSpace(const gfx::ColorSpace& color_space) override {
    last_output_color_space_ = color_space;
  }

  void SetStreamHDRMetadata(
      const DXGI_HDR_METADATA_HDR10& stream_metadata) override {
    last_stream_metadata_ = stream_metadata;
  }

  void SetDisplayHDRMetadata(
      const DXGI_HDR_METADATA_HDR10& display_metadata) override {
    last_display_metadata_ = display_metadata;
  }

  HRESULT VideoProcessorBlt(ID3D11VideoProcessorOutputView* output_view,
                            UINT output_frameno,
                            UINT stream_count,
                            D3D11_VIDEO_PROCESSOR_STREAM* streams) override {
    return MockVideoProcessorBlt();
  }

  MOCK_METHOD2(MockInit, Status(uint32_t, uint32_t));
  MOCK_METHOD0(MockCreateVideoProcessorOutputView, HRESULT());
  MOCK_METHOD0(MockCreateVideoProcessorInputView, HRESULT());
  MOCK_METHOD0(MockVideoProcessorBlt, HRESULT());

  // Most recent arguments to SetStream/OutputColorSpace()/etc.
  absl::optional<gfx::ColorSpace> last_stream_color_space_;
  absl::optional<gfx::ColorSpace> last_output_color_space_;
  absl::optional<DXGI_HDR_METADATA_HDR10> last_stream_metadata_;
  absl::optional<DXGI_HDR_METADATA_HDR10> last_display_metadata_;

 private:
  ~MockVideoProcessorProxy() override = default;
};

class MockTexture2DWrapper : public Texture2DWrapper {
 public:
  MockTexture2DWrapper() {}

  Status ProcessTexture(const gfx::ColorSpace& input_color_space,
                        MailboxHolderArray* mailbox_dest,
                        gfx::ColorSpace* output_color_space) override {
    // Pretend we created an arbitrary color space, so that we're sure that it
    // is returned from the copying wrapper.
    *output_color_space = gfx::ColorSpace::CreateHDR10();
    return MockProcessTexture();
  }

  Status Init(scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
              GetCommandBufferHelperCB get_helper_cb,
              ComD3D11Texture2D in_texture,
              size_t array_slice) override {
    gpu_task_runner_ = std::move(gpu_task_runner);
    return MockInit();
  }

  Status AcquireKeyedMutexIfNeeded() override {
    return MockAcquireKeyedMutexIfNeeded();
  }

  MOCK_METHOD0(MockInit, Status());
  MOCK_METHOD0(MockAcquireKeyedMutexIfNeeded, Status());
  MOCK_METHOD0(MockProcessTexture, Status());
  MOCK_METHOD1(SetStreamHDRMetadata,
               void(const gfx::HDRMetadata& stream_metadata));
  MOCK_METHOD1(SetDisplayHDRMetadata,
               void(const DXGI_HDR_METADATA_HDR10& dxgi_display_metadata));

  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
};

CommandBufferHelperPtr UselessHelper() {
  return nullptr;
}

class D3D11CopyingTexture2DWrapperTest
    : public ::testing::TestWithParam<
          std::tuple<HRESULT, HRESULT, HRESULT, bool, bool, bool, bool, bool>> {
 public:
#define FIELD(TYPE, NAME, INDEX) \
  TYPE Get##NAME() { return std::get<INDEX>(GetParam()); }
  FIELD(HRESULT, CreateVideoProcessorOutputView, 0)
  FIELD(HRESULT, CreateVideoProcessorInputView, 1)
  FIELD(HRESULT, VideoProcessorBlt, 2)
  FIELD(bool, ProcessorProxyInit, 3)
  FIELD(bool, TextureWrapperInit, 4)
  FIELD(bool, ProcessTexture, 5)
  FIELD(bool, PassthroughColorSpace, 6)
  FIELD(bool, AcquireKeyedMutexIfNeeded, 7)
#undef FIELD

  void SetUp() override {
    gpu_task_runner_ = task_environment_.GetMainThreadTaskRunner();
  }

  scoped_refptr<MockVideoProcessorProxy> ExpectProcessorProxy() {
    auto result = base::MakeRefCounted<MockVideoProcessorProxy>();
    ON_CALL(*result.get(), MockInit(_, _))
        .WillByDefault(Return(GetProcessorProxyInit()
                                  ? StatusCode::kOk
                                  : StatusCode::kCodeOnlyForTesting));

    ON_CALL(*result.get(), MockCreateVideoProcessorOutputView())
        .WillByDefault(Return(GetCreateVideoProcessorOutputView()));

    ON_CALL(*result.get(), MockCreateVideoProcessorInputView())
        .WillByDefault(Return(GetCreateVideoProcessorInputView()));

    ON_CALL(*result.get(), MockVideoProcessorBlt())
        .WillByDefault(Return(GetVideoProcessorBlt()));

    return result;
  }

  std::unique_ptr<MockTexture2DWrapper> ExpectTextureWrapper() {
    auto result = std::make_unique<MockTexture2DWrapper>();

    ON_CALL(*result.get(), MockInit())
        .WillByDefault(Return(GetTextureWrapperInit()
                                  ? StatusCode::kOk
                                  : StatusCode::kCodeOnlyForTesting));

    ON_CALL(*result.get(), MockAcquireKeyedMutexIfNeeded())
        .WillByDefault(Return(GetAcquireKeyedMutexIfNeeded()
                                  ? StatusCode::kOk
                                  : StatusCode::kCodeOnlyForTesting));

    ON_CALL(*result.get(), MockProcessTexture())
        .WillByDefault(Return(GetProcessTexture()
                                  ? StatusCode::kOk
                                  : StatusCode::kCodeOnlyForTesting));

    return result;
  }

  GetCommandBufferHelperCB CreateMockHelperCB() {
    return base::BindRepeating(&UselessHelper);
  }

  bool InitSucceeds() {
    return GetProcessorProxyInit() && GetTextureWrapperInit();
  }

  bool ProcessTextureSucceeds() {
    return GetAcquireKeyedMutexIfNeeded() && GetProcessTexture() &&
           SUCCEEDED(GetCreateVideoProcessorOutputView()) &&
           SUCCEEDED(GetCreateVideoProcessorInputView()) &&
           SUCCEEDED(GetVideoProcessorBlt());
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
};

INSTANTIATE_TEST_CASE_P(CopyingTexture2DWrapperTest,
                        D3D11CopyingTexture2DWrapperTest,
                        Combine(Values(S_OK, E_FAIL),
                                Values(S_OK, E_FAIL),
                                Values(S_OK, E_FAIL),
                                Bool(),
                                Bool(),
                                Bool(),
                                Bool(),
                                Bool()));

// For ever potential return value combination for the D3D11VideoProcessor,
// make sure that any failures result in a total failure.
TEST_P(D3D11CopyingTexture2DWrapperTest,
       CopyingTextureWrapperProcessesCorrectly) {
  gfx::Size size;
  auto processor = ExpectProcessorProxy();
  MockVideoProcessorProxy* processor_raw = processor.get();
  // Provide an unlikely color space, to see if it gets to the video processor,
  // if we're not just doing a pass-through of the input.
  absl::optional<gfx::ColorSpace> copy_color_space;
  if (!GetPassthroughColorSpace())
    copy_color_space = gfx::ColorSpace::CreateDisplayP3D65();
  auto texture_wrapper = ExpectTextureWrapper();
  MockTexture2DWrapper* texture_wrapper_raw = texture_wrapper.get();
  auto wrapper = std::make_unique<CopyingTexture2DWrapper>(
      size, std::move(texture_wrapper), processor, nullptr, copy_color_space);

  // TODO: check |gpu_task_runner_|.

  MailboxHolderArray mailboxes;
  gfx::ColorSpace input_color_space = gfx::ColorSpace::CreateSCRGBLinear();
  gfx::ColorSpace output_color_space;
  EXPECT_EQ(wrapper
                ->Init(gpu_task_runner_, CreateMockHelperCB(),
                       /*texture_d3d=*/nullptr, /*array_slice=*/0)
                .is_ok(),
            InitSucceeds());
  task_environment_.RunUntilIdle();
  if (GetProcessorProxyInit())
    EXPECT_EQ(texture_wrapper_raw->gpu_task_runner_, gpu_task_runner_);
  EXPECT_EQ(
      wrapper
          ->ProcessTexture(input_color_space, &mailboxes, &output_color_space)
          .is_ok(),
      ProcessTextureSucceeds());

  if (ProcessTextureSucceeds()) {
    // Regardless of what the input space is, the output should be provided by
    // the mock wrapper.
    EXPECT_EQ(gfx::ColorSpace::CreateHDR10(), output_color_space);

    // Also expect that the input and copy spaces were provided to the video
    // processor as the stream and output color spaces, respectively.  If no
    // copy space was provided, then expect that the output is the input.
    EXPECT_TRUE(processor_raw->last_stream_color_space_);
    EXPECT_EQ(*processor_raw->last_stream_color_space_, input_color_space);
    EXPECT_TRUE(processor_raw->last_output_color_space_);
    EXPECT_EQ(*processor_raw->last_output_color_space_,
              copy_color_space ? *copy_color_space : input_color_space);
  }

  // TODO: verify that these aren't sent multiple times, unless they change.
}

TEST_P(D3D11CopyingTexture2DWrapperTest, HDRMetadataIsSentToVideoProcessor) {
  gfx::HDRMetadata metadata;
  metadata.color_volume_metadata.primary_r =
      gfx::ColorVolumeMetadata::Chromaticity(0.1, 0.2);
  metadata.color_volume_metadata.primary_g =
      gfx::ColorVolumeMetadata::Chromaticity(0.3, 0.4);
  metadata.color_volume_metadata.primary_b =
      gfx::ColorVolumeMetadata::Chromaticity(0.5, 0.6);
  metadata.color_volume_metadata.white_point =
      gfx::ColorVolumeMetadata::Chromaticity(0.7, 0.8);
  metadata.color_volume_metadata.luminance_max = 0.9;
  metadata.color_volume_metadata.luminance_min = 0.05;
  metadata.max_content_light_level = 1000;
  metadata.max_frame_average_light_level = 10000;

  auto processor = ExpectProcessorProxy();
  MockVideoProcessorProxy* processor_raw = processor.get();
  auto wrapper = std::make_unique<CopyingTexture2DWrapper>(
      gfx::Size(100, 200), ExpectTextureWrapper(), std::move(processor),
      nullptr, gfx::ColorSpace::CreateSCRGBLinear());

  const DXGI_HDR_METADATA_HDR10 dxgi_metadata =
      gl::HDRMetadataHelperWin::HDRMetadataToDXGI(metadata);

  wrapper->SetStreamHDRMetadata(metadata);
  EXPECT_TRUE(processor_raw->last_stream_metadata_);
  EXPECT_FALSE(processor_raw->last_display_metadata_);
  EXPECT_EQ(memcmp(&dxgi_metadata, &(*processor_raw->last_stream_metadata_),
                   sizeof(dxgi_metadata)),
            0);
  processor_raw->last_stream_metadata_.reset();

  wrapper->SetDisplayHDRMetadata(dxgi_metadata);
  EXPECT_FALSE(processor_raw->last_stream_metadata_);
  EXPECT_TRUE(processor_raw->last_display_metadata_);
  EXPECT_EQ(memcmp(&dxgi_metadata, &(*processor_raw->last_display_metadata_),
                   sizeof(dxgi_metadata)),
            0);
}

}  // namespace media
