/*
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/dom/mutation_observer.h"

namespace cobalt {
namespace dom {
// Abstract base class for a MutationCallback.
class MutationObserver::CallbackInternal {
 public:
  virtual void RunCallback(const MutationRecordSequence& mutations,
                           const scoped_refptr<MutationObserver>& observer) = 0;
  virtual ~CallbackInternal() {}
};

namespace {
// Implement the CallbackInternal interface for a JavaScript callback.
class ScriptCallback : public MutationObserver::CallbackInternal {
 public:
  ScriptCallback(const MutationObserver::MutationCallbackArg& callback,
                 MutationObserver* owner)
      : callback_(owner, callback) {}
  void RunCallback(const MutationObserver::MutationRecordSequence& mutations,
                   const scoped_refptr<MutationObserver>& observer) OVERRIDE {
    callback_.value().Run(mutations, observer);
  }

 private:
  MutationObserver::MutationCallbackArg::Reference callback_;
};

// Implement the CallbackInternal interface for a native callback.
class NativeCallback : public MutationObserver::CallbackInternal {
 public:
  explicit NativeCallback(
      const MutationObserver::NativeMutationCallback& callback)
      : callback_(callback) {}
  void RunCallback(const MutationObserver::MutationRecordSequence& mutations,
                   const scoped_refptr<MutationObserver>& observer) OVERRIDE {
    callback_.Run(mutations, observer);
  }

 private:
  MutationObserver::NativeMutationCallback callback_;
};
}  // namespace

MutationObserver::MutationObserver(
    const NativeMutationCallback& native_callback) {
  callback_.reset(new NativeCallback(native_callback));
}

MutationObserver::MutationObserver(const MutationCallbackArg& callback) {
  callback_.reset(new ScriptCallback(callback, this));
}

void MutationObserver::Observe(const scoped_refptr<Node>& target,
                               const MutationObserverInit& options) {
  UNREFERENCED_PARAMETER(target);
  UNREFERENCED_PARAMETER(options);
  NOTIMPLEMENTED();
}

void MutationObserver::Disconnect() { NOTIMPLEMENTED(); }

MutationObserver::MutationRecordSequence MutationObserver::TakeRecords() {
  NOTIMPLEMENTED();
  return MutationRecordSequence();
}

}  // namespace dom
}  // namespace cobalt
