// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/dawn_texture_solid_color_pool.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "base/win/scoped_handle.h"
#include "media/base/win/d3d12_mocks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/win/d3d_shared_fence.h"
#include "ui/gl/dc_commit_error.h"
#include "ui/gl/dcomp_mocks.h"

namespace gpu {

using ::testing::_;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;

class MockDawnTextureSolidColorPool : public DawnTextureSolidColorPool {
 public:
  MockDawnTextureSolidColorPool() {
    ON_CALL(*this, CreateEntry).WillByDefault([] {
      return base::WrapUnique(
          new TextureEntry(Microsoft::WRL::Make<media::D3D12ResourceMock>(),
                           Microsoft::WRL::Make<gl::IDCompositionTextureMock>(),
                           wgpu::SharedTextureMemory()));
    });
    ON_CALL(*this, EncodeFill)
        .WillByDefault([](const wgpu::SharedTextureMemory&, const SkColor4f&,
                          scoped_refptr<gfx::D3DSharedFence>,
                          bool) { return base::ok(); });
    ON_CALL(*this, CheckAvailableFence)
        .WillByDefault(
            Return(MockDawnTextureSolidColorPool::FenceCheckResult{}));
  }

  MOCK_METHOD((base::expected<std::unique_ptr<TextureEntry>, gl::CommitError>),
              CreateEntry,
              (),
              (override));
  MOCK_METHOD((base::expected<void, gl::CommitError>),
              EncodeFill,
              (const wgpu::SharedTextureMemory&,
               const SkColor4f&,
               scoped_refptr<gfx::D3DSharedFence>,
               bool),
              (override));
  MOCK_METHOD(FenceCheckResult,
              CheckAvailableFence,
              (IDCompositionTexture*),
              (override));

  using DawnTextureSolidColorPool::entries;
  using DawnTextureSolidColorPool::FenceCheckResult;
};

namespace {

class DawnTextureSolidColorPoolTest : public ::testing::Test {
 protected:
  // Runs the per-frame end-of-frame sequence on the pool so the
  // previously-filled entry remains usable on the next call.
  void EndFrame() {
    ASSERT_EQ(pool_.FlushPendingFillsBeforeCommit(), base::ok());
    pool_.TrimAfterCommit();
  }

  // Calls `GetSolidColorContent(color)` and asserts it returned a content
  // pointer backed by an entry whose cached color matches `color`. Returns
  // the dcomp texture so callers can compare across calls (e.g. for
  // cache-hit identity).
  IDCompositionTexture* ValidateContent(const SkColor4f& color) {
    auto result = pool_.GetSolidColorContent(color);
    EXPECT_TRUE(result.has_value()) << "GetSolidColorContent failed";
    if (!result.has_value()) {
      return nullptr;
    }
    auto* content = static_cast<IDCompositionTexture*>(result.value());
    EXPECT_NE(content, nullptr) << "GetSolidColorContent returned null content";

    for (const auto& entry : pool_.entries()) {
      auto* texture_entry =
          static_cast<DawnTextureSolidColorPool::TextureEntry*>(entry.get());
      if (texture_entry->dcomp_texture() == content) {
        EXPECT_TRUE(entry->color().has_value())
            << "Returned entry has no cached color";
        if (entry->color().has_value()) {
          EXPECT_EQ(*entry->color(), color)
              << "Returned entry's cached color doesn't match request";
        }
        return content;
      }
    }
    ADD_FAILURE() << "Returned content doesn't match any pool entry";
    return content;
  }

  NiceMock<MockDawnTextureSolidColorPool> pool_;
};

// Test the fresh entry path where pool calls FillEntry with a null entry.
TEST_F(DawnTextureSolidColorPoolTest, FreshEntry_AllocatesAndFills) {
  EXPECT_CALL(pool_, CreateEntry).Times(1);
  EXPECT_CALL(pool_, EncodeFill(_, SkColors::kRed, IsNull(), false)).Times(1);
  EXPECT_CALL(pool_, CheckAvailableFence).Times(0);

  ValidateContent(SkColors::kRed);
  EXPECT_EQ(pool_.GetNumEntriesForTesting(), 1u);
}

// Test the reuse path with fully-completed fence. No D3D12 / dcomp rebuilds;
// EncodeFill should be called with no wait_fence and initialized=true.
TEST_F(DawnTextureSolidColorPoolTest, ReuseEntry_FenceCompleted) {
  EXPECT_CALL(pool_, CreateEntry).Times(1);
  EXPECT_CALL(pool_, CheckAvailableFence).Times(1);
  EXPECT_CALL(pool_, EncodeFill(_, SkColors::kRed, IsNull(), false)).Times(1);
  EXPECT_CALL(pool_, EncodeFill(_, SkColors::kBlue, IsNull(), true)).Times(1);

  IDCompositionTexture* red_content = ValidateContent(SkColors::kRed);
  EndFrame();
  IDCompositionTexture* blue_content = ValidateContent(SkColors::kBlue);

  EXPECT_EQ(red_content, blue_content);
  EXPECT_EQ(pool_.GetNumEntriesForTesting(), 1u);
}

// Test the reuse path with available but incomplete fence.
TEST_F(DawnTextureSolidColorPoolTest, ReuseEntry_FenceNotCompleted) {
  scoped_refptr<gfx::D3DSharedFence> fake_wait_fence =
      gfx::D3DSharedFence::CreateFromScopedHandle(base::win::ScopedHandle{},
                                                  gfx::DXGIHandleToken());
  ASSERT_TRUE(fake_wait_fence);

  EXPECT_CALL(pool_, CreateEntry).Times(1);
  EXPECT_CALL(pool_, CheckAvailableFence)
      .WillOnce(Return(MockDawnTextureSolidColorPool::FenceCheckResult{
          .wait_fence = fake_wait_fence}));
  EXPECT_CALL(pool_, EncodeFill(_, SkColors::kRed, IsNull(), false)).Times(1);
  EXPECT_CALL(pool_, EncodeFill(_, SkColors::kBlue, NotNull(), true)).Times(1);

  IDCompositionTexture* red_content = ValidateContent(SkColors::kRed);
  EndFrame();
  IDCompositionTexture* blue_content = ValidateContent(SkColors::kBlue);

  EXPECT_EQ(red_content, blue_content);
  EXPECT_EQ(pool_.GetNumEntriesForTesting(), 1u);
}

// Reuse path with unavailable fence.
TEST_F(DawnTextureSolidColorPoolTest, ReuseEntry_NullFenceRebuildsResources) {
  EXPECT_CALL(pool_, CreateEntry).Times(2);
  EXPECT_CALL(pool_, CheckAvailableFence)
      .WillOnce(Return(MockDawnTextureSolidColorPool::FenceCheckResult{
          .needs_rebuild = true}));
  EXPECT_CALL(pool_, EncodeFill(_, SkColors::kRed, IsNull(), false)).Times(1);
  EXPECT_CALL(pool_, EncodeFill(_, SkColors::kBlue, IsNull(), false)).Times(1);

  ValidateContent(SkColors::kRed);
  EndFrame();
  ValidateContent(SkColors::kBlue);

  EXPECT_EQ(pool_.GetNumEntriesForTesting(), 1u);
}

}  // namespace
}  // namespace gpu
