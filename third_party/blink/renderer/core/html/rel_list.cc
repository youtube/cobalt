// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/rel_list.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

RelList::RelList(Element* element)
    : DOMTokenList(*element, html_names::kRelAttr) {}

static HashSet<AtomicString>& SupportedTokensLink() {
  // There is a use counter for <link rel="monetization"> but the feature is
  // actually not implemented yet, so "monetization" is not included in the
  // list below. See https://crbug.com/1031476
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, tokens,
                      ({
                          "preload",
                          "preconnect",
                          "dns-prefetch",
                          "stylesheet",
                          "icon",
                          "alternate",
                          "prefetch",
                          "prerender",
                          "next",
                          "manifest",
                          "apple-touch-icon",
                          "apple-touch-icon-precomposed",
                          "canonical",
                          "modulepreload",
                          "allowed-alt-sxg",
                      }));

  return tokens;
}

static HashSet<AtomicString>& SupportedTokensAnchorAndAreaAndForm() {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, tokens,
                      ({
                          "noreferrer",
                          "noopener",
                          "opener",
                      }));

  return tokens;
}

bool RelList::ValidateTokenValue(const AtomicString& token_value,
                                 ExceptionState&) const {
  //  https://html.spec.whatwg.org/C/#linkTypes
  if (GetElement().HasTagName(html_names::kLinkTag)) {
    if (SupportedTokensLink().Contains(token_value)) {
      return true;
    }
  } else if ((GetElement().HasTagName(html_names::kATag) ||
              GetElement().HasTagName(html_names::kAreaTag) ||
              GetElement().HasTagName(html_names::kFormTag)) &&
             SupportedTokensAnchorAndAreaAndForm().Contains(token_value)) {
    return true;
  }
  return false;
}

}  // namespace blink
