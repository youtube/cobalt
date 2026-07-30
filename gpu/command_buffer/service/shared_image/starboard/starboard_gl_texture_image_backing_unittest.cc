// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "gpu/command_buffer/service/shared_image/starboard/starboard_gl_texture_image_backing.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_test_base.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "starboard/decode_target.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

// Mock Starboard functions
extern "C" {
static int g_release_count = 0;
void SbDecodeTargetRelease(SbDecodeTarget decode_target) {
  g_release_count++;
}

bool SbDecodeTargetGetInfo(SbDecodeTarget decode_target,
                           SbDecodeTargetInfo* out_info) {
  if (!decode_target || !out_info) {
    return false;
  }
  return true;
}
}

namespace gpu {
namespace {

class StarboardGLTextureBackingTest : public SharedImageTestBase {
 public:
  StarboardGLTextureBackingTest() = default;

  void SetUp() override {
    SharedImageTestBase::SetUp();
    ASSERT_NO_FATAL_FAILURE(InitializeContext(GrContextType::kGL));
    g_release_count = 0;
  }
};

TEST_F(StarboardGLTextureBackingTest, Basic) {
  Mailbox mailbox = Mailbox::Generate();
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(100, 100);
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kOpaque_SkAlphaType;
  SharedImageUsageSet usage = SHARED_IMAGE_USAGE_GLES2_READ;
  std::vector<uint32_t> texture_ids = {1};
  std::vector<uint32_t> texture_targets = {0x0DE1};  // GL_TEXTURE_2D
  uint64_t decode_target = 0xdeadbeef;

  auto backing = std::make_unique<StarboardGLTextureBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      texture_ids, texture_targets, decode_target);

  EXPECT_EQ(backing->mailbox(), mailbox);
  EXPECT_EQ(backing->size(), size);
  EXPECT_EQ(backing->format(), format);
  EXPECT_EQ(backing->color_space(), color_space);
  EXPECT_EQ(backing->surface_origin(), surface_origin);
  EXPECT_EQ(backing->alpha_type(), alpha_type);
  EXPECT_EQ(backing->usage(), usage);
  EXPECT_EQ(backing->GetType(), SharedImageBackingType::kGLTexture);

  auto factory_rep =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);

  auto gl_rep = shared_image_manager_.ProduceGLTexturePassthrough(
      mailbox, &memory_type_tracker_);
  ASSERT_NE(gl_rep, nullptr);
  EXPECT_EQ(gl_rep->GetTexturePassthrough()->service_id(), 1u);

  // Test Begin/End access.
  {
    auto scoped_access = gl_rep->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    EXPECT_TRUE(scoped_access);
  }

  gl_rep.reset();
  factory_rep.reset();

  EXPECT_EQ(g_release_count, 1);
}

TEST_F(StarboardGLTextureBackingTest, Multiplanar) {
  Mailbox mailbox = Mailbox::Generate();
  viz::SharedImageFormat format = viz::MultiPlaneFormat::kNV12;
  gfx::Size size(100, 100);
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateREC709();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kOpaque_SkAlphaType;
  SharedImageUsageSet usage = SHARED_IMAGE_USAGE_GLES2_READ;
  // NV12 has two planes.
  std::vector<uint32_t> texture_ids = {1, 2};
  std::vector<uint32_t> texture_targets = {0x0DE1, 0x0DE1};
  uint64_t decode_target = 0xbeefdead;

  auto backing = std::make_unique<StarboardGLTextureBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      texture_ids, texture_targets, decode_target);

  auto factory_rep =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);

  auto gl_rep = shared_image_manager_.ProduceGLTexturePassthrough(
      mailbox, &memory_type_tracker_);
  ASSERT_NE(gl_rep, nullptr);
  EXPECT_EQ(gl_rep->GetTexturePassthrough(0)->service_id(), 1u);
  EXPECT_EQ(gl_rep->GetTexturePassthrough(1)->service_id(), 2u);

  gl_rep.reset();
  factory_rep.reset();

  EXPECT_EQ(g_release_count, 1);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(StarboardGLTextureBackingTest, DrDcLock) {
  Mailbox mailbox = Mailbox::Generate();
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(100, 100);
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kOpaque_SkAlphaType;
  SharedImageUsageSet usage = SHARED_IMAGE_USAGE_GLES2_READ;
  std::vector<uint32_t> texture_ids = {1};
  std::vector<uint32_t> texture_targets = {0x0DE1};
  uint64_t decode_target = 0x12345678;

  auto drdc_lock = base::MakeRefCounted<gpu::RefCountedLock>();

  auto backing = std::make_unique<StarboardGLTextureBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      texture_ids, texture_targets, decode_target, drdc_lock);

  auto factory_rep =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  auto gl_rep = shared_image_manager_.ProduceGLTexturePassthrough(
      mailbox, &memory_type_tracker_);

  // BeginAccess should acquire the lock.
  // In a real scenario, we'd check if the lock is held, but RefCountedLock
  // doesn't expose that easily. We just verify it doesn't crash.
  {
    auto scoped_access = gl_rep->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kNo);
    EXPECT_TRUE(scoped_access);
  }

  gl_rep.reset();
  factory_rep.reset();
  EXPECT_EQ(g_release_count, 1);
}
#endif

}  // namespace
}  // namespace gpu
