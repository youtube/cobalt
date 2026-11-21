// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_SUB_MENU_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_SUB_MENU_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

class HTMLMenuItemElement;
class HTMLMenuListElement;

class CORE_EXPORT HTMLSubMenuElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLSubMenuElement(Document&);
  ~HTMLSubMenuElement() override;

  ElementType GetElementType() const final {
    return ElementType::kHTMLSubMenuElement;
  }

  HTMLMenuItemElement* MenuItem() const;
  HTMLMenuListElement* MenuList() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_SUB_MENU_ELEMENT_H_
