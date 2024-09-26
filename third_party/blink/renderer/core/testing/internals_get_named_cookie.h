// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_INTERNALS_GET_NAMED_COOKIE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_INTERNALS_GET_NAMED_COOKIE_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class Internals;
class ScriptPromise;
class ScriptState;

class InternalsGetNamedCookie {
  STATIC_ONLY(InternalsGetNamedCookie);

 public:
  static ScriptPromise getNamedCookie(ScriptState* script_state,
                                      Internals& internals,
                                      const String& name);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_INTERNALS_GET_NAMED_COOKIE_H_
