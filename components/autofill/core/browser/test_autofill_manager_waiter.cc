// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_manager_waiter.h"

#include <vector>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_run_loop_timeout.h"

namespace autofill {

TestAutofillManagerWaiter::State::State() = default;
TestAutofillManagerWaiter::State::~State() = default;

TestAutofillManagerWaiter::EventCount* TestAutofillManagerWaiter::State::Get(
    Event event) {
  auto it = events.find(event);
  return it != events.end() ? &it->second : nullptr;
}

TestAutofillManagerWaiter::EventCount&
TestAutofillManagerWaiter::State::GetOrCreate(Event event,
                                              base::Location location) {
  if (EventCount* e = Get(event)) {
    return *e;
  }
  EventCount& e = events[event];
  e = EventCount{.location = location};
  return e;
}

size_t TestAutofillManagerWaiter::State::num_pending_calls() const {
  size_t pending_calls = 0;
  for (const auto& [_, event_count] : events) {
    pending_calls += event_count.num_pending_calls;
  }
  return pending_calls;
}

size_t TestAutofillManagerWaiter::State::num_total_calls() const {
  size_t total_calls = 0;
  for (const auto& [_, event_count] : events) {
    total_calls += event_count.num_total_calls;
  }
  return total_calls;
}

std::string TestAutofillManagerWaiter::State::Describe() const {
  std::vector<std::string> strings;
  for (const auto& [_, event_count] : events) {
    strings.push_back(base::StringPrintf("[event=%s, pending=%zu, total=%zu]",
                                         event_count.location.function_name(),
                                         event_count.num_pending_calls,
                                         event_count.num_total_calls));
  }
  return base::JoinString(strings, ", ");
}

TestAutofillManagerWaiter::TestAutofillManagerWaiter(
    AutofillManager& manager,
    DenseSet<Event> relevant_events)
    : relevant_events_(relevant_events) {
  observation_.Observe(&manager);
}

TestAutofillManagerWaiter::~TestAutofillManagerWaiter() = default;

void TestAutofillManagerWaiter::OnAutofillManagerDestroyed(
    AutofillManager& manager) {
  observation_.Reset();
}

void TestAutofillManagerWaiter::OnAutofillManagerReset(
    AutofillManager& manager) {
  Reset();
}

void TestAutofillManagerWaiter::OnBeforeLanguageDetermined(
    AutofillManager& manager) {
  Increment(Event::kLanguageDetermined);
}

void TestAutofillManagerWaiter::OnAfterLanguageDetermined(
    AutofillManager& manager) {
  Decrement(Event::kLanguageDetermined);
}

void TestAutofillManagerWaiter::OnBeforeFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> forms) {
  Increment(Event::kFormsSeen);
}

void TestAutofillManagerWaiter::OnAfterFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> forms) {
  Decrement(Event::kFormsSeen);
}

void TestAutofillManagerWaiter::OnBeforeTextFieldDidChange(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  Increment(Event::kTextFieldDidChange);
}

void TestAutofillManagerWaiter::OnAfterTextFieldDidChange(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  Decrement(Event::kTextFieldDidChange);
}

void TestAutofillManagerWaiter::OnBeforeTextFieldDidScroll(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  Increment(Event::kTextFieldDidScroll);
}

void TestAutofillManagerWaiter::OnAfterTextFieldDidScroll(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  Decrement(Event::kTextFieldDidScroll);
}

void TestAutofillManagerWaiter::OnBeforeSelectControlDidChange(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  Increment(Event::kSelectControlDidChange);
}

void TestAutofillManagerWaiter::OnAfterSelectControlDidChange(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  Decrement(Event::kSelectControlDidChange);
}

void TestAutofillManagerWaiter::OnBeforeAskForValuesToFill(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field,
    const FormData& form_data) {
  Increment(Event::kAskForValuesToFill);
}

void TestAutofillManagerWaiter::OnAfterAskForValuesToFill(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  Decrement(Event::kAskForValuesToFill);
}

void TestAutofillManagerWaiter::OnBeforeDidFillAutofillFormData(
    AutofillManager& manager,
    FormGlobalId form) {
  Increment(Event::kDidFillAutofillFormData);
}

void TestAutofillManagerWaiter::OnAfterDidFillAutofillFormData(
    AutofillManager& manager,
    FormGlobalId form) {
  Decrement(Event::kDidFillAutofillFormData);
}

void TestAutofillManagerWaiter::OnBeforeJavaScriptChangedAutofilledValue(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  Increment(Event::kJavaScriptChangedAutofilledValue);
}

void TestAutofillManagerWaiter::OnAfterJavaScriptChangedAutofilledValue(
    AutofillManager& manager,
    FormGlobalId form,
    FieldGlobalId field) {
  Decrement(Event::kJavaScriptChangedAutofilledValue);
}

void TestAutofillManagerWaiter::OnFormSubmitted(AutofillManager& manager,
                                                FormGlobalId form) {
  Increment(Event::kFormSubmitted);
  Decrement(Event::kFormSubmitted);
}

void TestAutofillManagerWaiter::Reset() {
  // The declaration order ensures that `lock` is destroyed before `state`, so
  // that `state_->lock` has been released at its own destruction time.
  auto state = std::make_unique<State>();
  base::AutoLock lock(state_->lock);
  VLOG(1) << __func__;
  ASSERT_EQ(state_->num_pending_calls(), 0u) << state_->Describe();
  using std::swap;
  swap(state_, state);
}

bool TestAutofillManagerWaiter::IsRelevant(Event event) const {
  return relevant_events_.empty() || relevant_events_.contains(event);
}

void TestAutofillManagerWaiter::Increment(Event event,
                                          base::Location location) {
  base::AutoLock lock(state_->lock);
  if (!IsRelevant(event)) {
    VLOG(1) << "Ignoring irrelevant event: " << __func__ << "("
            << location.function_name() << ")";
    return;
  }
  if (state_->run_loop.AnyQuitCalled()) {
    VLOG(1) << "Ignoring event because no more calls are awaited: " << __func__
            << "(" << location.function_name() << ")";
    return;
  }
  VLOG(1) << __func__ << "(" << location.function_name() << ")";
  EventCount& e = state_->GetOrCreate(event, location);
  e.location = location;
  ++e.num_total_calls;
  ++e.num_pending_calls;
}

void TestAutofillManagerWaiter::Decrement(Event event,
                                          base::Location location) {
  base::AutoLock lock(state_->lock);
  if (!IsRelevant(event)) {
    VLOG(1) << "Ignoring irrelevant event: " << __func__ << "("
            << location.function_name() << ")";
    return;
  }
  if (state_->run_loop.AnyQuitCalled()) {
    VLOG(1) << "Ignoring event because no more calls are awaited: " << __func__
            << "(" << location.function_name() << ")";
    return;
  }
  VLOG(1) << __func__ << "(" << location.function_name() << ")";
  EventCount* e = state_->Get(event);
  ASSERT_TRUE(e) << state_->Describe();
  ASSERT_GT(e->num_pending_calls, 0u) << state_->Describe();
  if (state_->num_awaiting_total_calls > 0)
    --state_->num_awaiting_total_calls;
  --e->num_pending_calls;
  if (state_->num_pending_calls() == 0 && state_->num_awaiting_total_calls == 0)
    state_->run_loop.Quit();
}

testing::AssertionResult TestAutofillManagerWaiter::Wait(
    size_t num_awaiting_calls) {
  base::ReleasableAutoLock lock(&state_->lock);
  if (state_->run_loop.AnyQuitCalled()) {
    return testing::AssertionFailure()
           << "Waiter has not been Reset() since last Wait().";
  }
  // Some events may already have happened.
  num_awaiting_calls = num_awaiting_calls > state_->num_total_calls()
                           ? num_awaiting_calls - state_->num_total_calls()
                           : 0u;
  if (state_->num_pending_calls() > 0 || num_awaiting_calls > 0) {
    base::test::ScopedRunLoopTimeout run_loop_timeout(
        FROM_HERE, timeout_,
        base::BindRepeating(
            [](State* state) {
              state->timed_out = true;
              return state->Describe();
            },
            base::Unretained(state_.get())));
    state_->num_awaiting_total_calls = num_awaiting_calls;
    lock.Release();
    state_->run_loop.Run();
  }
  return !state_->timed_out ? testing::AssertionSuccess()
                            : testing::AssertionFailure() << "Waiter timed out";
}

const FormStructure* WaitForMatchingForm(
    AutofillManager* manager,
    base::RepeatingCallback<bool(const FormStructure&)> pred,
    base::TimeDelta timeout) {
  class Waiter : public AutofillManager::Observer {
   public:
    explicit Waiter(AutofillManager* manager,
                    base::RepeatingCallback<bool(const FormStructure&)> pred)
        : manager_(manager), pred_(std::move(pred)) {
      observation_.Observe(manager);
    }

    const FormStructure* Wait(base::TimeDelta timeout) {
      DCHECK(observation_.IsObserving());
      DCHECK(!matching_form_);
      matching_form_ = FindForm();
      if (!matching_form_) {
        base::test::ScopedRunLoopTimeout run_loop_timeout(
            FROM_HERE, timeout,
            base::BindRepeating(
                [](const Waiter* self) {
                  return std::string("Didn't see a matching form ") +
                         (self->observation_.IsObserving()
                              ? "within the timeout"
                              : "before the AutofillManager was reset or "
                                "destroyed");
                },
                base::Unretained(this)));
        run_loop_.Run();
      }
      return std::exchange(matching_form_, nullptr);
    }

   private:
    void OnAutofillManagerDestroyed(AutofillManager& manager) override {
      DCHECK_EQ(&manager, manager_.get());
      manager_ = nullptr;
      run_loop_.Quit();
      observation_.Reset();
    }

    void OnAutofillManagerReset(AutofillManager& manager) override {
      DCHECK_EQ(&manager, manager_.get());
      manager_ = nullptr;
      run_loop_.Quit();
      observation_.Reset();
    }

    void OnAfterFormsSeen(AutofillManager& manager,
                          base::span<const FormGlobalId> forms) override {
      DCHECK_EQ(&manager, manager_.get());
      if (const auto* form = FindForm()) {
        matching_form_ = form;
        run_loop_.Quit();
      }
    }

    FormStructure* FindForm() const {
      auto it = base::ranges::find_if(
          manager_->form_structures(),
          [&](const auto& p) { return pred_.Run(*p.second); });
      return it != manager_->form_structures().end() ? it->second.get()
                                                     : nullptr;
    }

    base::ScopedObservation<AutofillManager, AutofillManager::Observer>
        observation_{this};
    raw_ptr<AutofillManager> manager_;
    base::RepeatingCallback<bool(const FormStructure&)> pred_;
    base::RunLoop run_loop_;
    raw_ptr<const FormStructure> matching_form_ = nullptr;
  };
  return Waiter(manager, std::move(pred)).Wait(timeout);
}

}  // namespace autofill
