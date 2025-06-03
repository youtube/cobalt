// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_ELEMENT_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSPrimitiveValue;
class CSSToLengthConversionData;
class QualifiedName;

class CORE_EXPORT MathMLElement : public Element {
  DEFINE_WRAPPERTYPEINFO();

 public:
  MathMLElement(const QualifiedName& tagName,
                Document& document,
                ConstructionType constructionType = kCreateMathMLElement);
  ~MathMLElement() override;

  bool HasTagName(const MathMLQualifiedName& name) const {
    return HasLocalName(name.LocalName());
  }

  bool IsMathMLElement() const =
      delete;  // This will catch anyone doing an unnecessary check.
  bool IsStyledElement() const =
      delete;  // This will catch anyone doing an unnecessary check.

  virtual bool IsGroupingElement() const { return false; }

 protected:
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      MutableCSSPropertyValueSet*) override;

  enum class AllowPercentages { kYes, kNo };
  const CSSPrimitiveValue* ParseMathLength(
      const QualifiedName& attr_name,
      AllowPercentages allow_percentages = AllowPercentages::kYes,
      CSSPrimitiveValue::ValueRange value_range =
          CSSPrimitiveValue::ValueRange::kAll);
  absl::optional<Length> AddMathLengthToComputedStyle(
      const CSSToLengthConversionData&,
      const QualifiedName&,
      AllowPercentages allow_percentages = AllowPercentages::kYes,
      CSSPrimitiveValue::ValueRange value_range =
          CSSPrimitiveValue::ValueRange::kAll);

  void ParseAttribute(const AttributeModificationParams&) override;

  // https://w3c.github.io/mathml-core/#dfn-boolean
  absl::optional<bool> BooleanAttribute(const QualifiedName& name) const;
};

template <typename T>
bool IsElementOfType(const MathMLElement&);
template <>
inline bool IsElementOfType<const MathMLElement>(const MathMLElement&) {
  return true;
}
template <>
inline bool IsElementOfType<const MathMLElement>(const Node& node) {
  return IsA<MathMLElement>(node);
}
template <>
struct DowncastTraits<MathMLElement> {
  static bool AllowFrom(const Node& node) { return node.IsMathMLElement(); }
};

inline bool Node::HasTagName(const MathMLQualifiedName& name) const {
  auto* mathml_element = DynamicTo<MathMLElement>(this);
  return mathml_element && mathml_element->HasTagName(name);
}

}  // namespace blink

#include "third_party/blink/renderer/core/mathml_element_type_helpers.h"

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MATHML_MATHML_ELEMENT_H_
