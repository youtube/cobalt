// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/frame_info_helper.h"

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/service/mock_texture_owner.h"
#include "gpu/command_buffer/service/ref_counted_lock_for_test.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::SetArgPointee;

namespace media {
namespace {
constexpr gfx::Size kTestVisibleSize(100, 100);
constexpr gfx::Size kTestVisibleSize2(110, 110);
constexpr gfx::Size kTestCodedSize(128, 128);

std::unique_ptr<FrameInfoHelper> CreateHelper() {
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  auto get_stub_cb =
      base::BindRepeating([]() -> gpu::CommandBufferStub* { return nullptr; });
  return FrameInfoHelper::Create(
      std::move(task_runner), std::move(get_stub_cb),
      features::NeedThreadSafeAndroidMedia()
          ? base::MakeRefCounted<gpu::RefCountedLockForTest>()
          : nullptr);
}
}  // namespace

class FrameInfoHelperTest : public testing::Test {
 public:
  FrameInfoHelperTest() : helper_(CreateHelper()) {}

 protected:
  void GetFrameInfo(
      std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer) {
    const auto* buffer_renderer_raw = buffer_renderer.get();
    bool called = false;
    auto callback = base::BindLambdaForTesting(
        [&](std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
            FrameInfoHelper::FrameInfo info) {
          ASSERT_EQ(buffer_renderer_raw, buffer_renderer.get());
          called = true;
          last_frame_info_ = info;
        });
    helper_->GetFrameInfo(std::move(buffer_renderer), callback);
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(called);
  }

  std::unique_ptr<CodecOutputBufferRenderer> CreateBufferRenderer(
      gfx::Size size,
      scoped_refptr<gpu::TextureOwner> texture_owner) {
    auto codec_buffer_wait_coordinator =
        texture_owner
            ? base::MakeRefCounted<CodecBufferWaitCoordinator>(
                  texture_owner,
                  features::NeedThreadSafeAndroidMedia()
                      ? base::MakeRefCounted<gpu::RefCountedLockForTest>()
                      : nullptr)
            : nullptr;
    auto buffer = CodecOutputBuffer::CreateForTesting(
        0, size, gfx::ColorSpace::CreateSRGB());
    auto buffer_renderer = std::make_unique<CodecOutputBufferRenderer>(
        std::move(buffer), codec_buffer_wait_coordinator,
        features::NeedThreadSafeAndroidMedia()
            ? base::MakeRefCounted<gpu::RefCountedLockForTest>()
            : nullptr);

    // We don't have codec, so releasing test buffer is not possible. Mark it as
    // rendered for test purpose.
    buffer_renderer->set_phase_for_testing(
        CodecOutputBufferRenderer::Phase::kInFrontBuffer);
    return buffer_renderer;
  }

  void FailNextRender(CodecOutputBufferRenderer* buffer_renderer) {
    buffer_renderer->set_phase_for_testing(
        CodecOutputBufferRenderer::Phase::kInvalidated);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<FrameInfoHelper> helper_;
  FrameInfoHelper::FrameInfo last_frame_info_;
};

TEST_F(FrameInfoHelperTest, NoBufferRenderer) {
  // If there is no buffer renderer we shouldn't crash.
  GetFrameInfo(nullptr);
}

TEST_F(FrameInfoHelperTest, TextureOwner) {
  auto texture_owner = base::MakeRefCounted<NiceMock<gpu::MockTextureOwner>>(
      0, nullptr, nullptr, true);

  // Return CodedSize when GetCodedSizeAndVisibleRect is called.
  ON_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _))
      .WillByDefault(DoAll(SetArgPointee<1>(kTestCodedSize), Return(true)));

  // Fail rendering buffer.
  auto buffer1 = CreateBufferRenderer(kTestVisibleSize, texture_owner);
  FailNextRender(buffer1.get());
  // GetFrameInfo should fallback to visible size in this case, but mark request
  // as failed.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(0);
  GetFrameInfo(std::move(buffer1));
  EXPECT_EQ(last_frame_info_.coded_size, kTestVisibleSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // This time rendering should succeed. We expect GetCodedSizeAndVisibleRect to
  // be called and result should be kTestCodedSize instead of kTestVisibleSize.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(1);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // Verify that we don't render frame on subsequent calls with the same visible
  // size. GetCodedSizeAndVisibleRect should not be called.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(0);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // Verify that we render if the visible size changed.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(1);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize2, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
}

TEST_F(FrameInfoHelperTest, Overlay) {
  // In overlay case we always use visible size.
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, nullptr));
  EXPECT_EQ(last_frame_info_.coded_size, kTestVisibleSize);
}

TEST_F(FrameInfoHelperTest, SwitchBetweenOverlayAndTextureOwner) {
  auto texture_owner = base::MakeRefCounted<NiceMock<gpu::MockTextureOwner>>(
      0, nullptr, nullptr, true);

  // Return CodedSize when GetCodedSizeAndVisibleRect is called.
  ON_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _))
      .WillByDefault(DoAll(SetArgPointee<1>(kTestCodedSize), Return(true)));

  // In overlay case we always use visible size.
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, nullptr));
  EXPECT_EQ(last_frame_info_.coded_size, kTestVisibleSize);

  // Verify that when we switch to TextureOwner we request new size.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(1);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // Switch back to overlay and verify that we falled back to visible size and
  // didn't try to render image.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(0);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, nullptr));
  EXPECT_EQ(last_frame_info_.coded_size, kTestVisibleSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // Verify that when we switch to TextureOwner we use cached size.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(0);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // Switch back to overlay again.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(0);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, nullptr));
  EXPECT_EQ(last_frame_info_.coded_size, kTestVisibleSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // Verify that when we switch to TextureOwner with resize we render another
  // frame.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(1);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize2, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());
}

TEST_F(FrameInfoHelperTest, OrderingTest) {
  auto texture_owner = base::MakeRefCounted<NiceMock<gpu::MockTextureOwner>>(
      0, nullptr, nullptr, true);

  // Return CodedSize when GetCodedSizeAndVisibleRect is called.
  ON_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _))
      .WillByDefault(DoAll(SetArgPointee<1>(kTestCodedSize), Return(true)));

  auto buffer_renderer = CreateBufferRenderer(kTestVisibleSize, texture_owner);

  // Create first callback and request frame.
  const auto* buffer_renderer_raw = buffer_renderer.get();
  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
          FrameInfoHelper::FrameInfo info) {
        ASSERT_EQ(buffer_renderer_raw, buffer_renderer.get());
        called = true;
        last_frame_info_ = info;
      });
  helper_->GetFrameInfo(std::move(buffer_renderer), callback);

  // Create run after callback.
  bool run_after_called = false;
  auto run_after_callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
          FrameInfoHelper::FrameInfo info) {
        ASSERT_EQ(buffer_renderer.get(), nullptr);
        // Expect first callback was called before this.
        EXPECT_TRUE(called);
        run_after_called = true;
      });
  helper_->GetFrameInfo(nullptr, run_after_callback);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
  ASSERT_TRUE(run_after_called);
}

TEST_F(FrameInfoHelperTest, FailedGetCodedSize) {
  auto texture_owner = base::MakeRefCounted<NiceMock<gpu::MockTextureOwner>>(
      0, nullptr, nullptr, true);

  // Return CodedSize when GetCodedSizeAndVisibleRect is called.
  ON_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _))
      .WillByDefault(DoAll(SetArgPointee<1>(kTestCodedSize), Return(true)));

  // Fail next GetCodedSizeAndVisibleRect. GetFrameInfo should fallback to
  // visible size in this case, but mark request as failed.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _))
      .WillOnce(DoAll(SetArgPointee<1>(gfx::Size()), Return(false)));
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestVisibleSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // This time GetCodedSizeAndVisibleRect will succeed and be called and result
  // should be kTestCodedSize instead of kTestVisibleSize.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(1);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // Verify that we don't render frame on subsequent calls with the same visible
  // size. GetCodedSizeAndVisibleRect should not be called.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(0);
  GetFrameInfo(CreateBufferRenderer(kTestVisibleSize, texture_owner));
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  Mock::VerifyAndClearExpectations(texture_owner.get());
}

TEST_F(FrameInfoHelperTest, TextureOwnerBufferNotAvailable) {
  auto texture_owner = base::MakeRefCounted<NiceMock<gpu::MockTextureOwner>>(
      0, nullptr, nullptr, true);

  // Return CodedSize when GetCodedSizeAndVisibleRect is called.
  ON_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _))
      .WillByDefault(DoAll(SetArgPointee<1>(kTestCodedSize), Return(true)));

  // Save buffer available callback, we will run it manually.
  base::OnceClosure buffer_available_cb;
  EXPECT_CALL(*texture_owner, RunWhenBufferIsAvailable(_))
      .WillOnce(Invoke([&buffer_available_cb](base::OnceClosure cb) {
        buffer_available_cb = std::move(cb);
      }));

  // Verify that no GetCodedSizeAndVisibleRect will be called until buffer is
  // available.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(0);

  // Note that we can't use helper above because the callback won't run until a
  // buffer is available.
  auto buffer_renderer = CreateBufferRenderer(kTestVisibleSize, texture_owner);
  const auto* buffer_renderer_raw = buffer_renderer.get();
  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<CodecOutputBufferRenderer> buffer_renderer,
          FrameInfoHelper::FrameInfo info) {
        ASSERT_EQ(buffer_renderer_raw, buffer_renderer.get());
        called = true;
        last_frame_info_ = info;
      });
  helper_->GetFrameInfo(std::move(buffer_renderer), callback);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(buffer_available_cb);
  Mock::VerifyAndClearExpectations(texture_owner.get());

  // When buffer is available we expect GetCodedSizeAndVisibleRect to be called
  // and result should be kTestCodedSize.
  EXPECT_CALL(*texture_owner, GetCodedSizeAndVisibleRect(_, _, _)).Times(1);
  std::move(buffer_available_cb).Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(last_frame_info_.coded_size, kTestCodedSize);
  ASSERT_TRUE(called);
  Mock::VerifyAndClearExpectations(texture_owner.get());
}

}  // namespace media
