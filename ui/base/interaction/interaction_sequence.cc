// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/interaction_sequence.h"

#include <vector>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_auto_reset.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"

namespace ui {

namespace {

// Runs |callback| if it is valid.
// We have a lot of callbacks that can be null, so calling through this method
// prevents accidentally trying to run a null callback.
template <typename Signature, typename... Args>
void RunIfValid(base::OnceCallback<Signature> callback, Args... args) {
  if (callback)
    std::move(callback).Run(args...);
}

// Insert an unused argument `Arg` in the front of the argument list for
// `callback`, and return the new callback with the dummy argument.
template <typename Arg, typename Ret, typename... Args>
base::OnceCallback<Ret(Arg, Args...)> PushUnusedArg(
    base::OnceCallback<Ret(Args...)> callback) {
  return base::BindOnce([](base::OnceCallback<Ret(Args...)> callback, Arg arg,
                           Args... args) { std::move(callback).Run(args...); },
                        std::move(callback));
}

// Insert two unused arguments `Arg1` and `Arg2` in the front of the argument
// list for `callback`, and return the new callback with the dummy arguments.
template <typename Arg1, typename Arg2, typename Ret, typename... Args>
base::OnceCallback<Ret(Arg1, Arg2, Args...)> PushUnusedArgs2(
    base::OnceCallback<Ret(Args...)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<Ret(Args...)> callback, Arg1 arg1, Arg2 arg2,
         Args... args) { std::move(callback).Run(args...); },
      std::move(callback));
}

// Sets step->must_remain_visible if it does not have a value.
void SetDefaultMustRemainVisibleValue(InteractionSequence::Step* step,
                                      const InteractionSequence::Step* next) {
  if (step->must_remain_visible.has_value())
    return;

  // Default for types other than kShown is false.
  if (step->type != InteractionSequence::StepType::kShown) {
    step->must_remain_visible = false;
    return;
  }

  // If the following step is to hide the same element, the default is false.
  if (next && next->type == InteractionSequence::StepType::kHidden &&
      (next->id == step->id || next->element_name == step->element_name)) {
    step->must_remain_visible = false;
    return;
  }

  // If the following step is a re-show of the same element or element ID, the
  // default is false.
  if (next && next->type == InteractionSequence::StepType::kShown &&
      next->id == step->id && next->transition_only_on_event) {
    step->must_remain_visible = false;
    return;
  }

  // Otherwise for kShown steps, the default is true.
  step->must_remain_visible = true;
}

}  // anonymous namespace

InteractionSequence::AbortedData::AbortedData() = default;
InteractionSequence::AbortedData::~AbortedData() = default;
InteractionSequence::AbortedData::AbortedData(const AbortedData&) = default;
InteractionSequence::AbortedData& InteractionSequence::AbortedData::operator=(
    const AbortedData&) = default;

struct InteractionSequence::SubsequenceData {
  SubsequenceData() = default;
  ~SubsequenceData() = default;
  SubsequenceData(SubsequenceData&& other) = default;
  SubsequenceData& operator=(SubsequenceData&& other) = default;

  Builder builder;
  SubsequenceCondition condition;
  std::unique_ptr<InteractionSequence> sequence;
  absl::optional<bool> result;
  AbortedData aborted_data;
};

InteractionSequence::Step::Step() = default;
InteractionSequence::Step::~Step() = default;

struct InteractionSequence::Configuration {
  Configuration() = default;
  ~Configuration() = default;

  std::list<std::unique_ptr<Step>> steps;
  ElementContext context;
  AbortedCallback aborted_callback;
  CompletedCallback completed_callback;
};

InteractionSequence::Builder::Builder()
    : configuration_(std::make_unique<Configuration>()) {}
InteractionSequence::Builder::Builder(Builder&& other) = default;
InteractionSequence::Builder& InteractionSequence::Builder::operator=(
    Builder&& other) = default;
InteractionSequence::Builder::~Builder() {
  DCHECK(!configuration_);
}

InteractionSequence::Builder& InteractionSequence::Builder::SetAbortedCallback(
    AbortedCallback callback) {
  DCHECK(!configuration_->aborted_callback);
  configuration_->aborted_callback = std::move(callback);
  return *this;
}

InteractionSequence::Builder&
InteractionSequence::Builder::SetCompletedCallback(CompletedCallback callback) {
  DCHECK(!configuration_->completed_callback);
  configuration_->completed_callback = std::move(callback);
  return *this;
}

InteractionSequence::Builder& InteractionSequence::Builder::AddStep(
    std::unique_ptr<Step> step) {
  // Do consistency checks and set up defaults.
  const bool is_custom_event_any_element =
      step->type == StepType::kCustomEvent && !step->id &&
      !step->uses_named_element();
  DCHECK(is_custom_event_any_element || step->type == StepType::kSubsequence ||
         !step->id == step->uses_named_element())
      << " A step of type " << step->type
      << " must set an identifier or a name, but not both.";
  DCHECK(configuration_->steps.empty() || !step->element)
      << " Only the initial step of a sequence may have a pre-set element.";
  DCHECK(!step->transition_only_on_event || !step->element)
      << " Pre-set element precludes transition_only_on_event.";
  DCHECK(step->context != StepContext(ContextMode::kAny) ||
         step->uses_named_element() || step->type == StepType::kShown)
      << " Currently, find in any context only supports StepType::kShown.";
  DCHECK_NE(step->type == StepType::kSubsequence,
            step->subsequence_data.empty());

  // Set reasonable defaults for must_be_visible based on step type and
  // parameters.
  if (step->uses_named_element() && step->type != StepType::kHidden) {
    DCHECK(!step->must_be_visible.has_value() || step->must_be_visible.value())
        << "Named elements not being hidden must be visible at step start.";
    step->must_be_visible = true;
  } else if (is_custom_event_any_element) {
    DCHECK(!step->must_be_visible.has_value() || !step->must_be_visible.value())
        << "A custom event with no element restrictions cannot specify that"
           " its element must start visible, as we will not know which element"
           " to refer to.";
    step->must_be_visible = false;
  } else {
    step->must_be_visible =
        step->must_be_visible.value_or(step->type == StepType::kActivated ||
                                       step->type == StepType::kCustomEvent);
  }

  DCHECK(!step->element || step->must_be_visible.value())
      << " Initial step with associated element must be visible from start.";
  DCHECK(step->type != InteractionSequence::StepType::kHidden ||
         !step->must_remain_visible.has_value() ||
         !step->must_remain_visible.value())
      << "Hide steps cannot specify that the element should remain visible.";
  DCHECK(step->type != InteractionSequence::StepType::kShown ||
         !step->uses_named_element() || !step->transition_only_on_event)
      << " kShown steps with transition_only_on_event are not compatible with"
         " named elements since a named element ceases to be valid when it"
         " becomes hidden.";
  if (auto* context = absl::get_if<ElementContext>(&step->context)) {
    DCHECK(*context) << "Explicit context must be valid.";
    if (!configuration_->context)
      configuration_->context = *context;
  }

  // Since the must_remain_visible value can be dependent on the following
  // step, we'll set it on the previous step, then set it on the final step
  // when we build the sequence.
  if (!configuration_->steps.empty()) {
    SetDefaultMustRemainVisibleValue(configuration_->steps.back().get(),
                                     step.get());
  }

  // Add the step.
  configuration_->steps.emplace_back(std::move(step));
  return *this;
}

InteractionSequence::Builder& InteractionSequence::Builder::AddStep(
    StepBuilder& step_builder) {
  return AddStep(step_builder.Build());
}

InteractionSequence::Builder& InteractionSequence::Builder::AddStep(
    StepBuilder&& step_builder) {
  return AddStep(step_builder.Build());
}

InteractionSequence::Builder& InteractionSequence::Builder::SetContext(
    ElementContext context) {
  configuration_->context = context;
  return *this;
}

std::unique_ptr<InteractionSequence> InteractionSequence::Builder::Build() {
  DCHECK(!configuration_->steps.empty());
  // Configure defaults for the final step.
  SetDefaultMustRemainVisibleValue(configuration_->steps.back().get(), nullptr);
  DCHECK(configuration_->context)
      << "If no view is provided, Builder::SetContext() must be called.";
  return base::WrapUnique(
      new InteractionSequence(std::move(configuration_), nullptr));
}

std::unique_ptr<InteractionSequence>
InteractionSequence::Builder::BuildSubsequence(const Step* reference_step) {
  DCHECK(!configuration_->steps.empty());
  // Configure defaults for the final step.
  SetDefaultMustRemainVisibleValue(configuration_->steps.back().get(), nullptr);
  DCHECK(configuration_->context)
      << "If no view is provided, Builder::SetContext() must be called.";
  return base::WrapUnique(
      new InteractionSequence(std::move(configuration_), reference_step));
}

InteractionSequence::StepBuilder::StepBuilder()
    : step_(std::make_unique<Step>()) {}
InteractionSequence::StepBuilder::StepBuilder(StepBuilder&& other) = default;
InteractionSequence::StepBuilder& InteractionSequence::StepBuilder::operator=(
    StepBuilder&& other) = default;
InteractionSequence::StepBuilder::~StepBuilder() = default;

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetElementID(ElementIdentifier element_id) {
  DCHECK(element_id);
  step_->id = element_id;
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetElementName(
    const base::StringPiece& name) {
  step_->element_name = std::string(name);
  step_->context = ContextMode::kAny;
  return *this;
}

InteractionSequence::StepBuilder& InteractionSequence::StepBuilder::SetContext(
    StepContext context) {
  DCHECK(context != StepContext(ElementContext()));
  DCHECK(!step_->uses_named_element());
  step_->context = context;
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetMustBeVisibleAtStart(
    bool must_be_visible) {
  step_->must_be_visible = must_be_visible;
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetMustRemainVisible(
    bool must_remain_visible) {
  step_->must_remain_visible = must_remain_visible;
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetTransitionOnlyOnEvent(
    bool transition_only_on_event) {
  step_->transition_only_on_event = transition_only_on_event;
  return *this;
}

InteractionSequence::StepBuilder& InteractionSequence::StepBuilder::SetType(
    StepType step_type,
    CustomElementEventType event_type) {
  DCHECK_EQ(step_type == StepType::kCustomEvent, static_cast<bool>(event_type))
      << "Custom events require an event type; event type may not be specified"
         " for other step types.";
  DCHECK_NE(StepType::kSubsequence, step_type)
      << "Create a subsequence step by calling SetSubsequenceMode() or"
         " AddSubsequence()";
  DCHECK(step_->subsequence_data.empty())
      << "Once AddSubsequence() has been called, step type cannot be changed.";
  step_->type = step_type;
  step_->custom_event_type = event_type;
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetSubsequenceMode(
    SubsequenceMode subsequence_mode) {
  step_->type = StepType::kSubsequence;
  step_->subsequence_mode = subsequence_mode;
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::AddSubsequence(
    Builder subsequence,
    SubsequenceCondition condition) {
  step_->type = StepType::kSubsequence;
  SubsequenceData data;
  data.builder = std::move(subsequence);
  data.condition = std::move(condition);
  step_->subsequence_data.emplace_back(std::move(data));
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetStartCallback(
    StepStartCallback start_callback) {
  step_->start_callback = std::move(start_callback);
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetStartCallback(
    base::OnceCallback<void(TrackedElement*)> start_callback) {
  step_->start_callback =
      PushUnusedArg<InteractionSequence*>(std::move(start_callback));
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetStartCallback(
    base::OnceClosure start_callback) {
  step_->start_callback =
      PushUnusedArgs2<InteractionSequence*, TrackedElement*>(
          std::move(start_callback));
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetEndCallback(StepEndCallback end_callback) {
  step_->end_callback = std::move(end_callback);
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetEndCallback(
    base::OnceClosure end_callback) {
  step_->end_callback = PushUnusedArg<TrackedElement*>(std::move(end_callback));
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::SetDescription(
    const base::StringPiece& description) {
  step_->description = std::string(description);
  return *this;
}

InteractionSequence::StepBuilder&
InteractionSequence::StepBuilder::FormatDescription(
    const base::StringPiece& format_string) {
  step_->description =
      base::StringPrintf(format_string.data(), step_->description.c_str());
  return *this;
}

std::unique_ptr<InteractionSequence::Step>
InteractionSequence::StepBuilder::Build() {
  return std::move(step_);
}

InteractionSequence::InteractionSequence(
    std::unique_ptr<Configuration> configuration,
    const Step* reference_step)
    : configuration_(std::move(configuration)) {
  TrackedElement* const first_element = next_step()->element;
  if (first_element) {
    DCHECK(first_element->identifier() == next_step()->id);
    DCHECK(first_element->context() == context());
    next_step()->subscription =
        ElementTracker::GetElementTracker()->AddElementHiddenCallback(
            first_element->identifier(), first_element->context(),
            base::BindRepeating(&InteractionSequence::OnElementHidden,
                                base::Unretained(this)));
  }
  if (next_step()->type == StepType::kSubsequence) {
    BuildSubsequences(reference_step);
  }
}

// static
InteractionSequence::SubsequenceCondition InteractionSequence::AlwaysRun() {
  return base::BindOnce(
      [](const InteractionSequence*, const TrackedElement*) { return true; });
}

// static
std::unique_ptr<InteractionSequence::Step>
InteractionSequence::WithInitialElement(TrackedElement* element,
                                        StepStartCallback start_callback,
                                        StepEndCallback end_callback) {
  StepBuilder step;
  step.step_->element = element;
  step.SetType(StepType::kShown)
      .SetElementID(element->identifier())
      .SetContext(element->context())
      .SetMustBeVisibleAtStart(true)
      .SetMustRemainVisible(true)
      .SetStartCallback(std::move(start_callback))
      .SetEndCallback(std::move(end_callback));
  return step.Build();
}

InteractionSequence::~InteractionSequence() {
  // We can abort during a step callback, but we cannot destroy this object.
  if (started_)
    Abort(AbortedReason::kSequenceDestroyed);
}

void InteractionSequence::Start() {
  // Ensure we're not already started.
  DCHECK(!started_);
  started_ = true;
  if (missing_first_element_) {
    Abort(AbortedReason::kElementHiddenBeforeSequenceStart);
    return;
  }
  StageNextStep();
}

void InteractionSequence::RunSynchronouslyForTesting() {
  base::RunLoop run_loop;
  quit_run_loop_closure_for_testing_ = run_loop.QuitClosure();
  Start();
  run_loop.Run();
}

void InteractionSequence::FailForTesting() {
  Abort(AbortedReason::kFailedForTesting);
}

void InteractionSequence::NameElement(TrackedElement* element,
                                      const base::StringPiece& name) {
  DCHECK(!name.empty());
  named_elements_[std::string(name)] = SafeElementReference(element);
  DCHECK(!current_step_ || current_step_->element_name != name);
  // When possible, preload ids for named elements so we can report a more
  // correct identifier on abort.
  for (const auto& step : configuration_->steps) {
    if (step->element_name == name) {
      step->id = element ? element->identifier() : ElementIdentifier();
      step->context =
          element ? StepContext(element->context()) : ContextMode::kAny;
    }
  }

  // If this is called during a step transition, we may want to watch for
  // activation or event on the element for the next step. (If the next step
  // doesn't refer to the element we just named, it will already have a
  // subscription and this call will be a no-op).
  MaybeWatchForEarlyTrigger(current_step_.get());
}

TrackedElement* InteractionSequence::GetNamedElement(
    const base::StringPiece& name) {
  const auto it = named_elements_.find(std::string(name));
  TrackedElement* result = nullptr;
  if (it != named_elements_.end()) {
    result = it->second.get();
  } else {
    NOTREACHED();
  }
  return result;
}

const TrackedElement* InteractionSequence::GetNamedElement(
    const base::StringPiece& name) const {
  return const_cast<InteractionSequence*>(this)->GetNamedElement(name);
}

void InteractionSequence::OnElementShown(TrackedElement* element) {
  // If the element was destroyed before we got our callback, this could be
  // null.
  if (!element)
    return;
  DCHECK_EQ(StepType::kShown, next_step()->type);
  DCHECK(element->identifier() == next_step()->id);
  // Note that we don't need to look for a named element here, as any named
  // element referenced in a kShown step must already exist, and therefore we
  // should have already transitioned or failed.
  DoStepTransition(element);
}

void InteractionSequence::OnElementActivated(TrackedElement* element) {
  // If the element was destroyed before we got our callback, this could be
  // null.
  if (!element)
    return;
  DCHECK_EQ(StepType::kActivated, next_step()->type);
  DCHECK(element->identifier() == next_step()->id);
  if (MatchesNameIfSpecified(element, next_step()->element_name))
    DoStepTransition(element);
}

void InteractionSequence::OnCustomEvent(TrackedElement* element) {
  DCHECK_EQ(StepType::kCustomEvent, next_step()->type);
  if (next_step()->id && next_step()->id != element->identifier())
    return;
  if (MatchesNameIfSpecified(element, next_step()->element_name))
    DoStepTransition(element);
}

void InteractionSequence::OnElementHidden(TrackedElement* element) {
  if (!started_) {
    DCHECK_EQ(next_step()->element, element);
    missing_first_element_ = true;
    next_step()->subscription = ElementTracker::Subscription();
    next_step()->element = nullptr;
    return;
  }

  if (current_step_ && current_step_->element == element) {
    // If the current step is marked as needing to remain visible and we haven't
    // seen the triggering event for the next step, abort.
    if (current_step_->must_remain_visible.value() &&
        !trigger_during_callback_) {
      Abort(AbortedReason::kElementHiddenDuringStep);
      return;
    }

    // This element pointer is no longer valid and we can stop watching.
    current_step_->subscription = ElementTracker::Subscription();
    current_step_->element = nullptr;
  }

  // If we got a hidden callback and it wasn't to abort the current step, it
  // must be because we're waiting on the next step to start.
  if (next_step() && next_step()->id == element->identifier() &&
      next_step()->type == StepType::kHidden) {
    if (next_step()->uses_named_element()) {
      // Find the named element; if it still exists, it hasn't been hidden.
      const auto it = named_elements_.find(next_step()->element_name);
      DCHECK(it != named_elements_.end());
      if (it->second.get())
        return;
    }

    // We can get this situation when an element goes away during a step
    // callback, before we've actually staged the following hide step. At this
    // point it's not valid to do a transition, so we'll mark that the next
    // transition has happened.
    if (processing_step_) {
      trigger_during_callback_ = true;
      next_step()->context = element->context();
    } else {
      DoStepTransition(element);
    }
  }
}

void InteractionSequence::OnTriggerDuringStepTransition(
    TrackedElement* element) {
  if (!next_step() || !element)
    return;

  switch (next_step()->type) {
    case StepType::kHidden:
    case StepType::kActivated:
    case StepType::kShown:
      // We should know the identifier and name ahead of time for activation
      // steps, so just make sure nothing has gone awry.
      DCHECK(element->identifier() == next_step()->id);
      DCHECK(MatchesNameIfSpecified(element, next_step()->element_name));
      break;
    case StepType::kCustomEvent:
      // Since we don't specify the element ID when registering for custom
      // events we have to see if we specified an ID or name and if so, whether
      // it matches the element we actually got.
      if (next_step()->id && element->identifier() != next_step()->id)
        return;
      if (!MatchesNameIfSpecified(element, next_step()->element_name))
        return;
      break;
    default:
      NOTREACHED();
      return;
  }

  // Barring disaster, we will immediately transition as soon as we finish
  // processing the current step.
  trigger_during_callback_ = true;
  next_step()->context = element->context();

  if (next_step()->type == StepType::kHidden) {
    next_step()->element = nullptr;
    next_step()->subscription = base::CallbackListSubscription();
  } else {
    // Since we've hit the trigger for the next step, we need to make sure we
    // clean up (and possibly abort) if the element goes away before we can
    // finish processing the current step.
    next_step()->element = element;
    next_step()->subscription =
        ElementTracker::GetElementTracker()->AddElementHiddenCallback(
            element->identifier(), element->context(),
            base::BindRepeating(
                &InteractionSequence::OnElementHiddenDuringStepTransition,
                base::Unretained(this)));
  }
}

void InteractionSequence::OnElementHiddenDuringStepTransition(
    TrackedElement* element) {
  if (!next_step() || element != next_step()->element)
    return;

  next_step()->element = nullptr;
  next_step()->subscription = ElementTracker::Subscription();
}

void InteractionSequence::OnElementHiddenWaitingForActivate(
    TrackedElement* element) {
  if (!next_step())
    return;

  if (next_step()->element == element ||
      !ElementTracker::GetElementTracker()->GetFirstMatchingElement(
          next_step()->id, context())) {
    Abort(AbortedReason::kElementNotVisibleAtStartOfStep);
  }
}

void InteractionSequence::MaybeWatchForEarlyTrigger(const Step* current_step) {
  // This should only be called while we're processing a step, there is a next
  // step we care about, and we aren't already subscribed for an event on that
  // step.
  if (!processing_step_ || configuration_->steps.empty() ||
      next_step()->subscription) {
    return;
  }

  // If the next element is named but we have not yet named it, don't add a
  // listener; we can add one when we name the element.
  ElementIdentifier id;
  ElementContext context;
  if (next_step()->uses_named_element()) {
    const auto it = named_elements_.find(next_step()->element_name);
    if (it == named_elements_.end() || !it->second.get())
      return;
    id = it->second.get()->identifier();
    context = it->second.get()->context();
  } else {
    id = next_step()->id;
    context = UpdateNextStepContext(current_step);
  }

  auto* const tracker = ElementTracker::GetElementTracker();

  // If the next step is a discrete event, listen for the event so we don't miss
  // it during the step callback.
  switch (next_step()->type) {
    case StepType::kActivated:
      // For activation events the ID of the next node must be known.
      if (id) {
        next_step()->subscription = tracker->AddElementActivatedCallback(
            id, context,
            base::BindRepeating(
                &InteractionSequence::OnTriggerDuringStepTransition,
                base::Unretained((this))));
      }
      break;
    case StepType::kCustomEvent:
      // For custom events the ID is not necessary because ElementTracker allows
      // just listening for the event.
      next_step()->subscription = tracker->AddCustomEventCallback(
          next_step()->custom_event_type, context,
          base::BindRepeating(
              &InteractionSequence::OnTriggerDuringStepTransition,
              base::Unretained((this))));
      break;
    case StepType::kShown:
      // For shown events, the ID must be known and the event need only be
      // observed if the state change itself is being observed or the element
      // might immediately become invisible again.
      if (id && (next_step()->transition_only_on_event ||
                 !next_step()->must_remain_visible.value())) {
        auto cb = base::BindRepeating(
            &InteractionSequence::OnTriggerDuringStepTransition,
            base::Unretained(this));
        next_step()->subscription =
            context ? tracker->AddElementShownCallback(id, context, cb)
                    : tracker->AddElementShownInAnyContextCallback(id, cb);
      }
      break;
    case StepType::kHidden:
      // For hidden events, the ID must be known. Only watch if the state change
      // itself is the step transition.
      if (id && next_step()->transition_only_on_event) {
        next_step()->subscription = tracker->AddElementHiddenCallback(
            id, context,
            base::BindRepeating(
                &InteractionSequence::OnTriggerDuringStepTransition,
                base::Unretained((this))));
      }
      break;
    case StepType::kSubsequence:
      // For subsequences, instead of watching for a particular state change,
      // the subsequences themselves are staged and prepped to run.
      BuildSubsequences(current_step);
      break;
  }
}

void InteractionSequence::DoStepTransition(TrackedElement* element) {
  // There are a number of callbacks during this method that could potentially
  // result in this InteractionSequence being destructed, so maintain a weak
  // pointer we can check to see if we need to bail out early.
  base::WeakPtr<InteractionSequence> delete_guard = weak_factory_.GetWeakPtr();
  auto* const tracker = ElementTracker::GetElementTracker();
  {
    // This block is non-re-entrant.
    DCHECK(!processing_step_);
    base::WeakAutoReset processing(weak_factory_.GetWeakPtr(),
                                   &InteractionSequence::processing_step_,
                                   true);

    // End the current step.
    if (current_step_) {
      // Unsubscribe from any events during the step-end process. Since the step
      // has ended, conditions like "must remain visible" no longer apply.
      current_step_->subscription = ElementTracker::Subscription();
      RunIfValid(std::move(current_step_->end_callback),
                 current_step_->element.get());
      if (!delete_guard || AbortedDuringCallback())
        return;
    }

    // Set up the new current step.
    current_step_ = std::move(configuration_->steps.front());
    configuration_->steps.pop_front();
    ++active_step_index_;
    DCHECK(!current_step_->element || current_step_->element == element);
    current_step_->element =
        current_step_->type == StepType::kHidden ? nullptr : element;
    if (element)
      current_step_->context = element->context();
    if (current_step_->element) {
      current_step_->subscription = tracker->AddElementHiddenCallback(
          current_step_->element->identifier(),
          current_step_->element->context(),
          base::BindRepeating(&InteractionSequence::OnElementHidden,
                              base::Unretained(this)));
    } else {
      current_step_->subscription = ElementTracker::Subscription();
    }

    // If we've got a guard on the new current step's element having gone away
    // while we were waiting, we can release it.
    next_step_hidden_subscription_ = ElementTracker::Subscription();

    // Special care must be taken here, because theoretically *anything* could
    // happen as a result of this callback. If the next step is a shown or
    // hidden step and the element becomes shown or hidden (or it's a step that
    // requires the element to be visible and it is not), then the appropriate
    // transition (or Abort()) will happen in StageNextStep() below.
    //
    // If, however, the callback *activates* or sends a custom event on the next
    // target element, and the next step is of the matching type, then the event
    // will not register unless we explicitly listen for it. This will add a
    // temporary callback to handle this case.
    MaybeWatchForEarlyTrigger(current_step_.get());

    // Start the step. Like all callbacks, this could abort the sequence, or
    // cause `element` to become invalid. Because of this we use the element
    // field of the current step from here forward, because we've installed a
    // callback above that will null it out if it becomes invalid.
    RunIfValid(std::move(current_step_->start_callback), this,
               current_step_->element.get());
    if (!delete_guard || AbortedDuringCallback())
      return;
  }

  if (configuration_->steps.empty()) {
    // Reset anything that might cause state change during the final callback.
    // After this, Abort() will have basically no effect, since by the time it
    // gets called, both the aborted and step end callbacks will be null.
    current_step_->subscription = ElementTracker::Subscription();
    configuration_->aborted_callback.Reset();
    // Last step end callback needs to be run before sequence completed.
    // Because the InteractionSequence could conceivably be destroyed during
    // one of these callbacks, make local copies of the callbacks and data.
    base::OnceClosure quit_closure =
        std::move(quit_run_loop_closure_for_testing_);
    CompletedCallback completed_callback =
        std::move(configuration_->completed_callback);
    std::unique_ptr<Step> last_step = std::move(current_step_);
    RunIfValid(std::move(last_step->end_callback), last_step->element.get());
    RunIfValid(std::move(completed_callback));
    RunIfValid(std::move(quit_closure));
    return;
  }

  // Since we're not done, load up the next step.
  StageNextStep();
}

void InteractionSequence::StageNextStep() {
  auto* const tracker = ElementTracker::GetElementTracker();
  Step* const next = next_step();

  // Note that if the target element for the next step was activated and then
  // hidden during the previous step transition, `next_element` could be null.
  TrackedElement* next_element;
  if (trigger_during_callback_ || next->element) {
    next_element = next->element;
  } else if (next->uses_named_element()) {
    next->element = GetNamedElement(next->element_name);
    next_element = next->element;
    // We should have set the ID on this step when the element was named; the
    // element may have gone away but shouldn't differ in ID from what we
    // previously recorded.
    DCHECK(!next_element || next->id == next_element->identifier());
  } else {
    ElementContext ctx = UpdateNextStepContext(current_step_.get());
    next_element =
        tracker->GetFirstMatchingElement(next->id, ctx ? ctx : context());
    if (!next_element && !ctx)
      next_element = tracker->GetElementInAnyContext(next->id);
  }

  if (!trigger_during_callback_ && next->must_be_visible.value() &&
      !next_element) {
    // We're going to abort, but we have to finish the current step first.
    if (current_step_) {
      RunIfValid(std::move(current_step_->end_callback),
                 current_step_->element.get());
    }
    Abort(AbortedReason::kElementNotVisibleAtStartOfStep);
    return;
  }

  // This context will either be where the next event will occur, or null if
  // this is a kShown step with ContextMode::kAny. By this point, only one of
  // those cases should be true (any other ContextMode would have been
  // overwritten).
  ElementContext context;
  if (auto* context_ptr = absl::get_if<ElementContext>(&next->context)) {
    context = *context_ptr;
  } else {
    DCHECK(StepContext(ContextMode::kAny) == next->context);
  }

  switch (next->type) {
    case StepType::kShown:
      if (trigger_during_callback_) {
        trigger_during_callback_ = false;
        if (next->must_remain_visible.value() && !next_element) {
          Abort(AbortedReason::kElementHiddenDuringStep);
          return;
        } else {
          DoStepTransition(next_element);
        }
      } else if (next_element && !next->transition_only_on_event) {
        DoStepTransition(next_element);
      } else {
        DCHECK(!next->uses_named_element());
        auto callback = base::BindRepeating(
            &InteractionSequence::OnElementShown, base::Unretained(this));
        next->subscription =
            context
                ? tracker->AddElementShownCallback(next->id, context, callback)
                : tracker->AddElementShownInAnyContextCallback(next->id,
                                                               callback);
      }
      break;
    case StepType::kHidden:
      if (trigger_during_callback_ ||
          (!next_element && !next->transition_only_on_event)) {
        trigger_during_callback_ = false;
        DoStepTransition(nullptr);
      } else {
        DCHECK(next_element || !next->uses_named_element());
        next->subscription = tracker->AddElementHiddenCallback(
            next->id, context,
            base::BindRepeating(&InteractionSequence::OnElementHidden,
                                base::Unretained(this)));
      }
      break;
    case StepType::kActivated:
      if (trigger_during_callback_) {
        trigger_during_callback_ = false;
        DoStepTransition(next_element);
      } else {
        DCHECK(next_element || !next->uses_named_element());
        next->subscription = tracker->AddElementActivatedCallback(
            next->id, context,
            base::BindRepeating(&InteractionSequence::OnElementActivated,
                                base::Unretained(this)));
        // It's possible to have the element hidden between the time we stage
        // the event and when the activation would actually come in (which
        // could be never). In this case, we should abort.
        if (next_step()->must_be_visible.value()) {
          next_step_hidden_subscription_ = tracker->AddElementHiddenCallback(
              next->id, context,
              base::BindRepeating(
                  &InteractionSequence::OnElementHiddenWaitingForActivate,
                  base::Unretained(this)));
        }
      }
      break;
    case StepType::kCustomEvent:
      if (trigger_during_callback_) {
        trigger_during_callback_ = false;
        DoStepTransition(next_element);
      } else {
        DCHECK(next_element || !next->uses_named_element());
        next->subscription = tracker->AddCustomEventCallback(
            next->custom_event_type, context,
            base::BindRepeating(&InteractionSequence::OnCustomEvent,
                                base::Unretained(this)));
        // It's possible to have the element hidden between the time we stage
        // the event and when the custom event would actually come in (which
        // could be never). In this case, we should abort.
        if (next_step()->must_be_visible.value()) {
          DCHECK(next->id);
          next_step_hidden_subscription_ = tracker->AddElementHiddenCallback(
              next->id, context,
              base::BindRepeating(
                  &InteractionSequence::OnElementHiddenWaitingForActivate,
                  base::Unretained(this)));
        }
      }
      break;
    case StepType::kSubsequence:
      next->element = next_element;
      if (!next_element &&
          (*next->must_be_visible || *next->must_remain_visible)) {
        Abort(AbortedReason::kElementNotVisibleAtStartOfStep);
        return;
      }
      const bool multiple =
          next->subsequence_mode == SubsequenceMode::kAll ||
          next->subsequence_mode == SubsequenceMode::kAtLeastOne;
      bool found = false;
      for (auto& subsequence_data : next->subsequence_data) {
        const bool enabled =
            std::move(subsequence_data.condition).Run(this, next_element);
        if (enabled && (multiple || !found)) {
          found = true;

          // Subsequence inherits named elements from parent.
          for (const auto& [name, element] : named_elements_) {
            if (element) {
              subsequence_data.sequence->NameElement(element.get(), name);
            }
          }

          // These need to be done asynchronously because theoretically one
          // might immediately step transition and go into a run loop, which
          // might prevent the others from starting.
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  [](base::WeakPtr<InteractionSequence> seq) {
                    if (seq) {
                      // Unblock the sequence from proceeding and start it.
                      seq->processing_step_ = false;
                      seq->Start();
                    }
                  },
                  subsequence_data.sequence->weak_factory_.GetWeakPtr()));
        } else {
          // This subsequence cannot run, so clear it out.
          subsequence_data.sequence.reset();
          subsequence_data.result = absl::nullopt;
        }
      }
      if (!found) {
        switch (next->subsequence_mode) {
          case SubsequenceMode::kAtLeastOne:
          case SubsequenceMode::kExactlyOne:
            Abort(AbortedReason::kNoSubsequenceRun);
            return;
          case SubsequenceMode::kAtMostOne:
          case SubsequenceMode::kAll:
            DoStepTransition(next_element);
            break;
        }
      } else if (next_element) {
        next->subscription =
            ElementTracker::GetElementTracker()->AddElementHiddenCallback(
                next_element->identifier(), next_element->context(),
                base::BindRepeating(&InteractionSequence::OnElementHidden,
                                    base::Unretained(this)));
      }
      break;
  }
}

void InteractionSequence::Abort(AbortedReason reason) {
  DCHECK(started_);
  next_step_hidden_subscription_ = ElementTracker::Subscription();
  // The entire InteractionSequence could also go away during a callback, so
  // save anything we need locally so that we don't have to access any class
  // members as we finish terminating the sequence.
  base::OnceClosure quit_closure =
      std::move(quit_run_loop_closure_for_testing_);
  std::unique_ptr<Step> current_step = std::move(current_step_);
  AbortedCallback aborted_callback =
      std::move(configuration_->aborted_callback);
  AbortedData aborted_data;
  aborted_data.step_index = active_step_index_;
  aborted_data.aborted_reason = reason;
  if (reason == AbortedReason::kElementNotVisibleAtStartOfStep ||
      reason == AbortedReason::kElementHiddenBeforeSequenceStart ||
      reason == AbortedReason::kSequenceDestroyed ||
      reason == AbortedReason::kNoSubsequenceRun ||
      reason == AbortedReason::kSubsequenceFailed) {
    ++aborted_data.step_index;
    if (next_step()) {
      aborted_data.step_type = next_step()->type;
      aborted_data.element_id = next_step()->id;
      aborted_data.step_description = next_step()->description;
      if (reason == AbortedReason::kSubsequenceFailed) {
        for (const auto& data : next_step()->subsequence_data) {
          aborted_data.subsequence_failures.emplace_back(
              data.result == false ? absl::make_optional(data.aborted_data)
                                   : absl::nullopt);
        }
      }
    }
  } else if (current_step) {
    aborted_data.step_type = current_step->type;
    aborted_data.element_id = current_step->id;
    aborted_data.element = SafeElementReference(current_step->element);
    aborted_data.step_description = current_step->description;
  }
  configuration_->steps.clear();

  // Note that if the sequence has already been aborted, this is a no-op, the
  // callbacks will already be null.
  if (current_step) {
    // Stop listening for events; we don't want additional callbacks during
    // teardown.
    current_step->subscription = ElementTracker::Subscription();
    RunIfValid(std::move(current_step->end_callback), current_step->element);
  }
  RunIfValid(std::move(aborted_callback), aborted_data);
  RunIfValid(std::move(quit_closure));
}

bool InteractionSequence::AbortedDuringCallback() const {
  // All step callbacks are sourced from the current step. If the current step
  // is null, then the sequence must have aborted (which clears out the current
  // step). Completion can only happen after step callbacks are finished
  if (current_step_)
    return false;

  DCHECK(configuration_->steps.empty());
  DCHECK(!configuration_->aborted_callback);
  return true;
}

bool InteractionSequence::MatchesNameIfSpecified(
    const TrackedElement* element,
    const base::StringPiece& name) const {
  if (name.empty())
    return true;

  const TrackedElement* const expected = GetNamedElement(name);
  DCHECK(expected);
  return element == expected;
}

ElementContext InteractionSequence::UpdateNextStepContext(
    const Step* current_step) {
  Step& next = *next_step();
  // A different mechanism is used to determine the context for named elements.
  CHECK(!next.uses_named_element());
  // If the context is already set, nothing needs to be done.
  if (auto* context = absl::get_if<ElementContext>(&next.context))
    return *context;
  switch (absl::get<ContextMode>(next.context)) {
    case ContextMode::kAny:
      // Any is a valid context already.
      return ElementContext();
    case ContextMode::kInitial:
      next.context = context();
      return context();
    case ContextMode::kFromPreviousStep: {
      ElementContext current_context = context();
      if (current_step) {
        const ElementContext* const temp =
            absl::get_if<ElementContext>(&current_step->context);
        DCHECK(temp)
            << "Previous step should always have a context set at this point.";
        if (temp)
          current_context = *temp;
      } else {
        NOTREACHED()
            << "Should not specify kFromPreviousStep without a previous step.";
      }
      next.context = current_context;
      return current_context;
    }
  }
}

InteractionSequence::SubsequenceData* InteractionSequence::FindSubsequenceData(
    SubsequenceHandle subsequence) {
  auto* next = next_step();
  if (!next || next->type != StepType::kSubsequence) {
    return nullptr;
  }
  for (auto& subsequence_data : next->subsequence_data) {
    if (&subsequence_data == subsequence) {
      return &subsequence_data;
    }
  }
  return nullptr;
}

void InteractionSequence::OnSubsequenceCompleted(
    SubsequenceHandle subsequence) {
  auto* const data = FindSubsequenceData(subsequence);
  if (!data) {
    return;
  }

  data->result = true;
  data->sequence.reset();
  switch (next_step()->subsequence_mode) {
    case SubsequenceMode::kExactlyOne:
    case SubsequenceMode::kAtMostOne:
    case SubsequenceMode::kAtLeastOne:
      DoStepTransition(next_step()->element);
      break;
    case SubsequenceMode::kAll:
      // Only transition if all enabled sequences are complete.
      bool still_running = false;
      for (const auto& subsequence_data : next_step()->subsequence_data) {
        still_running |= (subsequence_data.sequence != nullptr);
      }
      if (!still_running) {
        DoStepTransition(next_step()->element);
      }
      break;
  }
}

void InteractionSequence::OnSubsequenceAborted(
    SubsequenceHandle subsequence,
    const AbortedData& aborted_data) {
  auto* const data = FindSubsequenceData(subsequence);
  if (!data) {
    return;
  }

  data->result = false;
  data->aborted_data = aborted_data;
  data->sequence.reset();
  switch (next_step()->subsequence_mode) {
    case SubsequenceMode::kAll:
    case SubsequenceMode::kExactlyOne:
    case SubsequenceMode::kAtMostOne:
      Abort(AbortedReason::kSubsequenceFailed);
      break;
    case SubsequenceMode::kAtLeastOne:
      // Verify that at least one sequence has either succeeded or is still
      // running.
      bool still_alive = false;
      for (const auto& subsequence_data : next_step()->subsequence_data) {
        if (subsequence_data.result) {
          still_alive |= subsequence_data.result.value();
        } else {
          still_alive |= subsequence_data.sequence != nullptr;
        }
      }
      if (!still_alive) {
        Abort(AbortedReason::kSubsequenceFailed);
      }
      break;
  }
}

void InteractionSequence::BuildSubsequences(const Step* current_step) {
  CHECK(next_step() && next_step()->type == StepType::kSubsequence);
  for (auto& subsequence_data : next_step()->subsequence_data) {
    if (!subsequence_data.sequence) {
      subsequence_data.builder.SetContext(configuration_->context);
      subsequence_data.builder.SetCompletedCallback(base::BindOnce(
          &InteractionSequence::OnSubsequenceCompleted, base::Unretained(this),
          base::Unretained(&subsequence_data)));
      subsequence_data.builder.SetAbortedCallback(base::BindOnce(
          &InteractionSequence::OnSubsequenceAborted, base::Unretained(this),
          base::Unretained(&subsequence_data)));
      subsequence_data.sequence =
          subsequence_data.builder.BuildSubsequence(current_step);
      // This will prevent internal transitions until the current step
      // transition is done, while allowing watching for pre-trigger of the
      // initial step.
      subsequence_data.sequence->processing_step_ = true;
      subsequence_data.sequence->MaybeWatchForEarlyTrigger(current_step);
    }
  }
}

InteractionSequence::Step* InteractionSequence::next_step() {
  return configuration_->steps.empty() ? nullptr
                                       : configuration_->steps.front().get();
}

ElementContext InteractionSequence::context() const {
  return configuration_->context;
}

void PrintTo(InteractionSequence::StepType step_type, std::ostream* os) {
  static const char* const kStepTypeNames[] = {
      "kShown", "kActivated", "kHidden", "kCustomEvent", "kSubsequence"};
  constexpr int kCount = sizeof(kStepTypeNames) / sizeof(kStepTypeNames[0]);
  static_assert(kCount ==
                static_cast<int>(InteractionSequence::StepType::kMaxValue) + 1);
  const int value = static_cast<int>(step_type);
  *os << ((value < 0 || value >= kCount) ? "[invalid StepType]"
                                         : kStepTypeNames[value]);
}

void PrintTo(InteractionSequence::AbortedReason reason, std::ostream* os) {
  static const char* const kAbortedReasonNames[] = {
      "kSequenceDestroyed",
      "kElementHiddenBeforeSequenceStart",
      "kElementNotVisibleAtStartOfStep",
      "kElementHiddenDuringStep",
      "kNoSubsequenceRun",
      "kSubsequenceFailed",
      "kFailedForTesting"};
  constexpr int kCount =
      sizeof(kAbortedReasonNames) / sizeof(kAbortedReasonNames[0]);
  static_assert(
      kCount ==
      static_cast<int>(InteractionSequence::AbortedReason::kMaxValue) + 1);
  const int value = static_cast<int>(reason);
  *os << ((value < 0 || value >= kCount) ? "[invalid StepType]"
                                         : kAbortedReasonNames[value]);
}

void PrintTo(InteractionSequence::SubsequenceMode mode, std::ostream* os) {
  static const char* const kSubsequenceModeNames[] = {
      "kAtMostOne", "kExactlyOne", "kAtLeastOne", "kAll"};
  constexpr int kCount =
      sizeof(kSubsequenceModeNames) / sizeof(kSubsequenceModeNames[0]);
  static_assert(
      kCount ==
      static_cast<int>(InteractionSequence::SubsequenceMode::kMaxValue) + 1);
  const int value = static_cast<int>(mode);
  *os << ((value < 0 || value >= kCount) ? "[invalid SubsequenceMode]"
                                         : kSubsequenceModeNames[value]);
}

void PrintTo(const InteractionSequence::AbortedData& data, std::ostream* os) {
  *os << "on step " << data.step_index;
  if (!data.step_description.empty()) {
    *os << " (" << data.step_description << ")";
  }
  *os << " with reason " << data.aborted_reason << "; step type "
      << data.step_type << "; id " << data.element_id;
  if (data.element) {
    *os << "; element " << data.element.get();
  }
  if (data.aborted_reason ==
      InteractionSequence::AbortedReason::kSubsequenceFailed) {
    *os << "; subsequence failures:";
    size_t i = 0;
    for (auto& subsequence : data.subsequence_failures) {
      if (subsequence) {
        *os << " { subsequence " << i << " failed " << *subsequence << " }";
      }
      ++i;
    }
  }
}

extern std::ostream& operator<<(std::ostream& os,
                                InteractionSequence::StepType step_type) {
  PrintTo(step_type, &os);
  return os;
}

extern std::ostream& operator<<(std::ostream& os,
                                InteractionSequence::AbortedReason reason) {
  PrintTo(reason, &os);
  return os;
}

extern std::ostream& operator<<(std::ostream& os,
                                InteractionSequence::SubsequenceMode mode) {
  PrintTo(mode, &os);
  return os;
}

extern std::ostream& operator<<(std::ostream& os,
                                const InteractionSequence::AbortedData& data) {
  PrintTo(data, &os);
  return os;
}

}  // namespace ui
