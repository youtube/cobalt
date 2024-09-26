// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_UNRESOLVED_PROPERTY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_UNRESOLVED_PROPERTY_H_

#include "base/containers/span.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/properties/css_exposure.h"
#include "third_party/blink/renderer/core/css/properties/css_property_instances.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;
CORE_EXPORT const CSSUnresolvedProperty& GetCSSPropertyVariableInternal();

// TODO(crbug.com/793288): audit and consider redesigning how aliases are
// handled once more of project Ribbon is done and all use of aliases can be
// found and (hopefully) constrained.
class CORE_EXPORT CSSUnresolvedProperty {
 public:
  static const CSSUnresolvedProperty& Get(CSSPropertyID);
  static const CSSUnresolvedProperty* GetAliasProperty(CSSPropertyID);

  // Origin trials are taken into account only when a non-nullptr
  // ExecutionContext is provided.
  bool IsWebExposed(const ExecutionContext* context = nullptr) const {
    return blink::IsWebExposed(Exposure(context));
  }
  bool IsUAExposed(const ExecutionContext* context = nullptr) const {
    return blink::IsUAExposed(Exposure(context));
  }
  virtual CSSExposure Exposure(const ExecutionContext* = nullptr) const {
    return CSSExposure::kWeb;
  }

  virtual bool IsResolvedProperty() const { return false; }
  virtual const char* GetPropertyName() const {
    NOTREACHED();
    return nullptr;
  }
  virtual const WTF::AtomicString& GetPropertyNameAtomicString() const {
    NOTREACHED();
    return g_empty_atom;
  }
  virtual const char* GetJSPropertyName() const {
    NOTREACHED();
    return "";
  }
  WTF::String GetPropertyNameString() const {
    // We share the StringImpl with the AtomicStrings.
    return GetPropertyNameAtomicString().GetString();
  }
  // See documentation near "alternative_of" in css_properties.json5.
  virtual CSSPropertyID GetAlternative() const {
    return CSSPropertyID::kInvalid;
  }

 protected:
  static const CSSUnresolvedProperty& GetNonAliasProperty(CSSPropertyID id) {
    if (id == CSSPropertyID::kVariable) {
      return GetCSSPropertyVariableInternal();
    }
    return *kPropertyClasses[static_cast<int>(id)];
  }

  constexpr CSSUnresolvedProperty() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_UNRESOLVED_PROPERTY_H_
