// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_TESTING_INTERNALS_FED_CM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_TESTING_INTERNALS_FED_CM_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ExceptionState;
class Internals;
class ScriptPromise;
class ScriptState;

class InternalsFedCm {
  STATIC_ONLY(InternalsFedCm);

 public:
  static ScriptPromise getFedCmDialogType(ScriptState*, Internals&);
  static ScriptPromise getFedCmTitle(ScriptState*, Internals&);
  static ScriptPromise selectFedCmAccount(ScriptState*,
                                          Internals&,
                                          int account_index,
                                          ExceptionState&);
  static ScriptPromise dismissFedCmDialog(ScriptState*, Internals&);
  static ScriptPromise confirmIdpLogin(ScriptState*, Internals&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_TESTING_INTERNALS_FED_CM_H_
