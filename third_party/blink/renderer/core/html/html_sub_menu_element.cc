// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_sub_menu_element.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/html_menu_item_element.h"
#include "third_party/blink/renderer/core/html/html_menu_list_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

HTMLSubMenuElement::HTMLSubMenuElement(Document& document)
    : HTMLElement(html_names::kSubmenuTag, document) {
  DCHECK(RuntimeEnabledFeatures::MenuElementsEnabled());
}

HTMLSubMenuElement::~HTMLSubMenuElement() = default;

HTMLMenuItemElement* HTMLSubMenuElement::MenuItem() const {
  return Traversal<HTMLMenuItemElement>::FirstChild(*this);
}

HTMLMenuListElement* HTMLSubMenuElement::MenuList() const {
  return Traversal<HTMLMenuListElement>::FirstChild(*this);
}

}  // namespace blink
