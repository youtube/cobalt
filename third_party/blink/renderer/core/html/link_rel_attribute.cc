/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/html/link_rel_attribute.h"


namespace blink {

LinkRelAttribute::LinkRelAttribute()
    : icon_type_(mojom::blink::FaviconIconType::kInvalid),
      is_style_sheet_(false),
      is_alternate_(false),
      is_dns_prefetch_(false),
      is_preconnect_(false),
      is_link_prefetch_(false),
      is_link_preload_(false),
      is_link_prerender_(false),
      is_link_next_(false),
      is_manifest_(false),
      is_module_preload_(false),
      is_service_worker_(false),
      is_canonical_(false),
      is_monetization_(false) {}

LinkRelAttribute::LinkRelAttribute(const String& rel) : LinkRelAttribute() {
  if (rel.empty())
    return;
  String rel_copy = rel;
  rel_copy.Replace('\n', ' ');
  Vector<String> list;
  rel_copy.Split(' ', list);
  for (const String& link_type : list) {
    if (EqualIgnoringASCIICase(link_type, "stylesheet")) {
      is_style_sheet_ = true;
    } else if (EqualIgnoringASCIICase(link_type, "alternate")) {
      is_alternate_ = true;
    } else if (EqualIgnoringASCIICase(link_type, "icon")) {
      // This also allows "shortcut icon" since we just ignore the non-standard
      // "shortcut" token (in accordance with the spec).
      icon_type_ = mojom::blink::FaviconIconType::kFavicon;
    } else if (EqualIgnoringASCIICase(link_type, "prefetch")) {
      is_link_prefetch_ = true;
    } else if (EqualIgnoringASCIICase(link_type, "dns-prefetch")) {
      is_dns_prefetch_ = true;
    } else if (EqualIgnoringASCIICase(link_type, "preconnect")) {
      is_preconnect_ = true;
    } else if (EqualIgnoringASCIICase(link_type, "preload")) {
      is_link_preload_ = true;
    } else if (EqualIgnoringASCIICase(link_type, "prerender")) {
      is_link_prerender_ = true;
    } else if (EqualIgnoringASCIICase(link_type, "next")) {
      is_link_next_ = true;
    } else if (EqualIgnoringASCIICase(link_type, "apple-touch-icon")) {
      icon_type_ = mojom::blink::FaviconIconType::kTouchIcon;
    } else if (EqualIgnoringASCIICase(link_type,
                                      "apple-touch-icon-precomposed")) {
      icon_type_ = mojom::blink::FaviconIconType::kTouchPrecomposedIcon;
    } else if (EqualIgnoringASCIICase(link_type, "manifest")) {
      is_manifest_ = true;
    } else if (EqualIgnoringASCIICase(link_type, "modulepreload")) {
      is_module_preload_ = true;
    } else if (EqualIgnoringASCIICase(link_type, "serviceworker")) {
      is_service_worker_ = true;
    } else if (EqualIgnoringASCIICase(link_type, "canonical")) {
      is_canonical_ = true;
    } else if (EqualIgnoringASCIICase(link_type, "monetization")) {
      is_monetization_ = true;
    }
    // Adding or removing a value here requires you to update
    // RelList::supportedTokens()
  }
}

}  // namespace blink
