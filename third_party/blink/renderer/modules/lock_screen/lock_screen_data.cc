// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/lock_screen/lock_screen_data.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

const char LockScreenData::kSupplementName[] = "LockScreenData";

LockScreenData::LockScreenData(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

LockScreenData::~LockScreenData() = default;

ScriptPromise LockScreenData::getLockScreenData(ScriptState* script_state,
                                                LocalDOMWindow& window) {
  LockScreenData* supplement =
      Supplement<LocalDOMWindow>::From<LockScreenData>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<LockScreenData>(window);
    ProvideTo(window, supplement);
  }
  return supplement->GetLockScreenData(script_state);
}

ScriptPromise LockScreenData::GetLockScreenData(ScriptState* script_state) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  resolver->Resolve(this);
  return promise;
}

ScriptPromise LockScreenData::getKeys(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // TODO(crbug.com/1006642): This should call out to a mojo service instead.
  Vector<String> keys;
  keys.ReserveInitialCapacity(static_cast<wtf_size_t>(fake_data_store_.size()));
  for (const auto& it : fake_data_store_) {
    keys.push_back(it.key);
  }
  resolver->Resolve(std::move(keys));
  return promise;
}

ScriptPromise LockScreenData::getData(ScriptState* script_state,
                                      const String& key) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // TODO(crbug.com/1006642): This should call out to a mojo service instead.
  auto it = fake_data_store_.find(key);
  if (it == fake_data_store_.end()) {
    resolver->Resolve();
  } else {
    resolver->Resolve(it->value);
  }
  return promise;
}

ScriptPromise LockScreenData::setData(ScriptState* script_state,
                                      const String& key,
                                      const String& data) {
  // TODO(crbug.com/1006642): This should call out to a mojo service instead.
  fake_data_store_.Set(key, data);

  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise LockScreenData::deleteData(ScriptState* script_state,
                                         const String& key) {
  // TODO(crbug.com/1006642): This should call out to a mojo service instead.
  fake_data_store_.erase(key);

  return ScriptPromise::CastUndefined(script_state);
}

void LockScreenData::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
