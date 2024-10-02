// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_OPENTYPE_FONT_FORMAT_CHECK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_OPENTYPE_FONT_FORMAT_CHECK_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

class PLATFORM_EXPORT FontFormatCheck {
  STACK_ALLOCATED();

 public:
  explicit FontFormatCheck(sk_sp<SkData>);
  bool IsVariableFont();
  bool IsCbdtCblcColorFont();
  bool IsColrCpalColorFont() {
    return IsColrCpalColorFontV0() || IsColrCpalColorFontV1();
  }
  bool IsColrCpalColorFontV0();
  bool IsColrCpalColorFontV1();
  bool IsSbixColorFont();
  bool IsCff2OutlineFont();
  bool IsColorFont();

  // Still needed in FontCustomPlatformData.
  enum class VariableFontSubType {
    kNotVariable,
    kVariableTrueType,
    kVariableCFF2
  };

  static VariableFontSubType ProbeVariableFont(sk_sp<SkTypeface>);

  // hb-common.h: typedef uint32_t hb_tag_t;
  using TableTagsVector = Vector<uint32_t>;

  enum class COLRVersion { kCOLRV0, kCOLRV1, kNoCOLR };

 private:
  TableTagsVector table_tags_;
  COLRVersion colr_version_ = COLRVersion::kNoCOLR;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_OPENTYPE_FONT_FORMAT_CHECK_H_
