// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_INTERACTION_INTERACTIVE_TEST_H_
#define UI_BASE_INTERACTION_INTERACTIVE_TEST_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/rectify_callback.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/interaction/interactive_test_internal.h"

#if !BUILDFLAG(IS_IOS)
#include "ui/base/accelerators/accelerator.h"
#endif

namespace ui::test {

// Provides basic interactive test functionality.
//
// Interactive tests use InteractionSequence, ElementTracker, and
// InteractionTestUtil to provide a common library of concise test methods. This
// convenience API is nicknamed "Kombucha" (see
// //chrome/test/interaction/README.md for more information).
//
// This class is not a test fixture; it is a mixin that can be added to an
// existing test fixture using `InteractiveTestT<T>` - or just use
// `InteractiveTest`, which *is* a test fixture.
//
// Also, since this class does not implement input automation for any particular
// framework, you are more likely to want e.g. InteractiveViewsTest[Api] or
// InteractiveBrowserTest[Api], which inherit from this class.
class InteractiveTestApi {
 public:
  explicit InteractiveTestApi(
      std::unique_ptr<internal::InteractiveTestPrivate> private_test_impl);
  virtual ~InteractiveTestApi();
  InteractiveTestApi(const InteractiveTestApi&) = delete;
  void operator=(const InteractiveTestApi&) = delete;

 protected:
  using InputType = InteractionTestUtil::InputType;
  using MultiStep = internal::InteractiveTestPrivate::MultiStep;
  using StepBuilder = InteractionSequence::StepBuilder;
  using TextEntryMode = InteractionTestUtil::TextEntryMode;
  using OnIncompatibleAction =
      internal::InteractiveTestPrivate::OnIncompatibleAction;

  // Construct a MultiStep from one or more StepBuilders and/or MultiSteps.
  template <typename... Args>
  static MultiStep Steps(Args&&... args);

  // Returns an interaction simulator for things like clicking buttons.
  // Generally, prefer to use functions like PressButton() to directly using the
  // InteractionTestUtil.
  InteractionTestUtil& test_util() { return private_test_impl_->test_util(); }

  // Runs a test InteractionSequence in `context` from a series of Steps or
  // StepBuilders with RunSynchronouslyForTesting(). Hooks both the completed
  // and aborted callbacks to ensure completion, and prints an error on failure.
  template <typename... Args>
  bool RunTestSequenceInContext(ElementContext context, Args&&... steps);

  // An ElementSpecifier holds either an ElementIdentifier or a
  // base::StringPiece denoting a named element in the test sequence.
  using ElementSpecifier = internal::ElementSpecifier;

  // Convenience methods for creating interaction steps of type kShown. The
  // resulting step's start callback is already set; therefore, do not try to
  // add additional logic. However, any other parameter on the step may be set,
  // such as SetMustBeVisibleAtStart(), SetTransitionOnlyOnEvent(),
  // SetContext(), etc.
  //
  // TODO(dfried): in the future, these will be supplanted/supplemented by more
  // flexible primitives that allow multiple actions in the same step in the
  // future.
  [[nodiscard]] StepBuilder PressButton(
      ElementSpecifier button,
      InputType input_type = InputType::kDontCare);
  [[nodiscard]] StepBuilder SelectMenuItem(
      ElementSpecifier menu_item,
      InputType input_type = InputType::kDontCare);
  [[nodiscard]] StepBuilder DoDefaultAction(
      ElementSpecifier element,
      InputType input_type = InputType::kDontCare);
  [[nodiscard]] StepBuilder SelectTab(
      ElementSpecifier tab_collection,
      size_t tab_index,
      InputType input_type = InputType::kDontCare);
  [[nodiscard]] StepBuilder SelectDropdownItem(
      ElementSpecifier collection,
      size_t item,
      InputType input_type = InputType::kDontCare);
  [[nodiscard]] StepBuilder EnterText(
      ElementSpecifier element,
      std::u16string text,
      TextEntryMode mode = TextEntryMode::kReplaceAll);
  [[nodiscard]] StepBuilder ActivateSurface(ElementSpecifier element);
#if !BUILDFLAG(IS_IOS)
  [[nodiscard]] StepBuilder SendAccelerator(ElementSpecifier element,
                                            Accelerator accelerator);
#endif
  [[nodiscard]] StepBuilder Confirm(ElementSpecifier element);

  // Logs the given arguments, in order, at level INFO.
  //
  // This is *roughly* (but not exactly) equivalent to:
  //   `Do([=](){ LOG(INFO) << args...; })`
  //
  // By default, values are captured at the time the Log step is created, rather
  // than when it is run. If you want the value to be captured at runtime, pass
  // `std::ref(value)` instead:
  //
  // ```
  //   int x = 0;
  //   RunTestSequence(
  //       /* maybe change the value of x */
  //       Log("Value of x at sequence creation: ", x),
  //       Log("Value of x right now: ", std::ref(x)));
  // ```
  template <typename... Args>
  [[nodiscard]] static StepBuilder Log(Args... args);

  // Does an action at this point in the test sequence.
  template <typename A, typename = internal::RequireSignature<A, void()>>
  [[nodiscard]] static StepBuilder Do(A&& action);

  // Performs a check and fails the test if `check_callback` returns false.
  template <typename C, typename = internal::RequireSignature<C, bool()>>
  [[nodiscard]] static StepBuilder Check(C&& check_callback);

  // Calls `function` and applies `matcher` to the result. If the matcher does
  // not match, an appropriate error message is printed and the test fails.
  //
  // `matcher` should resolve of convert to a `Matcher<R>`.
  template <typename C,
            typename M,
            typename R = internal::ReturnTypeOf<C>,
            typename = internal::RequireSignature<C, R()>>
  [[nodiscard]] static StepBuilder CheckResult(C&& function, M&& matcher);

  // Checks that `check` returns true for element `element`. Will fail the test
  // sequence if `check` returns false - the callback should log any specific
  // error before returning.
  //
  // Note that unless you add .SetMustBeVisibleAtStart(true), this test step
  // will wait for `element` to be shown before proceeding.
  template <typename C,
            typename = internal::RequireSignature<C, bool(TrackedElement*)>>
  [[nodiscard]] static StepBuilder CheckElement(ElementSpecifier element,
                                                C&& check);

  // As CheckElement(), but checks that the result of calling `function` on
  // `element` matches `matcher`. If not, the mismatch is printed and the test
  // fails.
  //
  // `matcher` should resolve or convert to a `Matcher<R>`.
  template <typename F,
            typename M,
            typename R = internal::ReturnTypeOf<F>,
            typename = internal::RequireSignature<F, R(TrackedElement*)>>
  [[nodiscard]] static StepBuilder CheckElement(ElementSpecifier element,
                                                F&& function,
                                                M&& matcher);

  // Shorthand methods for working with basic ElementTracker events. The element
  // will have `step_callback` called on it. You may specify additional
  // constraints such as SetMustBeVisibleAtStart(), SetTransitionOnlyOnEvent(),
  // SetContext(), etc.
  //
  // `step_callback` arguments may be omitted from the left-hand side.
  template <class T,
            typename = internal::RequireCompatibleSignature<
                T,
                void(InteractionSequence*, TrackedElement*)>>
  [[nodiscard]] static StepBuilder AfterShow(ElementSpecifier element,
                                             T&& step_callback);
  template <class T,
            typename = internal::RequireCompatibleSignature<
                T,
                void(InteractionSequence*, TrackedElement*)>>
  [[nodiscard]] static StepBuilder AfterActivate(ElementSpecifier element,
                                                 T&& step_callback);
  template <class T,
            typename = internal::RequireCompatibleSignature<
                T,
                void(InteractionSequence*, TrackedElement*)>>
  [[nodiscard]] static StepBuilder AfterEvent(ElementSpecifier element,
                                              CustomElementEventType event_type,
                                              T&& step_callback);
  template <class T,
            typename = internal::RequireCompatibleSignature<
                T,
                void(InteractionSequence*, TrackedElement*)>>
  [[nodiscard]] static StepBuilder AfterHide(ElementSpecifier element,
                                             T&& step_callback);

  // Versions of the above that have no step callback; included for clarity and
  // brevity.
  [[nodiscard]] static StepBuilder WaitForShow(
      ElementSpecifier element,
      bool transition_only_on_event = false);
  [[nodiscard]] static StepBuilder WaitForHide(
      ElementSpecifier element,
      bool transition_only_on_event = false);
  [[nodiscard]] static StepBuilder WaitForActivate(ElementSpecifier element);
  [[nodiscard]] static StepBuilder WaitForEvent(ElementSpecifier element,
                                                CustomElementEventType event);

  // Equivalent to AfterShow() but the element must already be present.
  template <class T,
            typename = internal::RequireCompatibleSignature<
                T,
                void(InteractionSequence*, TrackedElement*)>>
  [[nodiscard]] static StepBuilder WithElement(ElementSpecifier element,
                                               T&& step_callback);

  // Adds steps to the sequence that ensure that `element_to_check` is not
  // present. Flushes the current message queue to ensure that if e.g. the
  // previous step was responding to elements being added, the
  // `element_to_check` may not have had its shown event called yet.
  [[nodiscard]] static MultiStep EnsureNotPresent(
      ElementIdentifier element_to_check,
      bool in_any_context = false);

  // Opposite of EnsureNotPresent. Flushes the current message queue and then
  // checks that the specified element is [still] present. Equivalent to:
  // ```
  //   FlushEvents(),
  //   WithElement(element_to_check, base::DoNothing())
  // ```
  //
  // Like EnsureNotPresent(), is not compatible with InAnyContext(); set
  // `in_any_context` to true instead. Otherwise, you can still wrap this call
  // in an InContext() or InSameContext().
  [[nodiscard]] static MultiStep EnsurePresent(
      ElementSpecifier element_to_check,
      bool in_any_context = false);

  // Ensures that the next step does not piggyback on the previous step(s), but
  // rather, executes on a fresh message loop. Normally, steps will continue to
  // trigger on the same call stack until a start condition is not met.
  //
  // Use sparingly, and only when e.g. re-entrancy issues prevent the test from
  // otherwise working properly.
  [[nodiscard]] static MultiStep FlushEvents();

  // Provides syntactic sugar so you can put "in any context" before an action
  // or test step rather than after. For example the following are equivalent:
  // ```
  //    PressButton(kElementIdentifier)
  //        .SetContext(InteractionSequence::ContextMode::kAny)
  //
  //    InAnyContext(PressButton(kElementIdentifier))
  // ```
  //
  // Note: does not work with EnsureNotPresent; use the `in_any_context`
  // parameter. Also does not work with all event types (yet).
  //
  // TODO(dfried): consider if we should have a version that takes variadic
  // arguments and applies "in any context" to all of them?
  [[nodiscard]] static MultiStep InAnyContext(MultiStep steps);
  template <typename T>
  [[nodiscard]] static StepBuilder InAnyContext(T&& step);

  // Provides syntactic sugar so you can put "inherit context from previous
  // step" around a step or steps to ensure a sequence executes in a specific
  // context. For example:
  // ```
  //    InAnyContext(WaitForShow(kMyElementInOtherContext)),
  //    InSameContext(Steps(
  //      PressButton(kMyElementInOtherContext),
  //      WaitForHide(kMyElementInOtherContext)
  //    )),
  // ```
  [[nodiscard]] static MultiStep InSameContext(MultiStep steps);
  template <typename T>
  [[nodiscard]] static StepBuilder InSameContext(T&& step);

  [[nodiscard]] MultiStep InContext(ElementContext context, MultiStep steps);
  template <typename T>
  [[nodiscard]] StepBuilder InContext(ElementContext context, T&& step);

  // Executes `then_steps` if `condition` is true, else executes `else_steps`.
  template <typename C,
            typename T,
            typename E = MultiStep,
            typename = internal::RequireSignature<C, bool()>>
  [[nodiscard]] static StepBuilder If(C&& condition,
                                      T&& then_steps,
                                      E&& else_steps = MultiStep());

  // Executes `then_steps` if the result of `function` matches `matcher`, which
  // should resolve or convert to a `Matcher<R>`. Arguments to `function` may be
  // omitted.
  template <typename F,
            typename M,
            typename T,
            typename E = MultiStep,
            typename R = internal::ReturnTypeOf<F>,
            typename = internal::
                RequireCompatibleSignature<F, R(const InteractionSequence*)>>
  [[nodiscard]] static StepBuilder IfMatches(F&& function,
                                             M&& matcher,
                                             T&& then_steps,
                                             E&& else_steps = MultiStep());

  // As If*(), but the `condition` receives a pointer to `element`. If the
  // element is not present, null is passed instead (the step does not wait for
  // the element to become visible). Arguments to `condition` may be omitted
  // from the left.
  template <typename C,
            typename T,
            typename E = MultiStep,
            typename = internal::RequireCompatibleSignature<
                C,
                bool(const InteractionSequence*, const TrackedElement*)>>
  [[nodiscard]] static StepBuilder IfElement(ElementSpecifier element,
                                             C&& condition,
                                             T&& then_steps,
                                             E&& else_steps = MultiStep());

  // As IfElement(), but the result of `function` is compared against `matcher`.
  //
  // Arguments to `function` may be omitted from the left. `matcher` should
  // resolve or convert to a `Matcher<R>`.
  template <typename F,
            typename M,
            typename T,
            typename E = MultiStep,
            typename R = internal::ReturnTypeOf<F>,
            typename = internal::RequireCompatibleSignature<
                F,
                R(const InteractionSequence*, const TrackedElement*)>>
  [[nodiscard]] static StepBuilder IfElementMatches(
      ElementSpecifier element,
      F&& function,
      M&& matcher,
      T&& then_steps,
      E&& else_steps = MultiStep());

  // Executes each of `sequences` in parallel, independently of each other, with
  // the expectation that all will succeed. Each sequence should be a step or
  // MultiStep.
  //
  // All of `sequences` must succeed or the test will fail.
  //
  // This is useful when you are waiting for several discrete events, but the
  // order they may occur in is unspecified/undefined, and there is no way to
  // wait for them in sequence in a way that won't occasionally flake due to the
  // race condition.
  //
  // Side-effects due to callbacks during these subsequences should be
  // minimized, as one sequence could theoretically interfere with the
  // functioning of another.
  template <typename... Args>
  [[nodiscard]] static StepBuilder InParallel(Args&&... sequences);

  // Executes each of `sequences` in parallel, independently of each other, with
  // the expectation that at least one will succeed. (The others will be
  // canceled.) Each sequence should be a step or MultiStep.
  //
  // At least one of `sequences` must succeed or the test will fail.
  //
  // Side-effects due to callbacks during these subsequences should be
  // minimized, as one sequence could theoretically interfere with the
  // functioning of another, and no one sequence is guaranteed to execute to
  // completion.
  template <typename... Args>
  [[nodiscard]] static StepBuilder AnyOf(Args&&... sequences);

  // Sets how to handle a case where a test attempts an operation that is not
  // supported in the current platform/build/environment. Default is to fail
  // the test. See chrome/test/interaction/README.md for best practices.
  //
  // Note that `reason` must always be specified, unless `action` is
  // `kFailTest`, in which case it may be empty.
  [[nodiscard]] StepBuilder SetOnIncompatibleAction(OnIncompatibleAction action,
                                                    const char* reason);

  // Used internally by methods in this class; do not call.
  internal::InteractiveTestPrivate& private_test_impl() {
    return *private_test_impl_;
  }

  // Adds a step or steps to the end of an existing MultiStep. Shorthand for
  // making one or more calls to `std::vector::emplace_back`.
  static void AddStep(MultiStep& dest, StepBuilder src);
  static void AddStep(MultiStep& dest, MultiStep src);

 private:
  // Implementation for RunTestSequenceInContext().
  bool RunTestSequenceImpl(ElementContext context,
                           InteractionSequence::Builder builder);

  // Helper method to add a step or steps to a sequence builder.
  static void AddStep(InteractionSequence::Builder& builder, MultiStep steps);
  template <typename T>
  static void AddStep(InteractionSequence::Builder& builder, T&& step);

  std::unique_ptr<internal::InteractiveTestPrivate> private_test_impl_;
};

// Template that adds InteractiveTestApi to any test fixture. No simulators are
// attached to test_util() so if you want to use verbs like PressButton() you
// will need to install your own simulator.
template <typename T>
class InteractiveTestT : public T, public InteractiveTestApi {
 public:
  template <typename... Args>
  explicit InteractiveTestT(Args&&... args)
      : T(std::forward<Args>(args)...),
        InteractiveTestApi(std::make_unique<internal::InteractiveTestPrivate>(
            std::make_unique<InteractionTestUtil>())) {}

  ~InteractiveTestT() override = default;

 protected:
  void SetUp() override {
    T::SetUp();
    private_test_impl().DoTestSetUp();
  }

  void TearDown() override {
    private_test_impl().DoTestTearDown();
    T::TearDown();
  }
};

// A simple test fixture that brings in all of the features of
// InteractiveTestApi. No simulators are attached to test_util() so if you want
// to use verbs like PressButton() you will need to install your own simulator.
//
// Provided for convenience, but generally you will want InteractiveViewsTest
// or InteractiveBrowserTest instead.
using InteractiveTest = InteractiveTestT<testing::Test>;

// Template definitions:

// static
template <typename... Args>
InteractiveTestApi::MultiStep InteractiveTestApi::Steps(Args&&... args) {
  MultiStep result;
  (AddStep(result, std::forward<Args>(args)), ...);
  return result;
}

// static
template <typename T>
void InteractiveTestApi::AddStep(InteractionSequence::Builder& builder,
                                 T&& step) {
  builder.AddStep(std::forward<T>(step));
}

template <typename... Args>
bool InteractiveTestApi::RunTestSequenceInContext(ElementContext context,
                                                  Args&&... steps) {
  // TODO(dfried): is there any additional automation we need to do in order to
  // get proper error scoping, RunLoop timeout handling, etc.? We may have to
  // inject information directly into the steps or step callbacks; it's unclear.
  InteractionSequence::Builder builder;
  (AddStep(builder, std::forward<Args>(steps)), ...);
  return RunTestSequenceImpl(context, std::move(builder));
}

template <typename A, typename>
// static
InteractiveTestApi::StepBuilder InteractiveTestApi::Do(A&& action) {
  StepBuilder builder;
  builder.SetDescription("Do()");
  builder.SetElementID(internal::kInteractiveTestPivotElementId);
  builder.SetStartCallback(
      base::OnceClosure(internal::MaybeBind(std::forward<A>(action))));
  return builder;
}

// static
template <class T, typename>
InteractionSequence::StepBuilder InteractiveTestApi::AfterShow(
    ElementSpecifier element,
    T&& step_callback) {
  StepBuilder builder;
  builder.SetDescription("AfterShow()");
  internal::SpecifyElement(builder, element);
  builder.SetStartCallback(
      base::RectifyCallback<InteractionSequence::StepStartCallback>(
          internal::MaybeBind(std::forward<T>(step_callback))));
  return builder;
}

// static
template <class T, typename>
InteractionSequence::StepBuilder InteractiveTestApi::AfterActivate(
    ElementSpecifier element,
    T&& step_callback) {
  StepBuilder builder;
  builder.SetDescription("AfterActivate()");
  internal::SpecifyElement(builder, element);
  builder.SetType(InteractionSequence::StepType::kActivated);
  builder.SetStartCallback(
      base::RectifyCallback<InteractionSequence::StepStartCallback>(
          internal::MaybeBind(std::forward<T>(step_callback))));
  return builder;
}

// static
template <class T, typename>
InteractionSequence::StepBuilder InteractiveTestApi::AfterEvent(
    ElementSpecifier element,
    CustomElementEventType event_type,
    T&& step_callback) {
  StepBuilder builder;
  builder.SetDescription(
      base::StrCat({"AfterEvent( ", event_type.GetName(), " )"}));
  internal::SpecifyElement(builder, element);
  builder.SetType(InteractionSequence::StepType::kCustomEvent, event_type);
  builder.SetStartCallback(
      base::RectifyCallback<InteractionSequence::StepStartCallback>(
          internal::MaybeBind(std::forward<T>(step_callback))));
  return builder;
}

// static
template <class T, typename>
InteractionSequence::StepBuilder InteractiveTestApi::AfterHide(
    ElementSpecifier element,
    T&& step_callback) {
  StepBuilder builder;
  builder.SetDescription("AfterHide()");
  internal::SpecifyElement(builder, element);
  builder.SetType(InteractionSequence::StepType::kHidden);
  builder.SetStartCallback(
      base::RectifyCallback<InteractionSequence::StepStartCallback>(
          internal::MaybeBind(std::forward<T>(step_callback))));
  return builder;
}

// static
template <class T, typename>
InteractionSequence::StepBuilder InteractiveTestApi::WithElement(
    ElementSpecifier element,
    T&& step_callback) {
  StepBuilder builder;
  builder.SetDescription("WithElement()");
  internal::SpecifyElement(builder, element);
  builder.SetStartCallback(
      base::RectifyCallback<InteractionSequence::StepStartCallback>(
          internal::MaybeBind(std::forward<T>(step_callback))));
  builder.SetMustBeVisibleAtStart(true);
  return builder;
}

// static
template <typename T>
InteractionSequence::StepBuilder InteractiveTestApi::InAnyContext(T&& step) {
  return std::move(step.SetContext(InteractionSequence::ContextMode::kAny)
                       .FormatDescription("InAnyContext( %s )"));
}

// static
template <typename T>
InteractionSequence::StepBuilder InteractiveTestApi::InSameContext(T&& step) {
  return std::move(
      step.SetContext(InteractionSequence::ContextMode::kFromPreviousStep)
          .FormatDescription("InSameContext( %s )"));
}

template <typename T>
InteractionSequence::StepBuilder InteractiveTestApi::InContext(
    ElementContext context,
    T&& step) {
  const auto fmt = base::StringPrintf("InContext( %p, %%s )",
                                      static_cast<const void*>(context));
  return std::move(step.SetContext(context).FormatDescription(fmt));
}

// static
template <typename C, typename T, typename E, typename>
InteractionSequence::StepBuilder InteractiveTestApi::IfElement(
    ElementSpecifier element,
    C&& condition,
    T&& then_steps,
    E&& else_steps) {
  auto step = IfElementMatches(element, std::forward<C>(condition), true,
                               std::forward<T>(then_steps),
                               std::forward<E>(else_steps));
  step.SetDescription("IfElement()");
  return step;
}

// static
template <typename F, typename M, typename T, typename E, typename R, typename>
InteractionSequence::StepBuilder InteractiveTestApi::IfElementMatches(
    ElementSpecifier element,
    F&& function,
    M&& matcher,
    T&& then_steps,
    E&& else_steps) {
  InteractionSequence::StepBuilder step;
  internal::SpecifyElement(step, element);
  using FunctionType =
      base::OnceCallback<R(const InteractionSequence*, const TrackedElement*)>;
  step.SetSubsequenceMode(InteractionSequence::SubsequenceMode::kAtMostOne);
  step.AddSubsequence(
      internal::BuildSubsequence(Steps(std::forward<T>(then_steps))),
      base::BindOnce(
          [](FunctionType function, testing::Matcher<R> matcher,
             const InteractionSequence* seq, const TrackedElement* el) -> bool {
            return matcher.Matches(std::move(function).Run(seq, el));
          },
          base::RectifyCallback<FunctionType>(
              internal::MaybeBind(std::forward<F>(function))),
          std::forward<M>(matcher)));
  auto temp = Steps(std::forward<E>(else_steps));
  if (!temp.empty()) {
    step.AddSubsequence(internal::BuildSubsequence(std::move(temp)));
  }
  step.SetDescription("IfElementMatches()");
  return step;
}

// static
template <typename C, typename T, typename E, typename>
InteractionSequence::StepBuilder InteractiveTestApi::If(C&& condition,
                                                        T&& then_steps,
                                                        E&& else_steps) {
  auto step =
      IfMatches(std::forward<C>(condition), true, std::forward<T>(then_steps),
                std::forward<E>(else_steps));
  step.SetDescription("If()");
  return step;
}

// static
template <typename F, typename M, typename T, typename E, typename R, typename>
InteractionSequence::StepBuilder InteractiveTestApi::IfMatches(F&& function,
                                                               M&& matcher,
                                                               T&& then_steps,
                                                               E&& else_steps) {
  auto step = IfElementMatches(
      internal::kInteractiveTestPivotElementId,
      base::BindOnce(
          [](base::OnceCallback<R(const InteractionSequence*)> function,
             const InteractionSequence* seq, const ui::TrackedElement*) {
            return std::move(function).Run(seq);
          },
          base::RectifyCallback<R(const InteractionSequence*)>(
              internal::MaybeBind(std::forward<F>(function)))),
      std::forward<M>(matcher), std::forward<T>(then_steps),
      std::forward<E>(else_steps));
  step.SetDescription("IfMatches()");
  return step;
}

// static
template <typename... Args>
InteractionSequence::StepBuilder InteractiveTestApi::InParallel(
    Args&&... sequences) {
  InteractionSequence::StepBuilder step;
  step.SetElementID(internal::kInteractiveTestPivotElementId);
  step.SetSubsequenceMode(InteractionSequence::SubsequenceMode::kAll);
  (step.AddSubsequence(
       internal::BuildSubsequence(Steps(std::forward<Args>(sequences)))),
   ...);
  step.SetDescription("InParallel()");
  return step;
}

// static
template <typename... Args>
InteractionSequence::StepBuilder InteractiveTestApi::AnyOf(
    Args&&... sequences) {
  InteractionSequence::StepBuilder step;
  step.SetElementID(internal::kInteractiveTestPivotElementId);
  step.SetSubsequenceMode(InteractionSequence::SubsequenceMode::kAtLeastOne);
  (step.AddSubsequence(
       internal::BuildSubsequence(Steps(std::forward<Args>(sequences)))),
   ...);
  step.SetDescription("AnyOf()");
  return step;
}

// static
template <typename... Args>
InteractiveTestApi::StepBuilder InteractiveTestApi::Log(Args... args) {
  auto step = Do(base::BindOnce(
      [](std::remove_cvref_t<Args>... args) {
        auto info = COMPACT_GOOGLE_LOG_INFO;
        ((info.stream() <<
          [&]() {
            if constexpr (internal::IsCallbackValue<Args>) {
              return std::move(args).Run();
            } else if constexpr (internal::IsFunctionPointerValue<Args>) {
              return (*args)();
            } else if constexpr (internal::IsCallableValue<Args>) {
              return args();
            } else {
              return args;
            }
          }()),
         ...);
      },
      std::move(args)...));
  step.SetDescription("Log()");
  return step;
}

// static
template <typename C, typename>
InteractiveTestApi::StepBuilder InteractiveTestApi::Check(C&& check_callback) {
  StepBuilder builder;
  builder.SetDescription("Check()");
  builder.SetElementID(internal::kInteractiveTestPivotElementId);
  builder.SetStartCallback(base::BindOnce(
      [](base::OnceCallback<bool()> check_callback, InteractionSequence* seq,
         TrackedElement*) {
        const bool result = std::move(check_callback).Run();
        if (!result) {
          seq->FailForTesting();
        }
      },
      internal::MaybeBind(std::forward<C>(check_callback))));
  return builder;
}

// static
template <typename C, typename M, typename R, typename>
InteractionSequence::StepBuilder InteractiveTestApi::CheckResult(C&& function,
                                                                 M&& matcher) {
  return std::move(Check(base::BindOnce(
                             [](base::OnceCallback<R()> function,
                                testing::Matcher<R> matcher) {
                               return internal::MatchAndExplain(
                                   "CheckResult()", matcher,
                                   std::move(function).Run());
                             },
                             internal::MaybeBind(std::forward<C>(function)),
                             testing::Matcher<R>(std::forward<M>(matcher))))
                       .SetDescription("CheckResult()"));
}

// static
template <typename C, typename>
InteractionSequence::StepBuilder InteractiveTestApi::CheckElement(
    ElementSpecifier element,
    C&& check) {
  return CheckElement(element, std::forward<C>(check), true);
}

// static
template <typename F, typename M, typename R, typename>
InteractionSequence::StepBuilder InteractiveTestApi::CheckElement(
    ElementSpecifier element,
    F&& function,
    M&& matcher) {
  StepBuilder builder;
  builder.SetDescription("CheckElement()");
  internal::SpecifyElement(builder, element);
  builder.SetStartCallback(base::BindOnce(
      [](base::OnceCallback<R(TrackedElement*)> function,
         testing::Matcher<R> matcher, InteractionSequence* seq,
         TrackedElement* el) {
        if (!internal::MatchAndExplain("CheckElement()", matcher,
                                       std::move(function).Run(el))) {
          seq->FailForTesting();
        }
      },
      internal::MaybeBind(std::forward<F>(function)),
      testing::Matcher<R>(std::forward<M>(matcher))));
  return builder;
}

}  // namespace ui::test

#endif  // UI_BASE_INTERACTION_INTERACTIVE_TEST_BASE_H_
