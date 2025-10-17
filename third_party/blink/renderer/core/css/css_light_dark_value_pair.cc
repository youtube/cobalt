// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_light_dark_value_pair.h"

#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

String CSSLightDarkValuePair::CustomCSSText() const {
  String first = First().CssText();
  String second = Second().CssText();
  return WTF::StrCat({"light-dark(", first, ", ", second, ")"});
}

}  // namespace blink
