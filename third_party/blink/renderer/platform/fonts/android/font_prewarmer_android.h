// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_ANDROID_FONT_PREWARMER_ANDROID_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_ANDROID_FONT_PREWARMER_ANDROID_H_

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/platform/web_font_prewarmer.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class PLATFORM_EXPORT FontPrewarmer : public WebFontPrewarmer {
 public:
  FontPrewarmer();
  ~FontPrewarmer() override = default;

  // WebFontPrewarmer implementation:
  void PrewarmFamily(const WebString& family_name) override;

 private:
  scoped_refptr<base::SequencedTaskRunner> prewarm_task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_ANDROID_FONT_PREWARMER_ANDROID_H_
