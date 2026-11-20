// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/solid_color_pool_base.h"

#include <windows.h>

#include <wrl/client.h>
#include <wrl/implements.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/types/expected.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gl/dc_commit_error.h"

namespace gl {
namespace {

// Fake IUnknown implementation so that we're not dependent on any specific
// DComp objects for the test.
class FakeContent
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUnknown> {};

class FakeSolidColorPool : public SolidColorPoolBase {
 public:
  void FailNextFill(CommitError error) { fail_next_ = error; }
  int fill_call_count() const { return fill_call_count_; }
  using SolidColorPoolBase::entries;

 protected:
  base::expected<void, CommitError> FillEntry(std::unique_ptr<Entry>& entry,
                                              const SkColor4f& color) override {
    ++fill_call_count_;
    if (fail_next_) {
      CommitError error = *fail_next_;
      fail_next_.reset();
      return base::unexpected(error);
    }
    if (!entry) {
      entry = std::make_unique<FakeEntry>();
    }
    return base::ok();
  }

 private:
  class FakeEntry : public Entry {
   public:
    FakeEntry() : content_(Microsoft::WRL::Make<FakeContent>()) {}
    IUnknown* GetContent() const override { return content_.Get(); }

   private:
    Microsoft::WRL::ComPtr<IUnknown> content_;
  };

  int fill_call_count_ = 0;
  std::optional<CommitError> fail_next_;
};

class SolidColorPoolBaseTest : public ::testing::Test {
 protected:
  IUnknown* GetContent(const SkColor4f& color) {
    auto result = pool_.GetSolidColorContent(color);
    return result.value_or(nullptr);
  }
  void EndFrame() {
    ASSERT_EQ(pool_.FlushPendingFillsBeforeCommit(), base::ok());
    pool_.TrimAfterCommit();
  }

  FakeSolidColorPool pool_;
};

// Fill a fresh entry, happy path.
TEST_F(SolidColorPoolBaseTest, FreshEntry_AllocatesAndFills) {
  EXPECT_NE(GetContent(SkColors::kRed), nullptr);
  EXPECT_EQ(pool_.fill_call_count(), 1);
  EXPECT_EQ(pool_.GetNumEntriesForTesting(), 1u);
}

// Same color requested twice in one frame; `FillEntry` runs exactly once and
// both calls return the same content.
TEST_F(SolidColorPoolBaseTest, CacheHit_SameFrame) {
  IUnknown* first = GetContent(SkColors::kRed);
  IUnknown* second = GetContent(SkColors::kRed);
  EXPECT_EQ(first, second);
  EXPECT_EQ(pool_.fill_call_count(), 1);
  EXPECT_EQ(pool_.GetNumEntriesForTesting(), 1u);
}

// Cached entry survives the trim at the end of the frame and is reused on the
// next frame.
TEST_F(SolidColorPoolBaseTest, CacheHit_AcrossFrames) {
  IUnknown* first = GetContent(SkColors::kRed);
  EndFrame();
  IUnknown* second = GetContent(SkColors::kRed);
  EXPECT_EQ(first, second);
  EXPECT_EQ(pool_.fill_call_count(), 1);
  EXPECT_EQ(pool_.GetNumEntriesForTesting(), 1u);
}

// Fill a fresh entry, end the frame, reuse the same entry next frame for a new
// color.
TEST_F(SolidColorPoolBaseTest, ReuseEntry_RefillsWithNewColor) {
  IUnknown* red = GetContent(SkColors::kRed);
  EndFrame();

  IUnknown* blue = GetContent(SkColors::kBlue);
  EXPECT_EQ(red, blue) << "Reuse path must keep the same slot/content pointer";
  EXPECT_EQ(pool_.fill_call_count(), 2);
  EXPECT_EQ(pool_.GetNumEntriesForTesting(), 1u);
}

// A failure on the fresh-alloc path must not publish a partial entry.
TEST_F(SolidColorPoolBaseTest, FreshEntry_FailureLeavesPoolEmpty) {
  pool_.FailNextFill({CommitError::Reason::kUnknown});
  EXPECT_FALSE(GetContent(SkColors::kRed));
  EXPECT_EQ(pool_.GetNumEntriesForTesting(), 0u);
}

// A failure on the reuse path must invalidate the entry's cached color,
// so a subsequent request for the original color cannot return a
// spurious cache hit on the now-empty entry.
TEST_F(SolidColorPoolBaseTest, ReuseEntry_FailureInvalidatesColorCache) {
  GetContent(SkColors::kRed);
  EndFrame();

  pool_.FailNextFill({CommitError::Reason::kUnknown});
  EXPECT_FALSE(GetContent(SkColors::kBlue));
  EndFrame();
  // The failed entry will remain in the pool, but there should be no cached
  // color.
  EXPECT_EQ(pool_.GetNumEntriesForTesting(), 1u);
  EXPECT_EQ(pool_.entries()[0]->color(), std::nullopt);

  // A subsequent red request must go through the reuse path again (not
  // hit a stale cache), which means another FillEntry call.
  GetContent(SkColors::kRed);
  EXPECT_EQ(pool_.fill_call_count(), 3);
  EXPECT_EQ(pool_.GetNumEntriesForTesting(), 1u);
}

// Two distinct colors requested produce two filled entries.
TEST_F(SolidColorPoolBaseTest, MultipleColorsInOneFrame) {
  IUnknown* red = GetContent(SkColors::kRed);
  IUnknown* blue = GetContent(SkColors::kBlue);
  EXPECT_NE(red, blue);
  EXPECT_EQ(pool_.fill_call_count(), 2);
  EXPECT_EQ(pool_.GetNumEntriesForTesting(), 2u);
}

}  // namespace
}  // namespace gl
