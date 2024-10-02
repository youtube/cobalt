// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATE_EVENT_DISPATCH_PARAMS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATE_EVENT_DISPATCH_PARAMS_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class HTMLFormElement;
class HistoryItem;
class SerializedScriptValue;

// TODO(japhet): This should probably move to frame_loader_types.h and possibly
// be used more broadly once it is in the HTML spec.
enum class UserNavigationInvolvement { kBrowserUI, kActivation, kNone };
enum class NavigateEventType { kFragment, kHistoryApi, kCrossDocument };

struct CORE_EXPORT NavigateEventDispatchParams
    : public GarbageCollected<NavigateEventDispatchParams> {
 public:
  NavigateEventDispatchParams(const KURL&, NavigateEventType, WebFrameLoadType);
  ~NavigateEventDispatchParams();

  const KURL url;
  const NavigateEventType event_type;
  const WebFrameLoadType frame_load_type;
  UserNavigationInvolvement involvement = UserNavigationInvolvement::kNone;
  Member<HTMLFormElement> form;
  scoped_refptr<SerializedScriptValue> state_object;
  Member<HistoryItem> destination_item;
  bool is_browser_initiated = false;
  bool is_synchronously_committed_same_document = true;
  String download_filename;

  void Trace(Visitor*) const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATE_EVENT_DISPATCH_PARAMS_H_
