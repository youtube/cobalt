// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "base/test/null_task_runner.h"
#include "components/viz/test/test_gles2_interface.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_layer_bridge.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using testing::_;
using testing::Test;

namespace blink {

namespace {

template <class GLES2InterfaceType>
class SharedGpuContextTestBase : public Test {
 public:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::NullTaskRunner>();
    handle_ =
        std::make_unique<base::SingleThreadTaskRunner::CurrentDefaultHandle>(
            task_runner_);
    auto factory = [](GLES2InterfaceType* gl, bool* gpu_compositing_disabled)
        -> std::unique_ptr<WebGraphicsContext3DProvider> {
      *gpu_compositing_disabled = false;
      gl->SetIsContextLost(false);
      auto fake_context =
          std::make_unique<FakeWebGraphicsContext3DProvider>(gl);
      gpu::Capabilities capabilities;
      capabilities.max_texture_size = 20;
      fake_context->SetCapabilities(capabilities);
      return fake_context;
    };
    SharedGpuContext::SetContextProviderFactoryForTesting(
        WTF::BindRepeating(factory, WTF::Unretained(&gl_)));
  }

  void TearDown() override {
    handle_.reset();
    task_runner_.reset();
    SharedGpuContext::ResetForTesting();
  }

  scoped_refptr<base::NullTaskRunner> task_runner_;
  std::unique_ptr<base::SingleThreadTaskRunner::CurrentDefaultHandle> handle_;
  GLES2InterfaceType gl_;
};

class TestGLES2Interface : public FakeGLES2Interface {
 public:
  GLuint CreateAndTexStorage2DSharedImageCHROMIUM(const GLbyte*) override {
    return ++texture_id;
  }
  GLuint texture_id = 0u;
};

class SharedGpuContextTest
    : public SharedGpuContextTestBase<TestGLES2Interface> {};

class MailboxMockGLES2Interface : public TestGLES2Interface {
 public:
  MOCK_METHOD2(ProduceTextureDirectCHROMIUM, void(GLuint, GLbyte*));
  MOCK_METHOD1(GenSyncTokenCHROMIUM, void(GLbyte*));
  MOCK_METHOD1(GenUnverifiedSyncTokenCHROMIUM, void(GLbyte*));
};

// Test fixure that simulate a graphics context creation failure, when using gpu
// compositing.
class BadSharedGpuContextTest : public Test {
 public:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::NullTaskRunner>();
    handle_ =
        std::make_unique<base::SingleThreadTaskRunner::CurrentDefaultHandle>(
            task_runner_);
    auto factory = [](bool* gpu_compositing_disabled)
        -> std::unique_ptr<WebGraphicsContext3DProvider> {
      *gpu_compositing_disabled = false;
      return nullptr;
    };
    SharedGpuContext::SetContextProviderFactoryForTesting(
        WTF::BindRepeating(factory));
  }

  void TearDown() override {
    handle_.reset();
    task_runner_.reset();
    SharedGpuContext::ResetForTesting();
  }

  scoped_refptr<base::NullTaskRunner> task_runner_;
  std::unique_ptr<base::SingleThreadTaskRunner::CurrentDefaultHandle> handle_;
};

// Test fixure that simulate not using gpu compositing.
class SoftwareCompositingTest : public Test {
 public:
  void SetUp() override {
    auto factory = [](FakeGLES2Interface* gl, bool* gpu_compositing_disabled)
        -> std::unique_ptr<WebGraphicsContext3DProvider> {
      *gpu_compositing_disabled = true;
      // Return a context anyway, to ensure that's not what the class checks
      // to determine compositing mode.
      gl->SetIsContextLost(false);
      return std::make_unique<FakeWebGraphicsContext3DProvider>(gl);
    };
    SharedGpuContext::SetContextProviderFactoryForTesting(
        WTF::BindRepeating(factory, WTF::Unretained(&gl_)));
  }

  void TearDown() override { SharedGpuContext::ResetForTesting(); }

  FakeGLES2Interface gl_;
};

class SharedGpuContextTestViz : public Test {
 public:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<base::NullTaskRunner>();
    handle_ =
        std::make_unique<base::SingleThreadTaskRunner::CurrentDefaultHandle>(
            task_runner_);
    test_context_provider_ = viz::TestContextProvider::Create();
    InitializeSharedGpuContext(test_context_provider_.get(),
                               /*cache = */ nullptr,
                               SetIsContextLost::kSetToFalse);
  }

  void TearDown() override {
    handle_.reset();
    task_runner_.reset();
    SharedGpuContext::ResetForTesting();
  }
  scoped_refptr<base::NullTaskRunner> task_runner_;
  std::unique_ptr<base::SingleThreadTaskRunner::CurrentDefaultHandle> handle_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
};

TEST_F(SharedGpuContextTest, contextLossAutoRecovery) {
  EXPECT_NE(SharedGpuContext::ContextProviderWrapper(), nullptr);
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context =
      SharedGpuContext::ContextProviderWrapper();
  gl_.SetIsContextLost(true);
  EXPECT_FALSE(SharedGpuContext::IsValidWithoutRestoring());
  EXPECT_TRUE(!!context);

  // Context recreation results in old provider being discarded.
  EXPECT_TRUE(!!SharedGpuContext::ContextProviderWrapper());
  EXPECT_FALSE(!!context);
}

TEST_F(SharedGpuContextTest, Canvas2DLayerBridgeAutoRecovery) {
  // Verifies that after a context loss, attempting to allocate a
  // Canvas2DLayerBridge will restore the context and succeed.
  gl_.SetIsContextLost(true);
  EXPECT_FALSE(SharedGpuContext::IsValidWithoutRestoring());
  gfx::Size size(10, 10);
  std::unique_ptr<Canvas2DLayerBridge> bridge =
      std::make_unique<Canvas2DLayerBridge>(size, RasterMode::kGPU, kNonOpaque);
  EXPECT_TRUE(bridge->IsAccelerated());
  EXPECT_TRUE(SharedGpuContext::IsValidWithoutRestoring());
}

TEST_F(SharedGpuContextTest, IsValidWithoutRestoring) {
  EXPECT_NE(SharedGpuContext::ContextProviderWrapper(), nullptr);
  EXPECT_TRUE(SharedGpuContext::IsValidWithoutRestoring());
}

TEST_F(BadSharedGpuContextTest, IsValidWithoutRestoring) {
  EXPECT_FALSE(SharedGpuContext::IsValidWithoutRestoring());
}

TEST_F(BadSharedGpuContextTest, AllowSoftwareToAcceleratedCanvasUpgrade) {
  EXPECT_FALSE(SharedGpuContext::AllowSoftwareToAcceleratedCanvasUpgrade());
}

TEST_F(BadSharedGpuContextTest, AccelerateImageBufferSurfaceCreationFails) {
  // With a bad shared context, AccelerateImageBufferSurface should fail and
  // return a nullptr provider
  std::unique_ptr<CanvasResourceProvider> resource_provider =
      CanvasResourceProvider::CreateSharedImageProvider(
          SkImageInfo::MakeN32Premul(10, 10),
          cc::PaintFlags::FilterQuality::kLow,
          CanvasResourceProvider::ShouldInitialize::kNo,
          SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
          true /*is_origin_top_left*/, 0u /*shared_image_usage_flags*/);
  EXPECT_FALSE(resource_provider);
}

TEST_F(SharedGpuContextTest, CompositingMode) {
  EXPECT_TRUE(SharedGpuContext::IsGpuCompositingEnabled());
}

TEST_F(BadSharedGpuContextTest, CompositingMode) {
  EXPECT_TRUE(SharedGpuContext::IsGpuCompositingEnabled());
}

TEST_F(SoftwareCompositingTest, CompositingMode) {
  EXPECT_FALSE(SharedGpuContext::IsGpuCompositingEnabled());
}

TEST_F(SharedGpuContextTestViz, AccelerateImageBufferSurfaceAutoRecovery) {
  // Verifies that after a context loss, attempting to allocate an
  // AcceleratedImageBufferSurface will restore the context and succeed
  test_context_provider_->TestContextGL()->set_context_lost(true);
  EXPECT_FALSE(SharedGpuContext::IsValidWithoutRestoring());
  std::unique_ptr<CanvasResourceProvider> resource_provider =
      CanvasResourceProvider::CreateSharedImageProvider(
          SkImageInfo::MakeN32Premul(10, 10),
          cc::PaintFlags::FilterQuality::kLow,
          CanvasResourceProvider::ShouldInitialize::kNo,
          SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
          true /*is_origin_top_left*/, 0u /*shared_image_usage_flags*/);
  EXPECT_TRUE(resource_provider && resource_provider->IsValid());
  EXPECT_TRUE(resource_provider->IsAccelerated());
  EXPECT_TRUE(SharedGpuContext::IsValidWithoutRestoring());
}

}  // unnamed namespace

}  // namespace blink
