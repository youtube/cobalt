// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DOM_OVERLAY_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DOM_OVERLAY_STATE_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Implementation of
// https://immersive-web.github.io/dom-overlays/#dictdef-xrdomoverlaystate, used
// as SameObject instances owned by |XRSession|.
class XRDOMOverlayState : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class DOMOverlayType {
    kScreen = 0,
    kFloating,
  };

  explicit XRDOMOverlayState(DOMOverlayType type);
  ~XRDOMOverlayState() override = default;

  const String& type() const { return type_string_; }

  void Trace(Visitor*) const override;

 private:
  const String type_string_;
  // Currently, instances of this class are created at session start, on the
  // assumption that the objects are very small. If this becomes more complex in
  // the future, i.e. when adding additional members, consider switching to lazy
  // instantiation.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_DOM_OVERLAY_STATE_H_
