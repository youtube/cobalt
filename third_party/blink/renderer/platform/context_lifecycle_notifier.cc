// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/context_lifecycle_notifier.h"

#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#if BUILDFLAG(IS_COBALT)
#include "base/memory/raw_ptr.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#endif
#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"

namespace blink {

ContextLifecycleNotifier::~ContextLifecycleNotifier() {
  // `NotifyContextDestroyed()` must be called prior to destruction.
  DCHECK(context_destroyed_);
}

bool ContextLifecycleNotifier::IsContextDestroyed() const {
  return context_destroyed_;
}

void ContextLifecycleNotifier::AddContextLifecycleObserver(
    ContextLifecycleObserver* observer) {
  observers_.AddObserver(observer);
}

void ContextLifecycleNotifier::RemoveContextLifecycleObserver(
    ContextLifecycleObserver* observer) {
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

void ContextLifecycleNotifier::NotifyContextDestroyed() {
  context_destroyed_ = true;

  ScriptForbiddenScope forbid_script;
#if !BUILDFLAG(IS_COBALT)
  observers_.ForEachObserver([](ContextLifecycleObserver* observer) {
    observer->NotifyContextDestroyed();
  });
#else
  // This is a fix attempt. We do not have repro for related crsahes.
  // TODO: b/427982512 - Verify if the crash disappears from Prime after.
  // Make a copy of the observers to iterate over, to avoid problems with
  // re-entrancy if observers are added/removed during notification.
  Vector<raw_ptr<ContextLifecycleObserver>> observers_copy;
  observers_.ForEachObserver(
      [&observers_copy](ContextLifecycleObserver* obs) {
        observers_copy.push_back(obs);
      });

  for (auto& observer : observers_copy) {
    // An observer might have been removed from the original set by another
    // observer's destruction logic, so check it's still there before notifying.
    if (observers_.HasObserver(observer)) {
      observer->NotifyContextDestroyed();
    }
  }
#endif

  observers_.Clear();
}

void ContextLifecycleNotifier::Trace(Visitor* visitor) const {
  visitor->Trace(observers_);
}

}  // namespace blink
