// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DELEGATED_INK_INK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DELEGATED_INK_INK_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExceptionState;
class InkPresenterParam;
class Navigator;
class ScriptPromise;
class ScriptState;

class Ink : public ScriptWrappable, public Supplement<Navigator> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];
  static Ink* ink(Navigator& navigator);

  explicit Ink(Navigator&);
  ScriptPromise requestPresenter(ScriptState* state,
                                 InkPresenterParam* presenter_param,
                                 ExceptionState& exception_state);

  void Trace(blink::Visitor*) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DELEGATED_INK_INK_H_
