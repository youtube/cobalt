/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Motorola Mobility Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/url/dom_url_utils.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/known_ports.h"

namespace blink {

DOMURLUtils::~DOMURLUtils() = default;

void DOMURLUtils::setProtocol(const String& value) {
  KURL kurl = Url();
  if (kurl.IsNull())
    return;
  kurl.SetProtocol(value);
  SetURL(kurl);
}

void DOMURLUtils::setUsername(const String& value) {
  KURL kurl = Url();
  if (kurl.IsNull())
    return;
  kurl.SetUser(value);
  if (kurl.IsValid())
    SetURL(kurl);
}

void DOMURLUtils::setPassword(const String& value) {
  KURL kurl = Url();
  if (kurl.IsNull())
    return;
  kurl.SetPass(value);
  if (kurl.IsValid())
    SetURL(kurl);
}

void DOMURLUtils::setHost(const String& value) {
  if (value.empty())
    return;

  KURL kurl = Url();
  if (!kurl.CanSetHostOrPort())
    return;

  kurl.SetHostAndPort(value);
  if (kurl.IsValid())
    SetURL(kurl);
}

void DOMURLUtils::setHostname(const String& value) {
  if (value.empty())
    return;

  KURL kurl = Url();
  if (!kurl.CanSetHostOrPort())
    return;

  kurl.SetHost(value);
  if (kurl.IsValid())
    SetURL(kurl);
}

void DOMURLUtils::setPort(ExecutionContext* execution_context,
                          const String& value) {
  KURL kurl = Url();
  if (!kurl.CanSetHostOrPort())
    return;
  if (!value.empty()) {
    bool value_overflow;
    kurl.SetPort(value, &value_overflow);
    if (value_overflow &&
        base::FeatureList::IsEnabled(features::kURLSetPortCheckOverflow)) {
      execution_context->CountUse(
          mojom::blink::WebFeature::kURLSetPortCheckOverflow);
    }
  } else {
    kurl.RemovePort();
  }
  if (kurl.IsValid())
    SetURL(kurl);
}

void DOMURLUtils::setPathname(const String& value) {
  KURL kurl = Url();
  if (!kurl.CanSetPathname())
    return;
  kurl.SetPath(value);
  if (kurl.IsValid())
    SetURL(kurl);
}

void DOMURLUtils::setSearch(const String& value) {
  SetSearchInternal(value);
}

void DOMURLUtils::SetSearchInternal(const String& value) {
  DCHECK(!is_in_update_);
  KURL kurl = Url();
  if (!kurl.IsValid())
    return;

  // FIXME: have KURL do this clearing of the query component
  // instead, if practical. Will require addressing
  // http://crbug.com/108690, for one.
  if ((value.length() == 1 && value[0] == '?') || value.empty())
    kurl.SetQuery(String());
  else
    kurl.SetQuery(value);

  if (kurl.IsValid())
    SetURL(kurl);
}

void DOMURLUtils::setHash(const String& value) {
  KURL kurl = Url();
  if (kurl.IsNull())
    return;

  // FIXME: have KURL handle the clearing of the fragment component
  // on the same input.
  if (value[0] == '#')
    kurl.SetFragmentIdentifier(value.Substring(1));
  else {
    if (value.empty())
      kurl.RemoveFragmentIdentifier();
    else
      kurl.SetFragmentIdentifier(value);
  }

  if (kurl.IsValid())
    SetURL(kurl);
}

}  // namespace blink
