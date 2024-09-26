// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/test/assistant_ash_test_base.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/colors/assistant_colors.h"
#include "ash/assistant/ui/main_stage/assistant_onboarding_suggestion_view.h"
#include "ash/assistant/ui/main_stage/suggestion_chip_view.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event.h"
#include "ui/views/background.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using assistant::AssistantInteractionMetadata;
using assistant::AssistantInteractionType;

#define EXPECT_INTERACTION_OF_TYPE(type_)                      \
  ({                                                           \
    absl::optional<AssistantInteractionMetadata> interaction = \
        current_interaction();                                 \
    ASSERT_TRUE(interaction.has_value());                      \
    EXPECT_EQ(interaction->type, type_);                       \
  })

#define EXPECT_NO_INTERACTION()                                \
  ({                                                           \
    absl::optional<AssistantInteractionMetadata> interaction = \
        current_interaction();                                 \
    ASSERT_FALSE(interaction.has_value());                     \
  })

// Ensures that the given view has the focus. If it doesn't, this will print a
// nice error message indicating which view has the focus instead.
#define EXPECT_HAS_FOCUS(expected_)                                           \
  ({                                                                          \
    const views::View* actual = GetFocusedView();                             \
    EXPECT_TRUE(expected_->HasFocus())                                        \
        << "Expected focus on '" << expected_->GetClassName()                 \
        << "' but it is on '" << (actual ? actual->GetClassName() : "<null>") \
        << "'.";                                                              \
  })

// Ensures that the given view does not have the focus. If it does, this will
// print a nice error message.
#define EXPECT_NOT_HAS_FOCUS(expected_)                  \
  ({                                                     \
    EXPECT_FALSE(expected_->HasFocus())                  \
        << "'" << expected_->GetClassName()              \
        << "' should not have the focus (but it does)."; \
  })

views::View* AddTextfield(views::Widget* widget) {
  auto* result = widget->GetContentsView()->AddChildView(
      std::make_unique<views::Textfield>());
  // Give the text field a non-zero size, otherwise things like tapping on it
  // will fail.
  result->SetSize(gfx::Size(20, 10));
  // Focusable views need an accessible name to pass the accessibility paint
  // checks.
  result->SetAccessibleName(u"Name");

  return result;
}

// Stubbed |FocusChangeListener| that simply remembers all the views that
// received focus.
class FocusChangeListenerStub : public views::FocusChangeListener {
 public:
  explicit FocusChangeListenerStub(views::View* view)
      : focus_manager_(view->GetFocusManager()) {
    focus_manager_->AddFocusChangeListener(this);
  }

  FocusChangeListenerStub(const FocusChangeListenerStub&) = delete;
  FocusChangeListenerStub& operator=(const FocusChangeListenerStub&) = delete;

  ~FocusChangeListenerStub() override {
    focus_manager_->RemoveFocusChangeListener(this);
  }

  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override {}

  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override {
    focused_views_.push_back(focused_now);
  }

  // Returns all views that received the focus at some point.
  const std::vector<views::View*>& focused_views() const {
    return focused_views_;
  }

 private:
  std::vector<views::View*> focused_views_;
  raw_ptr<views::FocusManager, ExperimentalAsh> focus_manager_;
};

// |ViewObserver| that simply remembers whether the given view was drawn
// on the screen at least once during the lifetime of this observer.
// Note this checks |IsDrawn| and not |GetVisible| because visibility is a
// local property which does not take any of the ancestors into account, and
// we do not care if the |observed_view| is marked as visible but its parent
// is not visible.
class VisibilityObserver : public views::ViewObserver {
 public:
  explicit VisibilityObserver(views::View* observed_view)
      : observed_view_(observed_view) {
    observed_view_->AddObserver(this);

    UpdateWasDrawn();
  }
  ~VisibilityObserver() override { observed_view_->RemoveObserver(this); }

  void OnViewVisibilityChanged(views::View* view_or_ancestor,
                               views::View* starting_view) override {
    UpdateWasDrawn();
  }

  bool was_drawn() const { return was_drawn_; }

 private:
  void UpdateWasDrawn() {
    if (observed_view_->IsDrawn())
      was_drawn_ = true;
  }

  const raw_ptr<views::View, ExperimentalAsh> observed_view_;
  bool was_drawn_ = false;
};

// Base class for tests of the embedded assistant page in:
// - Legacy clamshell mode ("peeking launcher")
// - Clamshell mode ("bubble launcher")
// - Tablet mode
class AssistantPageViewTest : public AssistantAshTestBase {
 public:
  AssistantPageViewTest() = default;

  AssistantPageViewTest(const AssistantPageViewTest&) = delete;
  AssistantPageViewTest& operator=(const AssistantPageViewTest&) = delete;

  void ShowAssistantUiInTextMode() {
    ShowAssistantUi(AssistantEntryPoint::kUnspecified);
    EXPECT_TRUE(IsVisible());
  }

  void ShowAssistantUiInVoiceMode() {
    ShowAssistantUi(AssistantEntryPoint::kHotword);
    EXPECT_TRUE(IsVisible());
  }

  void PressKeyAndWait(ui::KeyboardCode key_code) {
    PressAndReleaseKey(key_code);
    base::RunLoop().RunUntilIdle();
  }

  ash::SuggestionChipView* CreateAndGetSuggestionChip(
      const std::string& chip_query) {
    MockTextInteraction().WithSuggestionChip(chip_query);
    auto suggestion_chips = GetSuggestionChips();
    DCHECK_EQ(suggestion_chips.size(), 1u);
    return suggestion_chips[0];
  }

  const views::View* GetFocusedView() {
    return page_view()->GetWidget()->GetFocusManager()->GetFocusedView();
  }

  // Ensures the onboarding views will not be shown.
  void DoNotShowOnboardingViews() {
    SetNumberOfSessionsWhereOnboardingShown(
        assistant::ui::kOnboardingMaxSessionsShown);
  }
};

// Counts the number of Assistant interactions that are started.
class AssistantInteractionCounter
    : private assistant::AssistantInteractionSubscriber {
 public:
  explicit AssistantInteractionCounter(assistant::Assistant* service) {
    interaction_observer_.Observe(service);
  }
  AssistantInteractionCounter(AssistantInteractionCounter&) = delete;
  AssistantInteractionCounter& operator=(AssistantInteractionCounter&) = delete;
  ~AssistantInteractionCounter() override = default;

  int interaction_count() const { return interaction_count_; }

 private:
  // AssistantInteractionSubscriber implementation:
  void OnInteractionStarted(const AssistantInteractionMetadata&) override {
    interaction_count_++;
  }

  int interaction_count_ = 0;
  assistant::ScopedAssistantInteractionSubscriber interaction_observer_{this};
};

TEST_F(AssistantPageViewTest, PressingAssistantKeyShowsAssistantPage) {
  ShowAssistantUi(AssistantEntryPoint::kHotkey);

  EXPECT_TRUE(Shell::Get()->app_list_controller()->IsVisible());
  EXPECT_TRUE(page_view()->GetVisible());
}

TEST_F(AssistantPageViewTest,
       OpeningLauncherThenPressingAssistantKeyShowsAssistantPage) {
  OpenLauncher();
  ShowAssistantUi(AssistantEntryPoint::kHotkey);

  EXPECT_TRUE(page_view()->GetVisible());
}

TEST_F(AssistantPageViewTest, PressingAssistantKeyTwiceClosesLauncher) {
  ShowAssistantUi(AssistantEntryPoint::kHotkey);
  CloseAssistantUi(AssistantExitPoint::kHotkey);

  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsVisible());
}

TEST_F(AssistantPageViewTest, ShouldFocusTextFieldWhenOpeningWithHotkey) {
  ShowAssistantUi(AssistantEntryPoint::kHotkey);

  EXPECT_HAS_FOCUS(input_text_field());
}

TEST_F(AssistantPageViewTest, ShouldNotLoseTextfieldFocusWhenSendingTextQuery) {
  ShowAssistantUi();

  SendQueryThroughTextField("The query");

  EXPECT_HAS_FOCUS(input_text_field());
}

TEST_F(AssistantPageViewTest,
       ShouldNotLoseTextfieldFocusWhenDisplayingResponse) {
  ShowAssistantUi();

  MockTextInteraction().WithTextResponse("The response");

  EXPECT_HAS_FOCUS(input_text_field());
}

TEST_F(AssistantPageViewTest, ShouldNotLoseTextfieldFocusWhenResizing) {
  ShowAssistantUi();

  MockTextInteraction().WithTextResponse(
      "This\ntext\nis\nbig\nenough\nto\ncause\nthe\nassistant\nscreen\nto\n"
      "resize.");

  EXPECT_HAS_FOCUS(input_text_field());
}

TEST_F(AssistantPageViewTest, FocusShouldRemainInAssistantViewWhenPressingTab) {
  constexpr int kMaxIterations = 100;
  ShowAssistantUi();

  const views::View* initial_focused_view = GetFocusedView();
  const views::View* focused_view;
  int num_views = 0;

  do {
    PressKeyAndWait(ui::VKEY_TAB);
    focused_view = GetFocusedView();
    EXPECT_TRUE(page_view()->Contains(focused_view))
        << "Focus advanced to view '" << focused_view->GetClassName()
        << "' which is not a part of the Assistant UI";

    // Validity check to ensure we do not loop forever
    num_views++;
    ASSERT_LT(num_views, kMaxIterations);
  } while (focused_view != initial_focused_view);
}

TEST_F(AssistantPageViewTest,
       FocusShouldCycleThroughOnboardingSuggestionsWhenPressingTab) {
  constexpr int kMaxIterations = 100;

  // Show Assistant UI and verify onboarding suggestions exist.
  ShowAssistantUi();
  auto onboarding_suggestions = GetOnboardingSuggestionViews();
  ASSERT_FALSE(onboarding_suggestions.empty());

  // Cache the first focused view.
  auto* first_focused_view = GetFocusedView();

  // Advance focus to the first onboarding suggestion.
  int num_iterations = 0;
  while (GetFocusedView() != onboarding_suggestions.at(0)) {
    PressKeyAndWait(ui::VKEY_TAB);
    ASSERT_LE(++num_iterations, kMaxIterations);  // Validity check.
  }

  // Verify we can cycle through them.
  for (auto* onboarding_suggestion : onboarding_suggestions) {
    ASSERT_EQ(GetFocusedView(), onboarding_suggestion);
    PressKeyAndWait(ui::VKEY_TAB);
  }

  // Confirm that we eventually get back to our first focused view.
  num_iterations = 0;
  while (GetFocusedView() != first_focused_view) {
    PressKeyAndWait(ui::VKEY_TAB);
    ASSERT_LE(++num_iterations, kMaxIterations);  // Validity check.
  }
}

TEST_F(AssistantPageViewTest, ShouldFocusMicWhenOpeningWithHotword) {
  ShowAssistantUi(AssistantEntryPoint::kHotword);

  EXPECT_HAS_FOCUS(mic_view());
}

TEST_F(AssistantPageViewTest, ShouldShowGreetingLabelWhenOpening) {
  DoNotShowOnboardingViews();

  ShowAssistantUi();

  EXPECT_TRUE(greeting_label()->IsDrawn());
  EXPECT_FALSE(onboarding_view()->IsDrawn());
}

TEST_F(AssistantPageViewTest, ShouldShowOnboardingWhenOpening) {
  ShowAssistantUi();

  EXPECT_TRUE(onboarding_view()->IsDrawn());
  EXPECT_FALSE(greeting_label()->IsDrawn());
}

TEST_F(AssistantPageViewTest, ShouldDismissGreetingLabelAfterQuery) {
  DoNotShowOnboardingViews();

  ShowAssistantUi();

  MockTextInteraction().WithTextResponse("The response");

  EXPECT_FALSE(greeting_label()->IsDrawn());
  EXPECT_FALSE(onboarding_view()->IsDrawn());
}

TEST_F(AssistantPageViewTest, ShouldDismissOnboardingAfterQuery) {
  ShowAssistantUi();

  MockTextInteraction().WithTextResponse("The response");

  EXPECT_FALSE(onboarding_view()->IsDrawn());
  EXPECT_FALSE(greeting_label()->IsDrawn());
}

TEST_F(AssistantPageViewTest, ShouldShowGreetingLabelAgainAfterReopening) {
  DoNotShowOnboardingViews();

  ShowAssistantUi();

  // Cause the label to be hidden.
  MockTextInteraction().WithTextResponse("The response");
  ASSERT_FALSE(greeting_label()->IsDrawn());

  // Close and reopen the Assistant UI.
  CloseAssistantUi();
  ShowAssistantUi();

  EXPECT_TRUE(greeting_label()->IsDrawn());
  EXPECT_FALSE(onboarding_view()->IsDrawn());
}

TEST_F(AssistantPageViewTest,
       ShouldNotShowGreetingLabelWhenOpeningFromSearchResult) {
  DoNotShowOnboardingViews();

  ShowAssistantUi(AssistantEntryPoint::kLauncherSearchResult);

  EXPECT_FALSE(greeting_label()->IsDrawn());
  EXPECT_FALSE(onboarding_view()->IsDrawn());
}

TEST_F(AssistantPageViewTest,
       ShouldNotShowOnboardingWhenOpeningFromSearchResult) {
  ShowAssistantUi(AssistantEntryPoint::kLauncherSearchResult);

  EXPECT_FALSE(onboarding_view()->IsDrawn());
  EXPECT_FALSE(greeting_label()->IsDrawn());
}

TEST_F(AssistantPageViewTest, ShouldShowOnboardingForNewUsers) {
  // A user is considered new if they haven't had an Assistant interaction in
  // the past 28 days.
  const base::Time new_user_cutoff = base::Time::Now() - base::Days(28);

  SetTimeOfLastInteraction(new_user_cutoff + base::Minutes(1));
  ShowAssistantUi();

  // This user *has* interacted with Assistant more recently than 28 days ago so
  // they are *not* considered new. Therefore, onboarding should *not* be shown.
  EXPECT_FALSE(onboarding_view()->IsDrawn());

  SetTimeOfLastInteraction(new_user_cutoff);

  CloseAssistantUi();
  ShowAssistantUi();

  // This user has *not* interacted with Assistant more recently than 28 days
  // ago so they *are* considered new. Therefore, onboarding *should* be shown.
  EXPECT_TRUE(onboarding_view()->IsDrawn());
}

TEST_F(AssistantPageViewTest, ShouldShowOnboardingUntilInteractionOccurs) {
  SetTimeOfLastInteraction(base::Time::Now() - base::Days(28));
  ShowAssistantUi();

  // This user has *not* interacted with Assistant more recently than 28 days
  // ago so they *are* considered new. Therefore, onboarding *should* be shown.
  EXPECT_TRUE(onboarding_view()->IsDrawn());

  CloseAssistantUi();
  ShowAssistantUi();

  // The user has *not* yet interacted with Assistant in this user session, so
  // we should continue to show onboarding.
  EXPECT_TRUE(onboarding_view()->IsDrawn());

  MockTextInteraction().WithQuery("Any Query").WithTextResponse("Any Response");

  CloseAssistantUi();
  ShowAssistantUi();

  // The user *has* had an interaction with Assistant in this user session, so
  // we should *not* show onboarding anymore.
  EXPECT_FALSE(onboarding_view()->IsDrawn());
}

TEST_F(AssistantPageViewTest,
       ShouldShowOnboardingToExistingUsersIfShownPreviouslyInDifferentSession) {
  SetTimeOfLastInteraction(base::Time::Now());
  SetNumberOfSessionsWhereOnboardingShown(1);

  ShowAssistantUi();

  // This user *has* interacted with Assistant more recently than 28 days ago so
  // so they are *not* considered new. Onboarding would not normally be shown
  // but, since it *was* shown in a previous user session, we *do* show it.
  EXPECT_TRUE(onboarding_view()->IsDrawn());

  MockTextInteraction().WithQuery("Any Query").WithTextResponse("Any Response");

  CloseAssistantUi();
  ShowAssistantUi();

  // But once the user has had an interaction with Assistant in this user
  // session, we still expect onboarding to no longer show.
  EXPECT_FALSE(onboarding_view()->IsDrawn());
}

TEST_F(AssistantPageViewTest,
       ShouldNotShowOnboardingToExistingUsersIfShownPreviouslyInMaxSessions) {
  SetTimeOfLastInteraction(base::Time::Now());
  SetNumberOfSessionsWhereOnboardingShown(
      assistant::ui::kOnboardingMaxSessionsShown);

  ShowAssistantUi();

  // This user has *not* interacted with Assistant more recently than 28 days
  // ago so they *are* considered new. Onboarding would normally be shown but,
  // since it was shown already in the max number of previous user sessions, we
  // do *not* show it.
  EXPECT_FALSE(onboarding_view()->IsDrawn());
}

TEST_F(AssistantPageViewTest, ShouldFocusMicViewWhenPressingVoiceInputToggle) {
  ShowAssistantUiInTextMode();

  ClickOnAndWait(voice_input_toggle());

  EXPECT_HAS_FOCUS(mic_view());
}

TEST_F(AssistantPageViewTest,
       ShouldStartVoiceInteractionWhenPressingVoiceInputToggle) {
  ShowAssistantUiInTextMode();

  ClickOnAndWait(voice_input_toggle());

  EXPECT_INTERACTION_OF_TYPE(AssistantInteractionType::kVoice);
}

TEST_F(AssistantPageViewTest,
       ShouldStopVoiceInteractionWhenPressingKeyboardInputToggle) {
  ShowAssistantUiInVoiceMode();
  EXPECT_INTERACTION_OF_TYPE(AssistantInteractionType::kVoice);

  ClickOnAndWait(keyboard_input_toggle());

  EXPECT_FALSE(current_interaction().has_value());
}

TEST_F(AssistantPageViewTest, ShouldShowOptInViewUnlessUserHasGivenConsent) {
  ShowAssistantUi();
  const views::View* suggestion_chips = suggestion_chip_container();
  const views::View* opt_in = opt_in_view();

  SetConsentStatus(ConsentStatus::kUnauthorized);
  EXPECT_TRUE(opt_in->IsDrawn());
  EXPECT_FALSE(suggestion_chips->IsDrawn());

  SetConsentStatus(ConsentStatus::kNotFound);
  EXPECT_TRUE(opt_in->IsDrawn());
  EXPECT_FALSE(suggestion_chips->IsDrawn());

  SetConsentStatus(ConsentStatus::kUnknown);
  EXPECT_TRUE(opt_in->IsDrawn());
  EXPECT_FALSE(suggestion_chips->IsDrawn());

  SetConsentStatus(ConsentStatus::kActivityControlAccepted);
  EXPECT_FALSE(opt_in->IsDrawn());
  EXPECT_TRUE(suggestion_chips->IsDrawn());
}

TEST_F(AssistantPageViewTest, ShouldSubmitQueryWhenClickingOnSuggestionChip) {
  ShowAssistantUi();
  ash::SuggestionChipView* suggestion_chip =
      CreateAndGetSuggestionChip("<suggestion chip query>");

  ClickOnAndWait(suggestion_chip);

  EXPECT_INTERACTION_OF_TYPE(AssistantInteractionType::kText);
  EXPECT_EQ("<suggestion chip query>", current_interaction()->query);
}

TEST_F(AssistantPageViewTest,
       ShouldSubmitQueryWhenPressingEnterOnSuggestionChip) {
  ShowAssistantUi();
  ash::SuggestionChipView* suggestion_chip =
      CreateAndGetSuggestionChip("<suggestion chip query>");

  suggestion_chip->RequestFocus();
  PressKeyAndWait(ui::VKEY_RETURN);

  EXPECT_INTERACTION_OF_TYPE(AssistantInteractionType::kText);
  EXPECT_EQ("<suggestion chip query>", current_interaction()->query);
}

TEST_F(AssistantPageViewTest,
       ShouldNotSubmitQueryWhenPressingSpaceOnSuggestionChip) {
  ShowAssistantUi();
  ash::SuggestionChipView* suggestion_chip =
      CreateAndGetSuggestionChip("<suggestion chip query>");

  suggestion_chip->RequestFocus();
  PressKeyAndWait(ui::VKEY_SPACE);

  EXPECT_NO_INTERACTION();
}

TEST_F(AssistantPageViewTest,
       ShouldOnlySubmitOneQueryWhenClickingSuggestionChipMultipleTimes) {
  ShowAssistantUi();
  ash::SuggestionChipView* suggestion_chip =
      CreateAndGetSuggestionChip("<suggestion chip query>");

  AssistantInteractionCounter counter{assistant_service()};
  ClickOnAndWait(suggestion_chip, /*check_if_view_can_process_events=*/false);
  ClickOnAndWait(suggestion_chip, /*check_if_view_can_process_events=*/false);
  ClickOnAndWait(suggestion_chip, /*check_if_view_can_process_events=*/false);
  ClickOnAndWait(suggestion_chip, /*check_if_view_can_process_events=*/false);

  EXPECT_EQ(1, counter.interaction_count());
}

TEST_F(AssistantPageViewTest,
       ShouldOnlySubmitQueryFromFirstSuggestionChipClickedOn) {
  ShowAssistantUi();
  MockTextInteraction()
      .WithSuggestionChip("<first query>")
      .WithSuggestionChip("<second query>")
      .WithSuggestionChip("<third query>");
  auto suggestion_chips = GetSuggestionChips();

  AssistantInteractionCounter counter{assistant_service()};
  ClickOnAndWait(suggestion_chips[0]);
  // All next clicks should be no-ops.
  ClickOnAndWait(suggestion_chips[0],
                 /*check_if_view_can_process_events=*/false);
  ClickOnAndWait(suggestion_chips[1],
                 /*check_if_view_can_process_events=*/false);
  ClickOnAndWait(suggestion_chips[2],
                 /*check_if_view_can_process_events=*/false);

  EXPECT_EQ(1, counter.interaction_count());
  EXPECT_EQ("<first query>", current_interaction()->query);
}

TEST_F(AssistantPageViewTest,
       SuggestionChipsShouldNotBeFocusableAfterSubmittingQuery) {
  ShowAssistantUi();
  MockTextInteraction()
      .WithSuggestionChip("<first query>")
      .WithSuggestionChip("<second query>")
      .WithSuggestionChip("<third query>");
  auto suggestion_chips = GetSuggestionChips();

  suggestion_chips[0]->RequestFocus();
  PressKeyAndWait(ui::VKEY_RETURN);

  for (auto* suggestion_chip : suggestion_chips) {
    EXPECT_FALSE(suggestion_chip->IsFocusable())
        << "Suggestion chip '" << suggestion_chip->GetText()
        << "' is still focusable";
  }
}

TEST_F(AssistantPageViewTest,
       ShouldFocusTextFieldWhenSubmittingSuggestionChipInTextMode) {
  ShowAssistantUiInTextMode();
  ash::SuggestionChipView* suggestion_chip =
      CreateAndGetSuggestionChip("<suggestion chip query>");

  suggestion_chip->RequestFocus();
  PressKeyAndWait(ui::VKEY_RETURN);

  EXPECT_HAS_FOCUS(input_text_field());
}

// TODO(b/234164113): Test is flaky.
TEST_F(AssistantPageViewTest,
       DISABLED_ShouldFocusMicWhenSubmittingSuggestionChipInVoiceMode) {
  ShowAssistantUi();
  ash::SuggestionChipView* suggestion_chip =
      CreateAndGetSuggestionChip("<suggestion chip query>");
  ClickOnAndWait(voice_input_toggle());

  suggestion_chip->RequestFocus();
  PressKeyAndWait(ui::VKEY_RETURN);

  EXPECT_HAS_FOCUS(mic_view());
}

TEST_F(AssistantPageViewTest,
       ShouldFocusTextFieldWhenPressingKeyboardInputToggle) {
  ShowAssistantUiInVoiceMode();

  ClickOnAndWait(keyboard_input_toggle());

  EXPECT_HAS_FOCUS(input_text_field());
}

TEST_F(AssistantPageViewTest,
       ShouldNotScrollSuggestionChipsWhenSubmittingQuery) {
  ShowAssistantUiInTextMode();
  MockTextInteraction()
      .WithSuggestionChip("there are                                        x")
      .WithSuggestionChip("enough queries                                   x")
      .WithSuggestionChip("to ensure                                        x")
      .WithSuggestionChip("the                                              x")
      .WithSuggestionChip("suggestion chips container                       x")
      .WithSuggestionChip("can scroll.                                      x");

  views::View* chip = GetSuggestionChips()[3];
  chip->RequestFocus();
  chip->ScrollViewToVisible();

  gfx::Rect initial_bounds = chip->GetBoundsInScreen();
  PressKeyAndWait(ui::VKEY_RETURN);
  gfx::Rect final_bounds = chip->GetBoundsInScreen();

  EXPECT_EQ(initial_bounds, final_bounds);
}

TEST_F(AssistantPageViewTest, RememberAndShowHistory) {
  ShowAssistantUiInTextMode();
  EXPECT_HAS_FOCUS(input_text_field());

  MockTextInteraction().WithQuery("query 1").WithTextResponse("response 1");
  MockTextInteraction().WithQuery("query 2").WithTextResponse("response 2");

  EXPECT_HAS_FOCUS(input_text_field());

  EXPECT_TRUE(input_text_field()->GetText().empty());

  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_EQ(input_text_field()->GetText(), u"query 2");

  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_EQ(input_text_field()->GetText(), u"query 1");

  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_EQ(input_text_field()->GetText(), u"query 1");

  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_EQ(input_text_field()->GetText(), u"query 2");

  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_TRUE(input_text_field()->GetText().empty());
}

TEST_F(AssistantPageViewTest, ShouldHaveConversationStarters) {
  DoNotShowOnboardingViews();

  ShowAssistantUi();

  EXPECT_FALSE(onboarding_view()->IsDrawn());
  EXPECT_FALSE(GetSuggestionChips().empty());
}

TEST_F(AssistantPageViewTest,
       ShouldNotHaveConversationStartersWhenShowingOnboarding) {
  ShowAssistantUi();

  EXPECT_TRUE(onboarding_view()->IsDrawn());
  EXPECT_TRUE(GetSuggestionChips().empty());
}

TEST_F(AssistantPageViewTest, ShouldHavePopulatedSuggestionChips) {
  constexpr char kAnyQuery[] = "<query>";
  constexpr char kAnyText[] = "<text>";
  constexpr char kAnyChip[] = "<chip>";

  ShowAssistantUi();
  MockTextInteraction()
      .WithQuery(kAnyQuery)
      .WithTextResponse(kAnyText)
      .WithSuggestionChip(kAnyChip);

  auto chips = GetSuggestionChips();
  ASSERT_EQ(chips.size(), 1u);
  auto* chip = chips.at(0);

  EXPECT_EQ(kAnyChip, base::UTF16ToUTF8(chip->GetText()));
}

TEST_F(AssistantPageViewTest, PageViewHasBackgroundBlurInTabletMode) {
  // AssistantPageView is only used in tablet mode. Clamshell mode uses
  // AppListBubbleAssistantPage.
  SetTabletMode(true);
  ShowAssistantUi();

  EXPECT_FALSE(page_view()->background());
  ui::Layer* page_view_layer = page_view()->layer();
  ASSERT_TRUE(page_view_layer);
  EXPECT_FALSE(page_view_layer->fills_bounds_opaquely());
  EXPECT_EQ(page_view_layer->background_blur(),
            ColorProvider::kBackgroundBlurSigma);
  EXPECT_EQ(page_view_layer->GetTargetColor(),
            ColorProvider::Get()->GetBaseLayerColor(
                ColorProvider::BaseLayerType::kTransparent80));
}

TEST_F(AssistantPageViewTest, BackgroundColorInDarkLightMode) {
  auto* color_provider = AshColorProvider::Get();
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());

  // AssistantPageView is only used in tablet mode. Clamshell mode uses
  // AppListBubbleAssistantPage.
  SetTabletMode(true);
  ShowAssistantUi();

  const bool initial_dark_mode_status =
      dark_light_mode_controller->IsDarkModeEnabled();
  EXPECT_EQ(page_view()->layer()->GetTargetColor(),
            color_provider->GetBaseLayerColor(
                ColorProvider::BaseLayerType::kTransparent80));

  // Switch the color mode.
  dark_light_mode_controller->ToggleColorMode();
  ASSERT_NE(initial_dark_mode_status,
            dark_light_mode_controller->IsDarkModeEnabled());

  EXPECT_EQ(page_view()->layer()->GetTargetColor(),
            color_provider->GetBaseLayerColor(
                ColorProvider::BaseLayerType::kTransparent80));
}

//------------------------------------------------------------------------------
// Tests the |AssistantPageView| with tablet mode enabled.
class AssistantPageViewTabletModeTest : public AssistantPageViewTest {
 public:
  AssistantPageViewTabletModeTest() = default;

  AssistantPageViewTabletModeTest(const AssistantPageViewTabletModeTest&) =
      delete;
  AssistantPageViewTabletModeTest& operator=(
      const AssistantPageViewTabletModeTest&) = delete;

  void SetUp() override {
    AssistantPageViewTest::SetUp();
    SetTabletMode(true);
  }

  // Ensures all views are created by showing and hiding the UI.
  void CreateAllViews() {
    ShowAssistantUi();
    CloseAssistantUi();
  }

  void ShowAssistantUiInTextMode() {
    // Note we launch in voice mode and switch to text because that was the
    // easiest way I could get this working (we can't simply use |kUnspecified|
    // as that puts us in voice mode.
    ShowAssistantUiInVoiceMode();
    TapOnAndWait(keyboard_input_toggle());
  }

  // Returns a point in the AppList, but outside the Assistant UI.
  gfx::Point GetPointInAppListOutsideAssistantUi() {
    gfx::Point result = GetPointOutside(page_view());

    // Validity check
    EXPECT_TRUE(app_list_view()->bounds().Contains(result));
    EXPECT_FALSE(page_view()->bounds().Contains(result));

    return result;
  }

  gfx::Point GetPointOutside(const views::View* view) {
    return gfx::Point(view->origin().x() - 10, view->origin().y() - 10);
  }

  gfx::Point GetPointInside(const views::View* view) {
    return view->GetBoundsInScreen().CenterPoint();
  }
};

TEST_F(AssistantPageViewTabletModeTest,
       ShouldFocusMicWhenOpeningWithLongPressLauncher) {
  ShowAssistantUi(AssistantEntryPoint::kLongPressLauncher);

  EXPECT_HAS_FOCUS(mic_view());
}

TEST_F(AssistantPageViewTabletModeTest, ShouldFocusMicWhenOpeningWithHotword) {
  ShowAssistantUi(AssistantEntryPoint::kHotword);

  EXPECT_HAS_FOCUS(mic_view());
}

TEST_F(AssistantPageViewTabletModeTest, ShouldFocusTextFieldAfterSendingQuery) {
  ShowAssistantUiInTextMode();

  SendQueryThroughTextField("The query");

  EXPECT_HAS_FOCUS(input_text_field());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldDismissKeyboardAfterSendingQuery) {
  ShowAssistantUiInTextMode();
  EXPECT_TRUE(IsKeyboardShowing());

  SendQueryThroughTextField("The query");

  EXPECT_FALSE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldNotShowKeyboardWhenOpeningLauncherButNotAssistantUi) {
  OpenLauncher();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldShowKeyboardAfterTouchingInputTextbox) {
  ShowAssistantUiInTextMode();
  DismissKeyboard();
  EXPECT_FALSE(IsKeyboardShowing());

  TapOnAndWait(input_text_field());

  EXPECT_TRUE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest, ShouldNotShowKeyboardWhenItsDisabled) {
  // This tests the scenario where the keyboard is disabled even in tablet mode,
  // e.g. when an external keyboard is connected to a tablet.
  DisableKeyboard();
  ShowAssistantUiInTextMode();

  EXPECT_FALSE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldFocusTextFieldAfterPressingKeyboardInputToggle) {
  ShowAssistantUiInVoiceMode();
  EXPECT_NOT_HAS_FOCUS(input_text_field());

  TapOnAndWait(keyboard_input_toggle());

  EXPECT_HAS_FOCUS(input_text_field());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldShowKeyboardAfterPressingKeyboardInputToggle) {
  ShowAssistantUiInVoiceMode();
  EXPECT_FALSE(IsKeyboardShowing());

  TapOnAndWait(keyboard_input_toggle());

  EXPECT_TRUE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldDismissKeyboardAfterPressingVoiceInputToggle) {
  ShowAssistantUiInTextMode();
  EXPECT_TRUE(IsKeyboardShowing());

  TapOnAndWait(voice_input_toggle());

  EXPECT_FALSE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldDismissKeyboardWhenOpeningUiInVoiceMode) {
  // Start by focussing a text field so the system has a reason to show the
  // keyboard.
  views::Widget* widget = SwitchToNewWidget();
  auto* textfield = AddTextfield(widget);
  TapOnAndWait(textfield);
  ASSERT_TRUE(IsKeyboardShowing());

  ShowAssistantUiInVoiceMode();

  EXPECT_FALSE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldDismissAssistantUiIfLostFocusWhenOtherAppWindowOpens) {
  ShowAssistantUi();

  // Create a new window to steal the focus which should dismiss the Assistant
  // UI.
  SwitchToNewAppWindow();

  EXPECT_FALSE(IsVisible());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldNotShowKeyboardWhenClosingAssistantUI) {
  // Note: This checks for a bug where the closing sequence of the UI switches
  // the input mode to text which then shows the keyboard.
  ShowAssistantUiInVoiceMode();

  CloseAssistantUi();

  EXPECT_FALSE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldDismissKeyboardWhenClosingAssistantUI) {
  ShowAssistantUiInTextMode();
  EXPECT_TRUE(IsKeyboardShowing());

  // Enable the animations because the fact that the switch-modality animation
  // runs changes the timing enough that it triggers a potential bug where the
  // keyboard remains visible after the UI is closed.
  ui::ScopedAnimationDurationScaleMode enable_animations{
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION};

  CloseAssistantUi();

  EXPECT_FALSE(IsKeyboardShowing());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldDismissAssistantUiWhenTappingAppListView) {
  ShowAssistantUiInVoiceMode();

  TapAndWait(GetPointInAppListOutsideAssistantUi());

  EXPECT_FALSE(IsVisible());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldDismissKeyboardButNotAssistantUiWhenTappingAppListView) {
  // Note: This is consistent with how the app list search box works;
  // the first tap will dismiss the keyboard but not change the state of the
  // search box.
  ShowAssistantUiInTextMode();
  EXPECT_TRUE(IsKeyboardShowing());

  TapAndWait(GetPointInAppListOutsideAssistantUi());

  EXPECT_FALSE(IsKeyboardShowing());
  EXPECT_TRUE(IsVisible());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldNotDismissKeyboardWhenTappingInsideAssistantUi) {
  ShowAssistantUiInTextMode();
  EXPECT_TRUE(IsKeyboardShowing());

  TapAndWait(GetPointInside(page_view()));

  EXPECT_TRUE(IsKeyboardShowing());
  EXPECT_TRUE(IsVisible());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldNotDismissAssistantUiWhenTappingInsideAssistantUi) {
  ShowAssistantUi();

  TapAndWait(GetPointInside(page_view()));

  EXPECT_TRUE(IsVisible());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldNotFlickerTextInputWhenOpeningAssistantUI) {
  // If the text input field is visible at any time when opening the Assistant
  // UI, it will cause an unwanted visible animation (of the voice input
  // animating in).

  CreateAllViews();
  VisibilityObserver text_field_observer(input_text_field());

  ShowAssistantUiInVoiceMode();

  EXPECT_FALSE(text_field_observer.was_drawn());
}

TEST_F(AssistantPageViewTabletModeTest,
       ShouldNotFlickerTextInputWhenClosingAssistantUI) {
  // Same as above but for the closing animation.
  ShowAssistantUiInVoiceMode();

  VisibilityObserver text_field_observer(input_text_field());

  CloseAssistantUi();

  EXPECT_FALSE(text_field_observer.was_drawn());
}

TEST_F(AssistantPageViewTabletModeTest, ShouldCloseAssistantUIInOverviewMode) {
  ShowAssistantUi(AssistantEntryPoint::kLongPressLauncher);
  EXPECT_TRUE(IsVisible());

  StartOverview();
  EXPECT_FALSE(IsVisible());
}

}  // namespace
}  // namespace ash
