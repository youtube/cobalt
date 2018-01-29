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

#ifndef COBALT_DOM_MUTATION_OBSERVER_H_
#define COBALT_DOM_MUTATION_OBSERVER_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/dom/mutation_observer_init.h"
#include "cobalt/dom/mutation_record.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class MutationObserverTaskManager;
class Node;

// A MutationObserver object can be used to observe mutations to the tree of
// nodes.
// https://www.w3.org/TR/dom/#mutationobserver
class MutationObserver : public script::Wrappable {
 public:
  typedef script::Sequence<scoped_refptr<MutationRecord> >
      MutationRecordSequence;

  typedef script::CallbackFunction<void(const MutationRecordSequence&,
                                        const scoped_refptr<MutationObserver>&)>
      MutationCallback;
  typedef script::ScriptValue<MutationCallback> MutationCallbackArg;

  typedef base::Callback<void(const MutationRecordSequence&,
                              const scoped_refptr<MutationObserver>&)>
      NativeMutationCallback;

  // Not part of the spec. Support creating MutationObservers from native Cobalt
  // code.
  MutationObserver(const NativeMutationCallback& native_callback,
                   MutationObserverTaskManager* task_manager);

  // Web Api: MutationObserver
  MutationObserver(script::EnvironmentSettings* settings,
                   const MutationCallbackArg& callback);

  ~MutationObserver();

  void Observe(const scoped_refptr<Node>& target,
               const MutationObserverInit& options,
               script::ExceptionState* exception_state) {
    ObserveInternal(target, options, exception_state);
  }

  // Call this from native code. Will DCHECK on exception.
  void Observe(const scoped_refptr<Node>& target,
               const MutationObserverInit& options);

  void Disconnect();
  MutationRecordSequence TakeRecords();

  // Not part of the MutationObserver interface. Implements step (4.8) of the
  // "queue a mutation record" algorithm.
  // https://www.w3.org/TR/dom/#queue-a-mutation-record
  void QueueMutationRecord(const scoped_refptr<MutationRecord>& record);

  // Not part of the the MutationObserver interface. Implements steps (3) of the
  // "notify mutation observers" algorithm.
  // https://www.w3.org/TR/dom/#notify-mutation-observers
  bool Notify();

  // Internal helper class to allow creation of a MutationObserver with either a
  // native or script callback. Must be public so it can be inherited from in
  // the .cc file.
  class CallbackInternal;

  DEFINE_WRAPPABLE_TYPE(MutationObserver);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  void TrackObservedNode(const scoped_refptr<dom::Node>& node);

  void ObserveInternal(const scoped_refptr<Node>& target,
                       const MutationObserverInit& options,
                       script::ExceptionState* exception_state);

  scoped_ptr<CallbackInternal> callback_;
  typedef std::vector<base::WeakPtr<dom::Node> > WeakNodeVector;
  WeakNodeVector observed_nodes_;
  MutationRecordSequence record_queue_;
  MutationObserverTaskManager* task_manager_;
};
}  // namespace dom
}  // namespace cobalt
#endif  // COBALT_DOM_MUTATION_OBSERVER_H_
