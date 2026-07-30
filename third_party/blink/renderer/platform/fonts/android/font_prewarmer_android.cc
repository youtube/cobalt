// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/android/font_prewarmer_android.h"

#include "base/task/thread_pool.h"
#include "skia/ext/font_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

namespace {

void PrewarmFamilyOnBackgroundThread(const std::string& family_name) {
  TRACE_EVENT1("fonts", "PrewarmFamilyOnBackgroundThread", "family",
               family_name);
  sk_sp<SkFontMgr> font_mgr = skia::DefaultFontMgr();
  if (font_mgr) {
    // We match style Normal to preload the base typeface.
    font_mgr->matchFamilyStyle(family_name.c_str(), SkFontStyle::Normal());
  }
}

}  // namespace

FontPrewarmer::FontPrewarmer() {
  prewarm_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

void FontPrewarmer::PrewarmFamily(const WebString& family_name) {
  if (family_name.IsEmpty()) {
    return;
  }
  std::string family_name_utf8 = family_name.Utf8();
  TRACE_EVENT1("fonts", "FontPrewarmer::PrewarmFamily", "family",
               family_name_utf8);
  prewarm_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PrewarmFamilyOnBackgroundThread,
                                std::move(family_name_utf8)));
}

}  // namespace blink
