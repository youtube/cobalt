// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/dom/mutation_observer.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/mutation_observer_task_manager.h"
#include "cobalt/dom/node.h"
#include "cobalt/script/exception_message.h"

namespace cobalt {
namespace dom {
namespace {
// script::ExceptionState that will DCHECK on exception.
class NativeExceptionState : public script::ExceptionState {
 public:
  void SetException(const scoped_refptr<script::ScriptException>&) override {
    NOTREACHED();
  }

  void SetSimpleExceptionVA(script::SimpleExceptionType, const char*,
                            va_list) override {
    NOTREACHED();
  }
};
}  // namespace
// Abstract base class for a MutationCallback.
class MutationObserver::CallbackInternal {
 public:
  virtual bool RunCallback(const MutationRecordSequence& mutations,
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
  bool RunCallback(const MutationObserver::MutationRecordSequence& mutations,
                   const scoped_refptr<MutationObserver>& observer) override {
    script::CallbackResult<void> result =
        callback_.value().Run(mutations, observer);
    return !result.exception;
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
  bool RunCallback(const MutationObserver::MutationRecordSequence& mutations,
                   const scoped_refptr<MutationObserver>& observer) override {
    callback_.Run(mutations, observer);
    return true;
  }

 private:
  MutationObserver::NativeMutationCallback callback_;
};

}  // namespace

MutationObserver::MutationObserver(
    const NativeMutationCallback& native_callback,
    MutationObserverTaskManager* task_manager)
    : task_manager_(task_manager) {
  callback_.reset(new NativeCallback(native_callback));
  task_manager_->OnMutationObserverCreated(this);
}

MutationObserver::MutationObserver(script::EnvironmentSettings* settings,
                                   const MutationCallbackArg& callback)
    : task_manager_(base::polymorphic_downcast<DOMSettings*>(settings)
                        ->mutation_observer_task_manager()) {
  callback_.reset(new ScriptCallback(callback, this));
  task_manager_->OnMutationObserverCreated(this);
}

MutationObserver::~MutationObserver() {
  task_manager_->OnMutationObserverDestroyed(this);
}

void MutationObserver::Observe(const scoped_refptr<Node>& target,
                               const MutationObserverInit& options) {
  NativeExceptionState exception_state;
  ObserveInternal(target, options, &exception_state);
}

void MutationObserver::Disconnect() {
  // The disconnect() method must, for each node in the context object's
  // list of nodes, remove any registered observer on node for which the context
  // object is the observer, and also empty context object's record queue.
  for (WeakNodeVector::iterator it = observed_nodes_.begin();
       it != observed_nodes_.end(); ++it) {
    dom::Node* node = it->get();
    if (node != NULL) {
      node->UnregisterMutationObserver(make_scoped_refptr(this));
    }
  }
  observed_nodes_.clear();
  record_queue_.clear();
}

MutationObserver::MutationRecordSequence MutationObserver::TakeRecords() {
  // The takeRecords() method must return a copy of the record queue and then
  // empty the record queue.
  MutationRecordSequence record_queue;
  record_queue.swap(record_queue_);
  return record_queue;
}

void MutationObserver::QueueMutationRecord(
    const scoped_refptr<MutationRecord>& record) {
  record_queue_.push_back(record);
  task_manager_->QueueMutationObserverMicrotask();
}

bool MutationObserver::Notify() {
  // https://www.w3.org/TR/dom/#mutationobserver
  // Step 3 of "notify mutation observers" steps:
  //     1. Let queue be a copy of mo's record queue.
  //     2. Empty mo's record queue.
  MutationRecordSequence records = TakeRecords();

  //     3. Remove all transient registered observers whose observer is mo.
  // TODO: handle transient registered observers.

  //     4. If queue is non-empty, call mo's callback with queue as first
  //        argument, and mo (itself) as second argument and callback this
  //        value. If this throws an exception, report the exception.
  if (!records.empty()) {
    return callback_->RunCallback(records, make_scoped_refptr(this));
  }
  // If no records, return true to indicate no error occurred.
  return true;
}

void MutationObserver::TraceMembers(script::Tracer* tracer) {
  tracer->TraceItems(observed_nodes_);
  tracer->TraceSequence(record_queue_);
}

void MutationObserver::TrackObservedNode(const scoped_refptr<dom::Node>& node) {
  for (WeakNodeVector::iterator it = observed_nodes_.begin();
       it != observed_nodes_.end();) {
    if (*it == NULL) {
      it = observed_nodes_.erase(it);
      continue;
    }
    if (*it == node) {
      return;
    }
    ++it;
  }
  observed_nodes_.push_back(base::AsWeakPtr(node.get()));
}

void MutationObserver::ObserveInternal(
    const scoped_refptr<Node>& target, const MutationObserverInit& options,
    script::ExceptionState* exception_state) {
  if (!target) {
    // |target| is not nullable, so if this is NULL that indicates a bug in the
    // bindings layer.
    NOTREACHED();
    return;
  }
  if (!target->RegisterMutationObserver(make_scoped_refptr(this), options)) {
    // This fails if the options are invalid.
    exception_state->SetSimpleException(script::kTypeError, "Invalid options.");
    return;
  }
  TrackObservedNode(target);
}

}  // namespace dom
}  // namespace cobalt
