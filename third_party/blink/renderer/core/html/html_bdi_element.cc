// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_bdi_element.h"

namespace blink {

HTMLBDIElement::HTMLBDIElement(Document& document)
    : HTMLElement(html_names::kBdiTag, document) {
  // <bdi> defaults to dir="auto"
  // https://html.spec.whatwg.org/C/#the-bdi-element
  SetSelfOrAncestorHasDirAutoAttribute();
  GetDocument().SetDirAttributeDirty();
}

}  // namespace blink
