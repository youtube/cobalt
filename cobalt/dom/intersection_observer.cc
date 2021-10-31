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

#include <algorithm>
#include <string>

#include "cobalt/dom/intersection_observer.h"

#include "base/trace_event/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/source_location.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/intersection_observer_init.h"
#include "cobalt/dom/intersection_observer_task_manager.h"
#include "cobalt/dom/window.h"

namespace cobalt {
namespace dom {
// Abstract base class for a IntersectionObserverCallback.
class IntersectionObserver::CallbackInternal {
 public:
  virtual bool RunCallback(
      const IntersectionObserverEntrySequence& intersections,
      const scoped_refptr<IntersectionObserver>& observer) = 0;
  virtual ~CallbackInternal() {}
};

namespace {
// Implement the CallbackInternal interface for a JavaScript callback.
class ScriptCallback : public IntersectionObserver::CallbackInternal {
 public:
  ScriptCallback(
      const IntersectionObserver::IntersectionObserverCallbackArg& callback,
      IntersectionObserver* owner)
      : callback_(owner, callback) {}
  bool RunCallback(
      const IntersectionObserver::IntersectionObserverEntrySequence&
          intersections,
      const scoped_refptr<IntersectionObserver>& observer) override {
    script::CallbackResult<void> result =
        callback_.value().Run(intersections, observer);
    return !result.exception;
  }

 private:
  IntersectionObserver::IntersectionObserverCallbackArg::Reference callback_;
};

// Implement the CallbackInternal interface for a native callback.
class NativeCallback : public IntersectionObserver::CallbackInternal {
 public:
  explicit NativeCallback(
      const IntersectionObserver::NativeIntersectionObserverCallback& callback)
      : callback_(callback) {}
  bool RunCallback(
      const IntersectionObserver::IntersectionObserverEntrySequence&
          intersections,
      const scoped_refptr<IntersectionObserver>& observer) override {
    callback_.Run(intersections, observer);
    return true;
  }

 private:
  IntersectionObserver::NativeIntersectionObserverCallback callback_;
};

}  // namespace

IntersectionObserver::IntersectionObserver(
    const scoped_refptr<Document>& document, cssom::CSSParser* css_parser,
    const NativeIntersectionObserverCallback& native_callback,
    script::ExceptionState* exception_state)
    : IntersectionObserver(document, css_parser, native_callback,
                           IntersectionObserverInit(), exception_state) {}

IntersectionObserver::IntersectionObserver(
    const scoped_refptr<Document>& document, cssom::CSSParser* css_parser,
    const NativeIntersectionObserverCallback& native_callback,
    const IntersectionObserverInit& options,
    script::ExceptionState* exception_state) {
  InitNativeCallback(native_callback);
  InitIntersectionObserverInternal(document, css_parser, options,
                                   exception_state);
}

IntersectionObserver::IntersectionObserver(
    script::EnvironmentSettings* settings,
    const IntersectionObserverCallbackArg& callback,
    script::ExceptionState* exception_state)
    : IntersectionObserver(settings, callback, IntersectionObserverInit(),
                           exception_state) {}

IntersectionObserver::IntersectionObserver(
    script::EnvironmentSettings* settings,
    const IntersectionObserverCallbackArg& callback,
    const IntersectionObserverInit& options,
    script::ExceptionState* exception_state) {
  InitScriptCallback(callback);
  const scoped_refptr<Document>& document =
      base::polymorphic_downcast<dom::DOMSettings*>(settings)
          ->window()
          ->document();
  cssom::CSSParser* css_parser =
      base::polymorphic_downcast<dom::DOMSettings*>(settings)
          ->window()
          ->html_element_context()
          ->css_parser();
  InitIntersectionObserverInternal(document, css_parser, options,
                                   exception_state);
}

IntersectionObserver::~IntersectionObserver() {
  if (root_) {
    root_->UnregisterIntersectionObserverRoot(this);
  }
  if (intersection_observer_task_manager_) {
    intersection_observer_task_manager_->OnIntersectionObserverDestroyed(this);
  }
}

void IntersectionObserver::set_layout_root(
    const scoped_refptr<layout::IntersectionObserverRoot>& layout_root) {
  layout_root_ = layout_root;
}

script::Sequence<double> IntersectionObserver::thresholds() const {
  script::Sequence<double> result;

  for (std::vector<double>::const_iterator it = thresholds_.begin();
       it != thresholds_.end(); ++it) {
    result.push_back(*it);
  }
  return result;
}

void IntersectionObserver::Observe(const scoped_refptr<Element>& target) {
  if (!target) {
    // |target| is not nullable, so if this is NULL that indicates a bug in the
    // bindings layer.
    NOTREACHED();
    return;
  }
  target->RegisterIntersectionObserverTarget(this);
  TrackObservationTarget(target);
}

void IntersectionObserver::Unobserve(const scoped_refptr<Element>& target) {
  if (!target) {
    // |target| is not nullable, so if this is NULL that indicates a bug in the
    // bindings layer.
    NOTREACHED();
    return;
  }
  target->UnregisterIntersectionObserverTarget(this);
  UntrackObservationTarget(target);
}

void IntersectionObserver::Disconnect() {
  // For each target in this's internal [[ObservationTargets]] slot:
  // Remove the IntersectionObserverRegistration record whose observer property
  // is equal to this from target's internal [[RegisteredIntersectionObservers]]
  // slot. Remove target from this's internal [[ObservationTargets]] slot.
  for (ElementVector::iterator it = observation_targets_.begin();
       it != observation_targets_.end(); ++it) {
    Element* target = it->get();
    if (target != NULL) {
      target->UnregisterIntersectionObserverTarget(this);
    }
  }
  observation_targets_.clear();
  queued_entries_.clear();
}

IntersectionObserver::IntersectionObserverEntrySequence
IntersectionObserver::TakeRecords() {
  // The takeRecords() method must return a copy of the entry queue and then
  // empty the entry queue.
  IntersectionObserverEntrySequence queued_entries;
  queued_entries.swap(queued_entries_);
  return queued_entries;
}

void IntersectionObserver::QueueIntersectionObserverEntry(
    const scoped_refptr<IntersectionObserverEntry>& entry) {
  TRACE_EVENT0("cobalt::dom",
               "IntersectionObserver::QueueIntersectionObserverEntry()");
  queued_entries_.push_back(entry);
  if (intersection_observer_task_manager_) {
    intersection_observer_task_manager_->QueueIntersectionObserverTask();
  }
}

bool IntersectionObserver::Notify() {
  TRACE_EVENT0("cobalt::dom", "IntersectionObserver::Notify()");
  // https://www.w3.org/TR/intersection-observer/#notify-intersection-observers-algo
  // Step 3 of "notify intersection observers" steps:
  //     1. If observer's internal [[QueuedEntries]] slot is empty, continue.
  if (queued_entries_.empty()) {
    return true;
  }

  //     2. Let queue be a copy of observer's internal [[QueuedEntries]] slot.
  //     3. Clear observer's internal [[QueuedEntries]] slot.
  IntersectionObserverEntrySequence queue = TakeRecords();

  //     4. Invoke callback with queue as the first argument and observer as the
  //     second argument and callback this value. If this throws an exception,
  //     report the exception.
  return callback_->RunCallback(queue, base::WrapRefCounted(this));
}

void IntersectionObserver::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(root_);
  tracer->TraceItems(observation_targets_);
  tracer->TraceItems(queued_entries_);
}

void IntersectionObserver::InitNativeCallback(
    const NativeIntersectionObserverCallback& native_callback) {
  callback_.reset(new NativeCallback(native_callback));
}

void IntersectionObserver::InitScriptCallback(
    const IntersectionObserverCallbackArg& callback) {
  callback_.reset(new ScriptCallback(callback, this));
}

void IntersectionObserver::InitIntersectionObserverInternal(
    const scoped_refptr<Document>& document, cssom::CSSParser* css_parser,
    const IntersectionObserverInit& options,
    script::ExceptionState* exception_state) {
  // https://www.w3.org/TR/intersection-observer/#intersection-observer-interface
  // Set this.root to options.root. If not provided, set to the implicit root.
  if (options.root()) {
    root_ = base::AsWeakPtr(options.root().get());
  } else {
    root_ = base::AsWeakPtr(document->document_element().get());
  }

  // Attempt to parse a root margin from options.rootMargin. If a list is
  // returned, set this's internal [[rootMargin]] slot to that. Otherwise, throw
  // a SyntaxError exception.
  // https://www.w3.org/TR/intersection-observer/#parse-a-root-margin
  root_margin_ = options.root_margin();
  scoped_refptr<cssom::PropertyListValue> property_list_value =
      base::polymorphic_downcast<cssom::PropertyListValue*>(
          css_parser
              ->ParsePropertyValue(
                  "intersection-observer-root-margin", root_margin_,
                  base::SourceLocation("[object IntersectionObserver]", 1, 1))
              .get());
  if (property_list_value) {
    root_margin_property_value_ = property_list_value;
  } else {
    exception_state->SetSimpleException(
        script::kSyntaxError,
        "Not able to parse IntersectionObserver root margin.");
  }

  // Let thresholds be a list equal to options.threshold.
  // If any value in thresholds is less than 0.0 or greater than 1.0, throw a
  // RangeError exception.
  ThresholdType threshold = options.threshold();
  if (threshold.IsType<double>()) {
    if (threshold.AsType<double>() < 0 || threshold.AsType<double>() > 1) {
      exception_state->SetSimpleException(
          script::kRangeError,
          "IntersectionObserver threshold values must be between 0.0 and 1.0.");
      return;
    }
    thresholds_.push_back(threshold.AsType<double>());
  } else if (threshold.IsType<script::Sequence<double>>()) {
    script::Sequence<double> thresholds =
        threshold.AsType<script::Sequence<double>>();
    for (script::Sequence<double>::iterator it = thresholds.begin();
         it != thresholds.end(); ++it) {
      if (*it < 0 || *it > 1) {
        exception_state->SetSimpleException(script::kRangeError,
                                            "IntersectionObserver threshold "
                                            "values must be between 0.0 and "
                                            "1.0.");
        return;
      }
      thresholds_.push_back(*it);
    }
    // Sort thresholds in ascending order.
    sort(thresholds_.begin(), thresholds_.end());
  }
  // If thresholds is empty, append 0 to thresholds.
  if (thresholds_.empty()) {
    thresholds_.push_back(0);
  }

  root_->RegisterIntersectionObserverRoot(this);
  intersection_observer_task_manager_ =
      root_->owner_document()->intersection_observer_task_manager();
  intersection_observer_task_manager_->OnIntersectionObserverCreated(this);
}

void IntersectionObserver::TrackObservationTarget(
    const scoped_refptr<Element>& target) {
  for (ElementVector::iterator it = observation_targets_.begin();
       it != observation_targets_.end();) {
    if (it->get() == NULL) {
      it = observation_targets_.erase(it);
      continue;
    }
    if (*it == target) {
      return;
    }
    ++it;
  }
  observation_targets_.push_back(base::AsWeakPtr(target.get()));
}

void IntersectionObserver::UntrackObservationTarget(
    const scoped_refptr<Element>& target) {
  for (ElementVector::iterator it = observation_targets_.begin();
       it != observation_targets_.end();) {
    if (it->get() == NULL) {
      it = observation_targets_.erase(it);
      continue;
    }
    if (*it == target) {
      observation_targets_.erase(it);
      return;
    }
    ++it;
  }
}

}  // namespace dom
}  // namespace cobalt
