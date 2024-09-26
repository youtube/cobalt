// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_FONT_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_FONT_FEATURES_H_

#include <hb.h>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Font;
class FontDescription;

// Represents a list of OpenType font feature settings.
class PLATFORM_EXPORT FontFeatures {
  STACK_ALLOCATED();

 public:
  // Initialize the list from |Font|.
  void Initialize(const FontDescription&);

  wtf_size_t size() const { return features_.size(); }
  bool IsEmpty() const { return features_.empty(); }

  const hb_feature_t* data() const { return features_.data(); }

  absl::optional<unsigned> FindValueForTesting(hb_tag_t tag) const;

  void Append(const hb_feature_t& feature) { features_.push_back(feature); }
  void Insert(const hb_feature_t& feature) { features_.push_front(feature); }

  void EraseAt(wtf_size_t position, wtf_size_t length) {
    features_.EraseAt(position, length);
  }

 private:
  Vector<hb_feature_t, 6> features_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_FONT_FEATURES_H_
