// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/performance_observer.h"

#include <unordered_set>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/performance.h"
#include "cobalt/dom/performance_entry.h"
#include "cobalt/dom/window.h"

namespace cobalt {
namespace dom {

namespace {
// Implement the CallbackInternal interface for a JavaScript callback.
class ScriptCallback : public PerformanceObserver::CallbackInternal {
 public:
  ScriptCallback(const PerformanceObserver::PerformanceObserverCallbackArg& callback,
                 PerformanceObserver* observer)
      : callback_(observer, callback) {}
  bool RunCallback(const scoped_refptr<PerformanceObserverEntryList>& entries,
                   const scoped_refptr<PerformanceObserver>& observer) override {
    script::CallbackResult<void> result = callback_.value().Run(entries, observer);
    return !result.exception;
  }

 private:
  PerformanceObserver::PerformanceObserverCallbackArg::Reference callback_;
};

// Implement the CallbackInternal interface for a native callback.
class NativeCallback : public PerformanceObserver::CallbackInternal {
 public:
  explicit NativeCallback(
      const PerformanceObserver::NativePerformanceObserverCallback& callback)
      : callback_(callback) {}
  bool RunCallback(const scoped_refptr<PerformanceObserverEntryList>& entries,
                   const scoped_refptr<PerformanceObserver>& observer) override {
    callback_.Run(entries, observer);
    return true;
  }

 private:
  PerformanceObserver::NativePerformanceObserverCallback callback_;
};

}  // namespace

PerformanceObserver::PerformanceObserver(
    const NativePerformanceObserverCallback& native_callback,
    const scoped_refptr<Performance>& performance)
    : performance_(base::AsWeakPtr(performance.get())),
    observer_type_(PerformanceObserverType::kUndefined),
    is_registered_(false) {
  callback_.reset(new NativeCallback(native_callback));
}

PerformanceObserver::PerformanceObserver(
    script::EnvironmentSettings* env_settings,
    const PerformanceObserverCallbackArg& callback)
    : performance_(
          base::AsWeakPtr(base::polymorphic_downcast<DOMSettings*>(
          env_settings)
          ->window()
          ->performance().get())),
    observer_type_(PerformanceObserverType::kUndefined),
    is_registered_(false) {
  callback_.reset(new ScriptCallback(callback, this));
}

PerformanceObserver::~PerformanceObserver() {}

void PerformanceObserver::Observe(const PerformanceObserverInit& options,
    script::ExceptionState* exception_state) {
  // The algorithm for registering the observer.
  //   https://www.w3.org/TR/2019/WD-performance-timeline-2-20191024/#observe-method
  // 1.  Let observer be the context object.
  // 2.  Let relevantGlobal be observer's relevant global object.
  if (!performance_) {
    return;
  }
  // 3.  If options's entryTypes and type members are both omitted, then throw
  // a SyntaxError.
  bool has_entry_types = options.has_entry_types();
  bool has_type = options.has_type();
  if (!has_entry_types && !has_type) {
    DOMException::Raise(DOMException::kSyntaxErr, exception_state);
  }
  // 4.  If options's entryTypes is present and any other member is also
  // present, then throw a SyntaxError.
  bool entry_types_present = has_entry_types &&
     !options.entry_types().empty();
  bool type_present = has_type && !options.type().empty();
  if (entry_types_present && type_present) {
    DOMException::Raise(DOMException::kSyntaxErr, exception_state);
  }
  // 5.  Update or check observer's observer type by running these steps:
  // 5.1   If observer's observer type is "undefined":
  if (observer_type_ == PerformanceObserverType::kUndefined) {
    // 5.1.1  If options's entryTypes member is present, then set observer's
    // observer type to "multiple".
    if (entry_types_present) {
      observer_type_ = PerformanceObserverType::kMultiple;
    }
    // 5.1.2  If options's type member is present, then set observer's
    // observer type to "single".
    if (type_present) {
      observer_type_ = PerformanceObserverType::kSingle;
    }
  }
  // 5.2  If observer's observer type is "single" and options's entryTypes
  // member is present, then throw an InvalidModificationError.
  if (observer_type_ == PerformanceObserverType::kSingle &&
      entry_types_present) {
    DOMException::Raise(DOMException::kInvalidModificationErr, exception_state);
  }
  // 5.3  If observer's observer type is "multiple" and options's type member
  // is present, then throw an InvalidModificationError.
  if (observer_type_ == PerformanceObserverType::kMultiple &&
      type_present) {
    DOMException::Raise(DOMException::kInvalidModificationErr, exception_state);
  }
  // 6  If observer's observer type is "multiple", run the following steps:
  if (observer_type_ == PerformanceObserverType::kMultiple) {
    // 6.1  Let entry types be options's entryTypes sequence.
    script::Sequence<std::string> entry_types;
    // 6.2  Remove all types from entry types that are not contained in
    // relevantGlobal's frozen array of supported entry types. The user agent
    // SHOULD notify developers if entry types is modified. For example, a
    // console warning listing removed types might be appropriate.
    for (const auto& entry_type_string : options.entry_types()) {
      PerformanceEntryType entry_type =
          PerformanceEntry::ToEntryTypeEnum(entry_type_string);
      if (entry_type != PerformanceEntry::kInvalid) {
        entry_types.push_back(entry_type_string);
      } else {
        DLOG(WARNING) << "The entry type " + entry_type_string +
            " does not exist or isn't supported.";
      }
    }
    // 6.3  If the resulting entry types sequence is an empty sequence,
    // abort these steps. The user agent SHOULD notify developers when the
    // steps are aborted to notify that registration has been aborted.
    // For example, a console warning might be appropriate.
    if (entry_types.empty()) {
      DLOG(WARNING) << "An observe() call must include either entryTypes.";
      return;
    }
    // 6.4  If the list of registered performance observer objects of
    // relevantGlobal contains a registered performance observer whose
    // observer is the context object, replace its options list with a list
    // containing options as its only item.
    if (is_registered_) {
      performance_->ReplaceRegisteredPerformanceObserverOptionsList(
          base::WrapRefCounted(this), options);
    // 6.5  Otherwise, create and append a registered performance observer
    // object to the list of registered performance observer objects of
    // relevantGlobal, with observer set to the context object and options list
    // set to a list containing options as its only item.
    } else {
      performance_->RegisterPerformanceObserver(
          base::WrapRefCounted(this), options);
      is_registered_ = true;
    }
  // 7.  Otherwise, run the following steps:
  } else {
    // 7.1  Assert that observer's observer type is "single".
    DCHECK(observer_type_ == PerformanceObserverType::kSingle);
    // 7.2  If options's type is not contained in the relevantGlobal's frozen
    // array of supported entry types, abort these steps. The user agent SHOULD
    // notify developers when this happens, for instance via a console warning.
    PerformanceEntryType options_type =
          PerformanceEntry::ToEntryTypeEnum(options.type());
    if (options_type == PerformanceEntry::kInvalid) {
      DLOG(WARNING) << "The type " + options.type() +
            " does not exist or isn't supported.";
      return;
    }
    // 7.3  If the list of registered performance observer objects of
    // relevantGlobal contains a registered performance observer obs whose
    // observer is the context object:
    if (is_registered_) {
      // 7.3.1  If obs's options list contains a PerformanceObserverInit item
      // currentOptions whose type is equal to options's type, replace
      // currentOptions with options in obs's options list.
      // 7.3.2  Otherwise, append options to obs's options list.
      performance_->UpdateRegisteredPerformanceObserverOptionsList(
          base::WrapRefCounted(this), options);
    // 7.4  Otherwise, create and append a registered performance observer
    // object to the list of registered performance observer objects of
    // relevantGlobal, with observer set to the context object and options
    // list set to a list containing options as its only item.
    } else {
      performance_->RegisterPerformanceObserver(
          base::WrapRefCounted(this), options);
      is_registered_ = true;
    }
  }
}

void PerformanceObserver::Observe(script::ExceptionState* exception_state) {
  PerformanceObserverInit options;
  Observe(options, exception_state);
}

void PerformanceObserver::Disconnect() {
  // The disconnect() method must remove the context object from the list of
  // registered performance observer objects of relevant global object, and
  // also empty context object's observer buffer.
  //   https://www.w3.org/TR/2019/WD-performance-timeline-2-20191024/#disconnect-method
  if (performance_) {
    performance_->UnregisterPerformanceObserver(base::WrapRefCounted(this));
  }
  observer_buffer_.clear();
  is_registered_ = false;
}

PerformanceEntryList PerformanceObserver::TakeRecords() {
  // The takeRecords() method must return a copy of the context object's
  // observer buffer, and also empty context object's observer buffer.
  //   https://www.w3.org/TR/2019/WD-performance-timeline-2-20191024/#takerecords-method
  PerformanceEntryList performance_entries;
  performance_entries.swap(observer_buffer_);
  return performance_entries;
}

script::Sequence<std::string> PerformanceObserver::supported_entry_types() {
  // Each global object has an associated frozen array of supported entry
  // types, which is initialized to the FrozenArray created from the
  // sequence of strings among the registry that are supported for the
  // global object, in alphabetical order.
  //   https://www.w3.org/TR/2019/WD-performance-timeline-2-20191024/#supportedentrytypes-attribute
  // TODO: Implement the 'Time Entry Names Registry'.
  //   // https://w3c.github.io/timing-entrytypes-registry/#registry
  // When supportedEntryTypes's attribute getter is called, run the
  // following steps:
  // 1.  Let globalObject be the environment settings object's global object.
  script::Sequence<std::string> supported_entry_type_list;
  for (size_t i = 0; i < arraysize(PerformanceEntry::kEntryTypeString); ++i) {
    const char* entry_type = PerformanceEntry::kEntryTypeString[i];
    if (!base::LowerCaseEqualsASCII("invalid", entry_type)) {
      std::string entry_type_string(entry_type);
      supported_entry_type_list.push_back(entry_type_string);
    }
  }

  // 2.  Return globalObject's frozen array of supported entry types.
  return supported_entry_type_list;
}

void PerformanceObserver::EnqueuePerformanceEntry(
    const scoped_refptr<PerformanceEntry>& entry) {
  observer_buffer_.push_back(entry);
}

void PerformanceObserver::TraceMembers(script::Tracer* tracer) {
  tracer->TraceItems(observer_buffer_);
}

}  // namespace dom
}  // namespace cobalt
