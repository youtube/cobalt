/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_TABLE_CELL_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_TABLE_CELL_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_table_part_element.h"

namespace blink {

class CORE_EXPORT HTMLTableCellElement final : public HTMLTablePartElement {
  DEFINE_WRAPPERTYPEINFO();

 public:

  HTMLTableCellElement(const QualifiedName&, Document&);

  int cellIndex() const;

  unsigned colSpan() const;
  unsigned rowSpan() const;

  void SetCellIndex(int);

  const AtomicString& Abbr() const;
  const AtomicString& Axis() const;
  void setColSpan(unsigned);
  const AtomicString& Headers() const;
  void setRowSpan(unsigned);

  bool HasNonInBodyInsertionMode() const override { return true; }

 private:
  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      MutableCSSPropertyValueSet*) override;
  const CSSPropertyValueSet* AdditionalPresentationAttributeStyle() override;

  bool IsURLAttribute(const Attribute&) const override;
  bool HasLegalLinkAttribute(const QualifiedName&) const override;
  const QualifiedName& SubResourceAttributeName() const override;
};

template <>
inline bool IsElementOfType<const HTMLTableCellElement>(const Node& node) {
  return IsA<HTMLTableCellElement>(node);
}
template <>
struct DowncastTraits<HTMLTableCellElement> {
  static bool AllowFrom(const Node& node) {
    auto* html_element = DynamicTo<HTMLElement>(node);
    return html_element && AllowFrom(*html_element);
  }
  static bool AllowFrom(const HTMLElement& html_element) {
    return html_element.HasTagName(html_names::kTdTag) ||
           html_element.HasTagName(html_names::kThTag);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_TABLE_CELL_ELEMENT_H_
