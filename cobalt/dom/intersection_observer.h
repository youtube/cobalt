// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_INTERSECTION_OBSERVER_H_
#define COBALT_DOM_INTERSECTION_OBSERVER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/dom/intersection_observer_entry.h"
#include "cobalt/layout/intersection_observer_root.h"
#include "cobalt/layout/intersection_observer_target.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/union_type.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class Document;
class Element;
class IntersectionObserverInit;
class IntersectionObserverTaskManager;

// The IntersectionObserver can be used to observe changes in the intersection
// of an intersection root and one or more target Elements.
// https://www.w3.org/TR/intersection-observer/#intersection-observer-interfaces
class IntersectionObserver : public base::SupportsWeakPtr<IntersectionObserver>,
                             public script::Wrappable {
 public:
  typedef script::Sequence<scoped_refptr<IntersectionObserverEntry>>
      IntersectionObserverEntrySequence;

  typedef script::CallbackFunction<void(
      const IntersectionObserverEntrySequence&,
      const scoped_refptr<IntersectionObserver>&)>
      IntersectionObserverCallback;
  typedef script::ScriptValue<IntersectionObserverCallback>
      IntersectionObserverCallbackArg;

  typedef base::Callback<void(const IntersectionObserverEntrySequence&,
                              const scoped_refptr<IntersectionObserver>&)>
      NativeIntersectionObserverCallback;

  typedef script::UnionType2<double, script::Sequence<double>> ThresholdType;

  // Not part of the spec. Support creating IntersectionObservers from native
  // Cobalt code.
  IntersectionObserver(
      const scoped_refptr<Document>& document, cssom::CSSParser* css_parser,
      const NativeIntersectionObserverCallback& native_callback,
      script::ExceptionState* exception_state);

  IntersectionObserver(
      const scoped_refptr<Document>& document, cssom::CSSParser* css_parser,
      const NativeIntersectionObserverCallback& native_callback,
      const IntersectionObserverInit& options,
      script::ExceptionState* exception_state);

  // Web API: IntersectionObserver
  IntersectionObserver(script::EnvironmentSettings* settings,
                       const IntersectionObserverCallbackArg& callback,
                       script::ExceptionState* exception_state);
  IntersectionObserver(script::EnvironmentSettings* settings,
                       const IntersectionObserverCallbackArg& callback,
                       const IntersectionObserverInit& options,
                       script::ExceptionState* exception_state);
  ~IntersectionObserver();

  const base::WeakPtr<Element>& root() const { return root_; }
  void set_layout_root(
      const scoped_refptr<layout::IntersectionObserverRoot>& layout_root);
  const scoped_refptr<layout::IntersectionObserverRoot>& layout_root() const {
    return layout_root_;
  }
  // Part of the IntersectionObserver interface, but differs from the web spec
  // in that it returns the exact string passed into |options|, regardless of
  // whether it is formatted as four space-separated pixels lengths/percentages.
  std::string root_margin() const { return root_margin_; }
  // Not part of the IntersectionObserver interface. Returns the root margin
  // parsed as a PropertyListValue.
  const scoped_refptr<cssom::PropertyListValue>& root_margin_property_value()
      const {
    return root_margin_property_value_;
  }
  script::Sequence<double> thresholds() const;
  std::vector<double> thresholds_vector() const { return thresholds_; }
  void Observe(const scoped_refptr<Element>& target);
  void Unobserve(const scoped_refptr<Element>& target);
  void Disconnect();
  IntersectionObserverEntrySequence TakeRecords();

  // Not part of the IntersectionObserver interface. Implements the "queue an
  // IntersectionObserverEntry" algorithm.
  // https://www.w3.org/TR/intersection-observer/#queue-intersection-observer-entry-algo
  void QueueIntersectionObserverEntry(
      const scoped_refptr<IntersectionObserverEntry>& entry);

  // Not part of the the IntersectionObserver interface. Implements step (3) of
  // the "notify intersection observers" algorithm.
  // https://www.w3.org/TR/intersection-observer/#notify-intersection-observers-algo
  bool Notify();

  // Internal helper class to allow creation of a IntersectionObserver with a
  // script callback. Must be public so it can be inherited from in the .cc
  // file.
  class CallbackInternal;

  DEFINE_WRAPPABLE_TYPE(IntersectionObserver);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  IntersectionObserverTaskManager* task_manager();
  void InitNativeCallback(
      const NativeIntersectionObserverCallback& native_callback);
  void InitScriptCallback(const IntersectionObserverCallbackArg& callback);
  void InitIntersectionObserverInternal(
      const scoped_refptr<Document>& document, cssom::CSSParser* css_parser,
      const IntersectionObserverInit& options,
      script::ExceptionState* exception_state);
  void TrackObservationTarget(const scoped_refptr<Element>& target);
  void UntrackObservationTarget(const scoped_refptr<Element>& target);

  std::unique_ptr<CallbackInternal> callback_;
  base::WeakPtr<Element> root_;
  scoped_refptr<layout::IntersectionObserverRoot> layout_root_;
  std::string root_margin_;
  scoped_refptr<cssom::PropertyListValue> root_margin_property_value_;
  std::vector<double> thresholds_;
  typedef std::vector<base::WeakPtr<Element>> ElementVector;
  ElementVector observation_targets_;
  IntersectionObserverEntrySequence queued_entries_;
  scoped_refptr<IntersectionObserverTaskManager>
      intersection_observer_task_manager_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_INTERSECTION_OBSERVER_H_
