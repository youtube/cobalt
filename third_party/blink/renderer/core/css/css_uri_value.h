// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_URI_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_URI_VALUE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Document;
class KURL;
class SVGResource;

namespace cssvalue {

class CORE_EXPORT CSSURIValue : public CSSValue {
 public:
  CSSURIValue(const AtomicString&, const KURL&);
  CSSURIValue(const AtomicString& relative_url,
              const AtomicString& absolute_url);
  explicit CSSURIValue(const AtomicString& absolute_url);
  ~CSSURIValue();

  SVGResource* EnsureResourceReference() const;
  void ReResolveUrl(const Document&) const;

  const AtomicString& ValueForSerialization() const {
    return is_local_ ? relative_url_ : absolute_url_;
  }

  String CustomCSSText() const;

  bool IsLocal(const Document&) const;
  AtomicString FragmentIdentifier() const;

  // Fragment identifier with trailing spaces removed and URL
  // escape sequences decoded. This is cached, because it can take
  // a surprisingly long time to normalize the URL into an absolute
  // value if we have lots of SVG elements that need to re-run this
  // over and over again.
  const AtomicString& NormalizedFragmentIdentifier() const;

  bool Equals(const CSSURIValue&) const;

  CSSURIValue* ComputedCSSValue(const KURL& base_url,
                                const WTF::TextEncoding&) const;

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  KURL AbsoluteUrl() const;

  AtomicString relative_url_;
  mutable AtomicString normalized_fragment_identifier_cache_;
  bool is_local_;

  mutable Member<SVGResource> resource_;
  mutable AtomicString absolute_url_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSURIValue> {
  static bool AllowFrom(const CSSValue& value) { return value.IsURIValue(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_URI_VALUE_H_
