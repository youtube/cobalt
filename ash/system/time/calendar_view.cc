// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_view.h"

#include <memory>
#include <string>

#include "ash/bubble/bubble_utils.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/time/calendar_event_list_view.h"
#include "ash/system/time/calendar_metrics.h"
#include "ash/system/time/calendar_month_view.h"
#include "ash/system/time/calendar_up_next_view.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/time/date_helper.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

// The paddings in each view.
constexpr int kContentVerticalPadding = 20;
constexpr int kContentHorizontalPadding = 20;
constexpr int kMonthVerticalPadding = 10;
constexpr int kLabelVerticalPadding = 10;
constexpr int kLabelTextInBetweenPadding = 4;
const int kWeekRowHorizontalPadding =
    kContentHorizontalPadding - calendar_utils::kDateHorizontalPadding;
constexpr int kExpandedCalendarPadding = 10;
constexpr int kChevronPadding = calendar_utils::kColumnSetPadding - 1;
constexpr int kChevronInBetweenPadding = 16;
constexpr int kMonthHeaderLabelTopPadding = 14;
constexpr int kMonthHeaderLabelBottomPadding = 2;
constexpr int kEventListViewHorizontalOffset = 1;
constexpr int kUpNextAnimationYOffset = 20;
// Adds a gap between the bottom visible row in the scrollview and the top of
// the event list view when open.
constexpr int kCalendarEventListViewOpenMargin = 8;

// The offset for `month_label_` to make it align with `month_header`.
constexpr int kMonthLabelPaddingOffset = -1;

// The cool-down time for calling `UpdateOnScreenMonthMap()` after scrolling.
constexpr base::TimeDelta kScrollingSettledTimeout = base::Milliseconds(500);

// The max number of rows in a month.
constexpr int kMaxRowsInOneMonth = 6;

// Duration of the delay for starting header animation.
constexpr base::TimeDelta kDelayHeaderAnimationDuration =
    base::Milliseconds(200);

// Duration of events moving animation.
constexpr base::TimeDelta kAnimationDurationForEventsMoving =
    base::Milliseconds(400);

// Duration of closing events panel animation.
constexpr base::TimeDelta kAnimationDurationForClosingEvents =
    base::Milliseconds(200);

// Duration of `event_list_view_` fade in animation delay.
constexpr base::TimeDelta kEventListAnimationStartDelay =
    base::Milliseconds(100);

// Duration of `up_next_view_` fade in animation delay.
constexpr base::TimeDelta kUpNextAnimationStartDelay = base::Milliseconds(50);

// The cool-down time for enabling animation.
constexpr base::TimeDelta kAnimationDisablingTimeout = base::Milliseconds(500);

// Periodic time delay for checking upcoming events.
constexpr base::TimeDelta kCheckUpcomingEventsDelay = base::Seconds(15);

// The multiplier used to reduce velocity of flings on the calendar view.
// Without this, CalendarView will scroll a few years per fast swipe.
constexpr float kCalendarScrollFlingMultiplier = 0.25f;

constexpr char kMonthViewScrollOneMonthAnimationHistogram[] =
    "Ash.CalendarView.ScrollOneMonth.MonthView.AnimationSmoothness";

constexpr char kLabelViewScrollOneMonthAnimationHistogram[] =
    "Ash.CalendarView.ScrollOneMonth.LabelView.AnimationSmoothness";

constexpr char kHeaderViewScrollOneMonthAnimationHistogram[] =
    "Ash.CalendarView.ScrollOneMonth.HeaderView.AnimationSmoothness";

constexpr char kContentViewResetToTodayAnimationHistogram[] =
    "Ash.CalendarView.ResetToToday.ContentView.AnimationSmoothness";

constexpr char kHeaderViewResetToTodayAnimationHistogram[] =
    "Ash.CalendarView.ResetToToday.HeaderView.AnimationSmoothness";

constexpr char kContentViewFadeInCurrentMonthAnimationHistogram[] =
    "Ash.CalendarView.FadeInCurrentMonth.ContentView.AnimationSmoothness";

constexpr char kHeaderViewFadeInCurrentMonthAnimationHistogram[] =
    "Ash.CalendarView.FadeInCurrentMonth.HeaderView.AnimationSmoothness";

constexpr char kOnMonthChangedAnimationHistogram[] =
    "Ash.CalendarView.OnMonthChanged.AnimationSmoothness";

constexpr char kCloseEventListAnimationHistogram[] =
    "Ash.CalendarView.CloseEventList.EventListView.AnimationSmoothness";

constexpr char kCloseEventListCalendarSlidingSurfaceAnimationHistogram[] =
    "Ash.CalendarView.CloseEventList.CalendarSlidingSurface."
    "AnimationSmoothness";

constexpr char kCloseEventListUpNextViewAnimationHistogram[] =
    "Ash.CalendarView.CloseEventList.UpNextView.AnimationSmoothness";

constexpr char kMonthViewOpenEventListAnimationHistogram[] =
    "Ash.CalendarView.OpenEventList.MonthView.AnimationSmoothness";

constexpr char kLabelViewOpenEventListAnimationHistogram[] =
    "Ash.CalendarView.OpenEventList.LabelView.AnimationSmoothness";

constexpr char kEventListViewOpenEventListAnimationHistogram[] =
    "Ash.CalendarView.OpenEventList.EventListView.AnimationSmoothness";

constexpr char kCalendarSlidingSurfaceOpenEventListAnimationHistogram[] =
    "Ash.CalendarView.OpenEventList.CalendarSlidingSurface.AnimationSmoothness";

constexpr char kUpNextViewOpenEventListAnimationHistogram[] =
    "Ash.CalendarView.OpenEventList.UpNextView.AnimationSmoothness";

constexpr char kShowUpNextViewAnimationHistogram[] =
    "Ash.CalendarView.ShowUpNextView.AnimationSmoothness";

constexpr char kSmoothScrollMonthViewWhenShowingTodaysDateCell[] =
    "Ash.CalendarView.SmoothScrollToTodaysDateCell.MonthView."
    "AnimationSmoothness";

constexpr char kSmoothScrollLabelViewWhenShowingTodaysDateCell[] =
    "Ash.CalendarView.SmoothScrollToTodaysDateCell.LabelView."
    "AnimationSmoothness";

std::unique_ptr<views::Label> CreateHeaderView(const std::u16string& month) {
  return views::Builder<views::Label>(
             bubble_utils::CreateLabel(TypographyToken::kCrosDisplay7, month,
                                       cros_tokens::kCrosSysOnSurface))
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD)
      .SetTextContext(CONTEXT_CALENDAR_LABEL)
      .SetAutoColorReadabilityEnabled(false)
      .Build();
}

std::unique_ptr<views::Label> CreateHeaderYearView(const std::u16string& year) {
  const int label_padding = kLabelTextInBetweenPadding;

  return views::Builder<views::Label>(
             bubble_utils::CreateLabel(TypographyToken::kCrosDisplay7, year,
                                       cros_tokens::kCrosSysOnSurface))
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD)
      .SetTextContext(CONTEXT_CALENDAR_LABEL)
      .SetAutoColorReadabilityEnabled(false)
      .SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, label_padding)))
      .Build();
}

int GetExpandedCalendarPadding() {
  return kExpandedCalendarPadding;
}

// The overridden `Label` view used in `CalendarView`.
class CalendarLabel : public views::Label {
 public:
  explicit CalendarLabel(const std::u16string& text) : views::Label(text) {
    views::Label::SetEnabledColor(calendar_utils::GetPrimaryTextColor());
    views::Label::SetAutoColorReadabilityEnabled(false);
  }
  CalendarLabel(const CalendarLabel&) = delete;
  CalendarLabel& operator=(const CalendarLabel&) = delete;
  ~CalendarLabel() override = default;

  void OnThemeChanged() override {
    views::Label::OnThemeChanged();

    views::Label::SetEnabledColor(calendar_utils::GetPrimaryTextColor());
  }
};

// The month view header which contains the title of each week day.
class MonthHeaderView : public views::View {
 public:
  MonthHeaderView() {
    views::TableLayout* layout =
        SetLayoutManager(std::make_unique<views::TableLayout>());
    calendar_utils::SetUpWeekColumns(layout);
    layout->AddRows(1, views::TableLayout::kFixedSize);

    for (const std::u16string& week_day :
         DateHelper::GetInstance()->week_titles()) {
      auto label =
          views::Builder<views::Label>(
              bubble_utils::CreateLabel(TypographyToken::kCrosButton1, week_day,
                                        cros_tokens::kCrosSysOnSurface))
              .Build();
      label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
      label->SetBorder((views::CreateEmptyBorder(
          gfx::Insets::VH(calendar_utils::kDateVerticalPadding, 0))));
      label->SetElideBehavior(gfx::NO_ELIDE);
      label->SetSubpixelRenderingEnabled(false);

      AddChildView(std::move(label));
    }
  }

  MonthHeaderView(const MonthHeaderView& other) = delete;
  MonthHeaderView& operator=(const MonthHeaderView& other) = delete;
  ~MonthHeaderView() override = default;
};

// Resets the `view`'s opacity and position.
void ResetLayer(views::View* view) {
  view->layer()->SetOpacity(1.0f);
  view->layer()->SetTransform(gfx::Transform());
}

}  // namespace

// The label for each month that's within the scroll view.
class CalendarView::MonthHeaderLabelView : public views::View {
 public:
  MonthHeaderLabelView(LabelType type,
                       CalendarViewController* calendar_view_controller)
      : month_label_(AddChildView(CreateHeaderView(std::u16string()))) {
    // The layer is required in animation.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    switch (type) {
      case PREVIOUS:
        month_name_ = calendar_view_controller->GetPreviousMonthName();
        break;
      case CURRENT:
        month_name_ = calendar_view_controller->GetOnScreenMonthName();
        break;
      case NEXT:
        month_name_ = calendar_view_controller->GetNextMonthName();
        break;
      case NEXTNEXT:
        month_name_ =
            calendar_view_controller->GetNextMonthName(/*num_months=*/2);
        break;
    }
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));

    month_label_->SetText(month_name_);
    month_label_->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(kMonthHeaderLabelTopPadding,
                          kContentHorizontalPadding + kMonthLabelPaddingOffset,
                          kMonthHeaderLabelBottomPadding, 0)));
  }
  MonthHeaderLabelView(const MonthHeaderLabelView&) = delete;
  MonthHeaderLabelView& operator=(const MonthHeaderLabelView&) = delete;
  ~MonthHeaderLabelView() override = default;

 private:
  // The name of the month.
  std::u16string month_name_;

  // The month label in the view.
  const raw_ptr<views::Label, ExperimentalAsh> month_label_ = nullptr;
};

CalendarView::ScrollContentsView::ScrollContentsView(
    CalendarViewController* controller)
    : controller_(controller),
      stylus_event_handler_(this),
      current_month_(controller_->GetOnScreenMonthName()) {}

void CalendarView::ScrollContentsView::OnMonthChanged() {
  current_month_ = controller_->GetOnScreenMonthName();
}

void CalendarView::ScrollContentsView::OnEvent(ui::Event* event) {
  views::View::OnEvent(event);

  if (controller_->GetOnScreenMonthName() == current_month_) {
    return;
  }
  OnMonthChanged();

  if (event->IsMouseWheelEvent()) {
    calendar_metrics::RecordScrollSource(
        calendar_metrics::CalendarViewScrollSource::kByMouseWheel);
  }

  if (event->IsScrollGestureEvent()) {
    calendar_metrics::RecordScrollSource(
        calendar_metrics::CalendarViewScrollSource::kByGesture);
  }

  if (event->IsFlingScrollEvent()) {
    calendar_metrics::RecordScrollSource(
        calendar_metrics::CalendarViewScrollSource::kByFling);
  }
}

void CalendarView::ScrollContentsView::OnStylusEvent(
    const ui::TouchEvent& event) {
  if (controller_->GetOnScreenMonthName() == current_month_) {
    return;
  }
  OnMonthChanged();

  calendar_metrics::RecordScrollSource(
      calendar_metrics::CalendarViewScrollSource::kByStylus);
}

CalendarView::ScrollContentsView::StylusEventHandler::StylusEventHandler(
    ScrollContentsView* content_view)
    : content_view_(content_view) {
  Shell::Get()->AddPreTargetHandler(this);
}

CalendarView::ScrollContentsView::StylusEventHandler::~StylusEventHandler() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void CalendarView::ScrollContentsView::StylusEventHandler::OnTouchEvent(
    ui::TouchEvent* event) {
  if (event->pointer_details().pointer_type == ui::EventPointerType::kPen) {
    content_view_->OnStylusEvent(*event);
  }
}

CalendarHeaderView::CalendarHeaderView(const std::u16string& month,
                                       const std::u16string& year)
    : header_(AddChildView(CreateHeaderView(month))),
      header_year_(AddChildView(CreateHeaderYearView(year))) {
  // The layer is required in animation.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
}

CalendarHeaderView::~CalendarHeaderView() = default;

void CalendarHeaderView::UpdateHeaders(const std::u16string& month,
                                       const std::u16string& year) {
  header_->SetText(month);
  header_year_->SetText(year);
}

BEGIN_METADATA(CalendarHeaderView, views::View)
END_METADATA

CalendarView::CalendarView(DetailedViewDelegate* delegate,
                           bool for_glanceables_container)
    : GlanceableTrayChildBubble(delegate, for_glanceables_container),
      calendar_view_controller_(std::make_unique<CalendarViewController>()),
      scrolling_settled_timer_(
          FROM_HERE,
          kScrollingSettledTimeout,
          base::BindRepeating(&CalendarView::UpdateOnScreenMonthMap,
                              base::Unretained(this))),
      header_animation_restart_timer_(
          FROM_HERE,
          kAnimationDisablingTimeout,
          base::BindRepeating(
              [](CalendarView* calendar_view) {
                if (!calendar_view) {
                  return;
                }
                calendar_view->set_should_header_animate(true);
              },
              base::Unretained(this))),
      months_animation_restart_timer_(
          FROM_HERE,
          kAnimationDisablingTimeout,
          base::BindRepeating(
              [](CalendarView* calendar_view) {
                if (!calendar_view) {
                  return;
                }
                calendar_view->set_should_months_animate(true);
              },
              base::Unretained(this))) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  SetFocusBehavior(FocusBehavior::ALWAYS);

  // Focusable nodes must have an accessible name and valid role.
  // TODO(crbug.com/1348930): Review the accessible name and role.
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kPane);
  GetViewAccessibility().OverrideName(GetClassName());

  // Since there's no separator in the `CalendarView`, first sets
  // `has_separator` in `TrayDetailedView` to false.
  IgnoreSeparator();

  CreateTitleRow(IDS_ASH_CALENDAR_TITLE, /*create_back_button=*/false);

  // Adds the progress bar to layout when initialization to avoid changing the
  // layout while reading the bounds of it.
  ShowProgress(-1, false);

  // Add the header. The `temp_header_` only shows up during the header
  // animation.
  auto* header_container = new views::View();
  header_container->SetLayoutManager(std::make_unique<views::FillLayout>());
  auto header = std::make_unique<CalendarHeaderView>(
      calendar_view_controller_->GetOnScreenMonthName(),
      calendar_utils::GetYear(
          calendar_view_controller_->currently_shown_date()));
  auto temp_header = std::make_unique<CalendarHeaderView>(
      calendar_view_controller_->GetPreviousMonthName(),
      calendar_utils::GetYear(
          calendar_view_controller_->currently_shown_date()));
  temp_header->SetVisible(false);
  header_ = header_container->AddChildView(std::move(header));
  temp_header_ = header_container->AddChildView(std::move(temp_header));

  TriView* tri_view =
      TrayPopupUtils::CreateDefaultRowView(/*use_wide_layout=*/false);
  tri_view->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kLabelVerticalPadding, kContentHorizontalPadding, 0,
                        kContentHorizontalPadding - kChevronPadding)));
  tri_view->AddView(TriView::Container::START, header_container);

  auto* button_container = new views::View();
  const int horizontal_padding = kWeekRowHorizontalPadding;
  views::BoxLayout* button_container_layout =
      button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  button_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  // Aligns button with the calendar dates in the `TableLayout`.
  button_container_layout->set_between_child_spacing(kChevronInBetweenPadding);

  up_button_ = button_container->AddChildView(std::make_unique<IconButton>(
      base::BindRepeating(&CalendarView::OnMonthArrowButtonActivated,
                          base::Unretained(this), /*up=*/true),
      IconButton::Type::kMediumFloating, &vector_icons::kCaretUpIcon,
      IDS_ASH_CALENDAR_UP_BUTTON_ACCESSIBLE_DESCRIPTION));

  down_button_ = button_container->AddChildView(std::make_unique<IconButton>(
      base::BindRepeating(&CalendarView::OnMonthArrowButtonActivated,
                          base::Unretained(this), /*up=*/false),
      IconButton::Type::kMediumFloating, &vector_icons::kCaretDownIcon,
      IDS_ASH_CALENDAR_DOWN_BUTTON_ACCESSIBLE_DESCRIPTION));

  tri_view->AddView(TriView::Container::END, button_container);
  AddChildView(tri_view);

  // Add month header.
  auto month_header = std::make_unique<MonthHeaderView>();
  month_header->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, horizontal_padding, 0, horizontal_padding)));
  AddChildView(std::move(month_header));

  // Add scroll view.
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view_->SetAllowKeyboardScrolling(false);
  scroll_view_->SetBackgroundColor(absl::nullopt);
  ClipScrollViewHeight(ScrollViewState::FULL_HEIGHT);
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  scroll_view_->vertical_scroll_bar()->SetFlingMultiplier(
      kCalendarScrollFlingMultiplier);

  scroll_view_->SetFocusBehavior(FocusBehavior::NEVER);
  on_contents_scrolled_subscription_ =
      scroll_view_->AddContentsScrolledCallback(base::BindRepeating(
          &CalendarView::OnContentsScrolled, base::Unretained(this)));

  content_view_ = scroll_view_->SetContents(
      std::make_unique<ScrollContentsView>(calendar_view_controller_.get()));
  content_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  content_view_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kContentVerticalPadding, horizontal_padding,
                        kContentVerticalPadding, horizontal_padding)));

  // Focusable nodes must have an accessible name and valid role.
  // TODO(crbug.com/1348930): Review the accessible name and role.
  content_view_->GetViewAccessibility().OverrideRole(ax::mojom::Role::kPane);
  content_view_->GetViewAccessibility().OverrideName(GetClassName());
  content_view_->SetFocusBehavior(FocusBehavior::ALWAYS);

  // Set up layer for animations.
  content_view_->SetPaintToLayer();
  content_view_->layer()->SetFillsBoundsOpaquely(false);

  SetMonthViews();

  // Container used for animating the event list view and / or the up next view.
  calendar_sliding_surface_ = AddChildView(std::make_unique<views::View>());
  calendar_sliding_surface_->SetUseDefaultFillLayout(true);
  // We manipulate this layer with translations which can take it off the screen
  // so for the animations to work we need to control its positioning.
  calendar_sliding_surface_->SetProperty(views::kViewIgnoredByLayoutKey, true);
  // This layer is required for animations.
  calendar_sliding_surface_->SetPaintToLayer();
  calendar_sliding_surface_->layer()->SetFillsBoundsOpaquely(false);

  // Override the default focus order so the calendar contents (which contains
  // the current date view) and the UI within calendar sliding surfaces get
  // focused before the "Today" button in the calendar view header.
  scroll_view_->InsertBeforeInFocusList(TrayDetailedView::tri_view());
  calendar_sliding_surface_->InsertAfterInFocusList(scroll_view_);

  scoped_calendar_model_observer_.Observe(calendar_model_.get());
  scoped_calendar_view_controller_observer_.Observe(
      calendar_view_controller_.get());
  scoped_view_observer_.AddObservation(scroll_view_.get());
  scoped_view_observer_.AddObservation(content_view_.get());
  scoped_view_observer_.AddObservation(this);

  check_upcoming_events_timer_.Start(
      FROM_HERE, kCheckUpcomingEventsDelay,
      base::BindRepeating(&CalendarView::MaybeShowUpNextView,
                          base::Unretained(this)));
}

CalendarView::~CalendarView() {
  is_destroying_ = true;
  RestoreHeadersStatus();
  RestoreMonthStatus();

  // Removes child views including month views and event list to remove their
  // dependency from `CalendarViewController`, since these views are destructed
  // after the controller.
  if (event_list_view_) {
    calendar_sliding_surface_->RemoveChildViewT(event_list_view_.get());
    event_list_view_ = nullptr;
  }
  check_upcoming_events_timer_.Stop();
  RemoveUpNextView();
  content_view_->RemoveAllChildViews();
}

void CalendarView::CreateExtraTitleRowButtons() {
  tri_view()->SetContainerVisible(TriView::Container::END, /*visible=*/true);
  if (calendar_utils::IsDisabledByAdmin()) {
    DCHECK(!managed_button_);
    managed_button_ = tri_view()->AddView(
        TriView::Container::END,
        std::make_unique<IconButton>(
            base::BindRepeating([]() {
              Shell::Get()->system_tray_model()->client()->ShowEnterpriseInfo();
            }),
            IconButton::Type::kMedium, &kSystemTrayManagedIcon,
            IDS_ASH_CALENDAR_DISABLED_BY_ADMIN));
  }

  DCHECK(!reset_to_today_button_);
  reset_to_today_button_ = CreateInfoButton(
      base::BindRepeating(&CalendarView::ResetToTodayWithAnimation,
                          base::Unretained(this)),
      IDS_ASH_CALENDAR_INFO_BUTTON_ACCESSIBLE_DESCRIPTION);
  reset_to_today_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_TODAY_BUTTON_TOOLTIP));
  tri_view()->AddView(TriView::Container::END, reset_to_today_button_);

  DCHECK(!settings_button_);
  settings_button_ = CreateSettingsButton(
      base::BindRepeating([]() {
        ClockModel* model = Shell::Get()->system_tray_model()->clock();

        if (Shell::Get()->session_controller()->ShouldEnableSettings()) {
          model->ShowDateSettings();
        } else if (model->can_set_time()) {
          model->ShowSetTimeDialog();
        }
      }),
      IDS_ASH_CALENDAR_SETTINGS);
  settings_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_SETTINGS_TOOLTIP));
  tri_view()->AddView(TriView::Container::END, settings_button_);
}

views::Button* CalendarView::CreateInfoButton(
    views::Button::PressedCallback callback,
    int info_accessible_name_id) {
  auto* button =
      new PillButton(std::move(callback),
                     l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_INFO_BUTTON),
                     PillButton::Type::kDefaultWithoutIcon, /*icon=*/nullptr);
  button->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_INFO_BUTTON_ACCESSIBLE_DESCRIPTION,
      calendar_utils::GetMonthDayYear(base::Time::Now())));
  return button;
}

void CalendarView::SetMonthViews() {
  previous_label_ = AddLabelWithId(LabelType::PREVIOUS);
  previous_month_ =
      AddMonth(calendar_view_controller_->GetPreviousMonthFirstDayUTC(1));

  current_label_ = AddLabelWithId(LabelType::CURRENT);
  current_month_ =
      AddMonth(calendar_view_controller_->GetOnScreenMonthFirstDayUTC());

  next_label_ = AddLabelWithId(LabelType::NEXT);
  next_month_ = AddMonth(calendar_view_controller_->GetNextMonthFirstDayUTC(1));

  next_next_label_ = AddLabelWithId(LabelType::NEXTNEXT);
  next_next_month_ = AddMonth(
      calendar_view_controller_->GetNextMonthFirstDayUTC(/*num_months=*/2));
}

int CalendarView::PositionOfCurrentMonth() const {
  // Compute the position, because this information may be required before
  // layout.
  return kContentVerticalPadding +
         previous_label_->GetPreferredSize().height() +
         previous_month_->GetPreferredSize().height() +
         current_label_->GetPreferredSize().height();
}

int CalendarView::PositionOfToday() const {
  return PositionOfCurrentMonth() +
         calendar_view_controller_->GetTodayRowTopHeight();
}

int CalendarView::PositionOfSelectedDate() const {
  DCHECK(calendar_view_controller_->selected_date().has_value());
  const int row_height = calendar_view_controller_->selected_date_row_index() *
                             calendar_view_controller_->row_height() +
                         GetExpandedCalendarPadding();
  // The selected date should be either in the current month or the next month.
  if (calendar_view_controller_->IsSelectedDateInCurrentMonth()) {
    return PositionOfCurrentMonth() + row_height;
  }

  return PositionOfCurrentMonth() +
         current_month_->GetPreferredSize().height() +
         next_label_->GetPreferredSize().height() + row_height;
}

void CalendarView::SetHeaderAndContentViewOpacity(float opacity) {
  header_->layer()->SetOpacity(opacity);
  content_view_->layer()->SetOpacity(opacity);
}

void CalendarView::SetShouldMonthsAnimateAndScrollEnabled(bool enabled) {
  set_should_months_animate(enabled);
  is_resetting_scroll_ = !enabled;
  scroll_view_->SetVerticalScrollBarMode(
      enabled ? views::ScrollView::ScrollBarMode::kHiddenButEnabled
              : views::ScrollView::ScrollBarMode::kDisabled);
}

void CalendarView::ResetToTodayWithAnimation() {
  if (!should_months_animate_) {
    return;
  }
  SetShouldMonthsAnimateAndScrollEnabled(/*enabled=*/false);

  auto content_reporter = calendar_metrics::CreateAnimationReporter(
      content_view_, kContentViewResetToTodayAnimationHistogram);
  auto header_reporter = calendar_metrics::CreateAnimationReporter(
      header_, kHeaderViewResetToTodayAnimationHistogram);

  // Fades out on-screen month. When animation ends sets date to today by
  // calling `ResetToToday` and fades in updated views after.
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&CalendarView::OnResetToTodayAnimationComplete,
                              weak_factory_.GetWeakPtr()))
      .OnAborted(base::BindOnce(&CalendarView::OnResetToTodayAnimationComplete,
                                weak_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(calendar_utils::kResetToTodayFadeAnimationDuration)
      .SetOpacity(header_, 0.0f)
      .SetOpacity(content_view_, 0.0f);
}

void CalendarView::ResetToToday() {
  if (event_list_view_) {
    scroll_view_->SetVerticalScrollBarMode(
        views::ScrollView::ScrollBarMode::kHiddenButEnabled);
    set_should_months_animate(false);
  }

  // Updates month to today's date without animating header.
  {
    base::AutoReset<bool> is_updating_month(&should_header_animate_, false);
    calendar_view_controller_->UpdateMonth(base::Time::Now());
  }

  content_view_->RemoveChildViewT(previous_label_.get());
  content_view_->RemoveChildViewT(previous_month_.get());
  content_view_->RemoveChildViewT(current_label_.get());
  content_view_->RemoveChildViewT(current_month_.get());
  content_view_->RemoveChildViewT(next_label_.get());
  content_view_->RemoveChildViewT(next_month_.get());
  content_view_->RemoveChildViewT(next_next_label_.get());
  content_view_->RemoveChildViewT(next_next_month_.get());

  // Before adding new label and month views, reset the `scroll_view_` to 0
  // position. Otherwise after all the views are deleted the 'scroll_view_`'s
  // position is still pointing to the position of the previous `current_month_`
  // view which is outside of the view. This will cause the scroll view show
  // nothing on the screen and get stuck there (can not scroll up and down any
  // more).
  {
    base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);
    scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(), 0);
  }

  SetMonthViews();
  ScrollToToday();
  MaybeResetContentViewFocusBehavior();

  if (event_list_view_) {
    // `ShowEventListView()` also updates the selected view.
    DCHECK(current_month_->has_today());
    calendar_view_controller_->ShowEventListView(
        calendar_view_controller_->todays_date_cell_view(), base::Time::Now(),
        calendar_view_controller_->today_row());
    months_animation_restart_timer_.Reset();
    SetShouldMonthsAnimateAndScrollEnabled(/*enabled=*/true);
    scroll_view_->SetVerticalScrollBarMode(
        views::ScrollView::ScrollBarMode::kDisabled);
  }
}

void CalendarView::UpdateOnScreenMonthMap() {
  base::Time current_date = calendar_view_controller_->currently_shown_date();
  base::Time start_time = calendar_utils::GetStartOfMonthUTC(
      current_date + calendar_utils::GetTimeDifference(current_date));

  on_screen_month_.clear();
  on_screen_month_[start_time] =
      calendar_model_->FindFetchingStatus(start_time);

  // Checks if `next_month_` is in the visible view. If so, adds it to
  // `on_screen_month_` if not already present. Otherwise updates the fetching
  // status. This is needed since a refetching request may be sent when this
  // function is called and we need to update the fetching status to toggle the
  // visibility of the loading bar.
  if (scroll_view_->GetVisibleRect().bottom() >= next_month_->y()) {
    base::Time next_start_time =
        calendar_utils::GetStartOfNextMonthUTC(start_time);
    on_screen_month_[next_start_time] =
        calendar_model_->FindFetchingStatus(next_start_time);

    // Checks if `next_next_month_` is in the visible view.
    if (scroll_view_->GetVisibleRect().bottom() >= next_next_month_->y()) {
      base::Time next_next_start_time =
          calendar_utils::GetStartOfNextMonthUTC(next_start_time);
      on_screen_month_[next_next_start_time] =
          calendar_model_->FindFetchingStatus(next_next_start_time);
    }
  }

  MaybeUpdateLoadingBarVisibility();
  calendar_view_controller_->CalendarLoaded();
}

bool CalendarView::EventsFetchComplete() {
  for (auto& it : on_screen_month_) {
    // Return false if there's an on-screen month that hasn't finished fetching
    // or re-fetching.
    if (it.second == CalendarModel::kFetching ||
        it.second == CalendarModel::kRefetching) {
      return false;
    }
  }
  return true;
}

void CalendarView::MaybeUpdateLoadingBarVisibility() {
  ShowProgress(-1, !EventsFetchComplete());
}

void CalendarView::FadeInCurrentMonth() {
  if (!should_months_animate_) {
    return;
  }
  SetShouldMonthsAnimateAndScrollEnabled(/*enabled=*/false);

  content_view_->SetPaintToLayer();
  content_view_->layer()->SetFillsBoundsOpaquely(false);
  SetHeaderAndContentViewOpacity(/*opacity=*/0.0f);

  auto content_reporter = calendar_metrics::CreateAnimationReporter(
      content_view_, kContentViewFadeInCurrentMonthAnimationHistogram);
  auto header_reporter = calendar_metrics::CreateAnimationReporter(
      header_, kHeaderViewFadeInCurrentMonthAnimationHistogram);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(
          base::BindOnce(&CalendarView::OnResetToTodayFadeInAnimationComplete,
                         weak_factory_.GetWeakPtr()))
      .OnAborted(
          base::BindOnce(&CalendarView::OnResetToTodayFadeInAnimationComplete,
                         weak_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(calendar_utils::kResetToTodayFadeAnimationDuration)
      .SetOpacity(header_, 1.0f)
      .SetOpacity(content_view_, 1.0f);
}

void CalendarView::UpdateHeaders() {
  header_->UpdateHeaders(
      calendar_view_controller_->GetOnScreenMonthName(),
      calendar_utils::GetYear(
          calendar_view_controller_->currently_shown_date()));
}

void CalendarView::RestoreHeadersStatus() {
  header_->layer()->GetAnimator()->StopAnimating();
  header_->layer()->SetOpacity(1.0f);
  header_->layer()->SetTransform(gfx::Transform());
  temp_header_->layer()->GetAnimator()->StopAnimating();
  temp_header_->SetVisible(false);
  scrolling_settled_timer_.Reset();
  if (!should_header_animate_) {
    header_animation_restart_timer_.Reset();
  }
}

void CalendarView::RestoreMonthStatus() {
  current_month_->layer()->GetAnimator()->StopAnimating();
  current_label_->layer()->GetAnimator()->StopAnimating();
  previous_month_->layer()->GetAnimator()->StopAnimating();
  previous_label_->layer()->GetAnimator()->StopAnimating();
  next_label_->layer()->GetAnimator()->StopAnimating();
  next_month_->layer()->GetAnimator()->StopAnimating();
  next_next_label_->layer()->GetAnimator()->StopAnimating();
  next_next_month_->layer()->GetAnimator()->StopAnimating();
  ResetLayer(current_month_);
  ResetLayer(current_label_);
  ResetLayer(previous_label_);
  ResetLayer(previous_month_);
  ResetLayer(next_label_);
  ResetLayer(next_month_);
  ResetLayer(next_next_label_);
  ResetLayer(next_next_month_);

  if (!should_months_animate_) {
    months_animation_restart_timer_.Reset();
  }
}

void CalendarView::ScrollToToday() {
  base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);

  if (event_list_view_ || up_next_view_) {
    scroll_view_->ScrollToPosition(
        scroll_view_->vertical_scroll_bar(),
        PositionOfToday() + GetExpandedCalendarPadding());
    return;
  }

  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 PositionOfCurrentMonth());

  // If the screen does not have enough height which makes today's cell not in
  // the visible rect, we auto scroll to today's row instead of scrolling to the
  // first row of the current month.
  if (PositionOfCurrentMonth() +
          calendar_view_controller_->GetTodayRowBottomHeight() >
      scroll_view_->GetVisibleRect().bottom()) {
    scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                   PositionOfToday());
  }
}

bool CalendarView::IsDateCellViewFocused() {
  // For tests, in which the view is not in a Widget.
  if (!GetFocusManager()) {
    return false;
  }

  auto* focused_view = GetFocusManager()->GetFocusedView();
  if (!focused_view) {
    return false;
  }

  return views::IsViewClass<CalendarDateCellView>(focused_view);
}

bool CalendarView::IsAnimating() {
  return header_->layer()->GetAnimator()->is_animating() ||
         current_month_->layer()->GetAnimator()->is_animating() ||
         content_view_->layer()->GetAnimator()->is_animating() ||
         (event_list_view_ &&
          event_list_view_->layer()->GetAnimator()->is_animating()) ||
         calendar_sliding_surface_->layer()->GetAnimator()->is_animating();
}

void CalendarView::MaybeResetContentViewFocusBehavior() {
  if (IsDateCellViewFocused() ||
      content_view_->GetFocusBehavior() == FocusBehavior::ALWAYS) {
    return;
  }

  content_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
}

void CalendarView::OnViewBoundsChanged(views::View* observed_view) {
  // When in the tablet mode and the display rotates and the `event_list_view_`
  // is shown, `event_list_view_` should update its height to fill out the
  // remaining space.
  if (observed_view == this && event_list_view_) {
    SetCalendarSlidingSurfaceBounds(true);
    return;
  }

  // For screen density or orientation changes, we need to redraw the up next
  // views position and adjust the scroll view height accordingly.
  if (observed_view == this && up_next_view_) {
    SetCalendarSlidingSurfaceBounds(false);
    ClipScrollViewHeight(ScrollViewState::UP_NEXT_SHOWING);
    return;
  }

  if (observed_view != scroll_view_) {
    return;
  }

  // The CalendarView is created and lives without being added to the view tree
  // for a while. The first time OnViewBoundsChanged is called is the sign that
  // the view has actually been added to a view hierarchy, and it is time to
  // make some changes which depend on the view belonging to a widget.
  scoped_view_observer_.RemoveObservation(observed_view);

  // Initializes the view to auto scroll to `PositionOfToday` or the first row
  // of today's month.
  ScrollToToday();

  // If the view was shown via keyboard shortcut, the widget will be focusable.
  // Request focus to enable the user to quickly press enter to see todays
  // events. If the view was not shown via keyboard, this will be a no-op.
  RequestFocus();

  // Reset the timer here to invoke `UpdateOnScreenMonthMap()` manually after
  // 'kScrollingSettledTimeout` since layout will be finalized after a few
  // iterations and `on_screen_month_` only wants the final result.
  scrolling_settled_timer_.Reset();
}

void CalendarView::OnViewFocused(View* observed_view) {
  if (observed_view == this) {
    content_view_->RequestFocus();
    SetFocusBehavior(FocusBehavior::NEVER);
    return;
  }

  if (observed_view != content_view_ || IsDateCellViewFocused()) {
    return;
  }

  auto* focus_manager = GetFocusManager();
  previous_month_->EnableFocus();
  current_month_->EnableFocus();
  next_month_->EnableFocus();
  next_next_month_->EnableFocus();

  // If the event list is showing, focus on the first cell in the current row or
  // today's cell if today is in this row.
  if (event_list_view_) {
    focus_manager->SetFocusedView(
        current_month_->focused_cells()[calendar_view_controller_
                                            ->GetExpandedRowIndex()]);
    AdjustDateCellVoxBounds();

    content_view_->SetFocusBehavior(FocusBehavior::NEVER);
    return;
  }

  FocusPreferredDateCellViewOrFirstVisible(/*prefer_today=*/true);
}

views::View* CalendarView::AddLabelWithId(LabelType type, bool add_at_front) {
  auto label = std::make_unique<MonthHeaderLabelView>(
      type, calendar_view_controller_.get());
  if (add_at_front) {
    return content_view_->AddChildViewAt(std::move(label), 0);
  }
  return content_view_->AddChildView(std::move(label));
}

CalendarMonthView* CalendarView::AddMonth(base::Time month_first_date,
                                          bool add_at_front) {
  auto month = std::make_unique<CalendarMonthView>(
      month_first_date, calendar_view_controller_.get());
  month->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kMonthVerticalPadding, 0, kMonthVerticalPadding, 0)));
  if (add_at_front) {
    return content_view_->AddChildViewAt(std::move(month), 0);
  } else {
    return content_view_->AddChildView(std::move(month));
  }
}

void CalendarView::OnMonthChanged() {
  // The header animation without event list view is handled in the
  // `ScrollOneMonthWithAnimation` method.
  if (!should_header_animate_ || !event_list_view_) {
    UpdateHeaders();
    RestoreHeadersStatus();
    return;
  }

  set_should_header_animate(false);

  const std::u16string month =
      calendar_view_controller_->GetOnScreenMonthName();
  const std::u16string year = calendar_utils::GetYear(
      calendar_view_controller_->currently_shown_date());
  gfx::Transform header_moving = GetHeaderMovingAndPrepareAnimation(
      is_scrolling_up_, kOnMonthChangedAnimationHistogram, month, year);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<CalendarView> calendar_view) {
            if (!calendar_view) {
              return;
            }
            calendar_view->UpdateHeaders();
            calendar_view->temp_header_->SetVisible(false);
            calendar_view->header_->layer()->SetOpacity(1.0f);
            calendar_view->header_->layer()->SetTransform(gfx::Transform());
            calendar_view->set_should_header_animate(true);
            calendar_view->reset_scrolling_settled_timer();
          },
          weak_factory_.GetWeakPtr()))
      .OnAborted(base::BindOnce(
          [](base::WeakPtr<CalendarView> calendar_view) {
            if (!calendar_view) {
              return;
            }
            calendar_view->temp_header_->SetVisible(false);
            calendar_view->UpdateHeaders();
            calendar_view->RestoreHeadersStatus();
          },
          weak_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(calendar_utils::kAnimationDurationForMoving)
      .SetTransform(header_, header_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(temp_header_, gfx::Transform(), gfx::Tween::EASE_OUT_2)
      .At(kDelayHeaderAnimationDuration)
      .SetDuration(calendar_utils::kAnimationDurationForVisibility)
      .SetOpacity(header_, 0.0f)
      .At(kDelayHeaderAnimationDuration +
          calendar_utils::kAnimationDurationForVisibility)
      .SetDuration(calendar_utils::kAnimationDurationForVisibility)
      .SetOpacity(temp_header_, 1.0f);
}

void CalendarView::OnEventsFetched(
    const CalendarModel::FetchingStatus status,
    const base::Time start_time,
    const google_apis::calendar::EventList* events) {
  if (base::Contains(on_screen_month_, start_time)) {
    on_screen_month_[start_time] = status;
  }

  MaybeUpdateLoadingBarVisibility();

  // Only show up next for events that are the same month as `base::Time::Now`.
  if (start_time == calendar_utils::GetStartOfMonthUTC(
                        base::Time::NowFromSystemTime().UTCMidnight())) {
    MaybeShowUpNextView();
  }
}

void CalendarView::OnTimeout(const base::Time start_time) {
  if (base::Contains(on_screen_month_, start_time)) {
    on_screen_month_[start_time] = CalendarModel::kNever;
  }

  MaybeUpdateLoadingBarVisibility();
}

void CalendarView::OpenEventList() {
  // Don't show the the `event_list_` view for unlogged in users.
  if (!calendar_utils::ShouldFetchEvents()) {
    return;
  }

  // If the event list is already open or if any animation is occurring do not
  // let the user open the EventListView. It is ok to show the EventListView if
  // the animation cooldown is active.
  if (event_list_view_ || is_calendar_view_scrolling_ || IsAnimating()) {
    return;
  }

  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);

  // Updates `scroll_view_`'s accessible name with the selected date.
  absl::optional<base::Time> selected_date =
      calendar_view_controller_->selected_date();
  scroll_view_->GetViewAccessibility().OverrideName(l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_CONTENT_ACCESSIBLE_DESCRIPTION,
      calendar_utils::GetMonthNameAndYear(
          calendar_view_controller_->currently_shown_date()),
      calendar_utils::GetMonthDayYear(selected_date.value())));
  scroll_view_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged,
                                         /*send_native_event=*/true);

  event_list_view_ = calendar_sliding_surface_->AddChildView(
      std::make_unique<CalendarEventListView>(calendar_view_controller_.get()));
  event_list_view_->SetFocusBehavior(FocusBehavior::NEVER);

  const int previous_surface_y = calendar_sliding_surface_->y();
  SetCalendarSlidingSurfaceBounds(true);

  set_should_months_animate(false);
  gfx::Vector2dF moving_up_location = gfx::Vector2dF(
      0, -PositionOfSelectedDate() + scroll_view_->GetVisibleRect().y());

  gfx::Transform month_moving;
  month_moving.Translate(moving_up_location);

  // If the `up_next_view_` is showing, then we want to start the animation from
  // there, otherwise we start from the bottom of the screen.
  const int y_transform_start_position =
      up_next_view_ ? previous_surface_y - calendar_sliding_surface_->y()
                    : calendar_sliding_surface_->y();

  std::unique_ptr<ui::InterpolatedTranslation> list_view_sliding_up =
      std::make_unique<ui::InterpolatedTranslation>(
          gfx::PointF(0.f, y_transform_start_position), gfx::PointF());

  // Tracks animation smoothness. For now, we only track animation smoothness
  // for 1 month and 1 label since all 2 month views and 2 label views are
  // similar and perform the same animation. If this is not the case in the
  // future, we should add additional metrics for the rest.
  auto month_reporter = calendar_metrics::CreateAnimationReporter(
      current_month_, kMonthViewOpenEventListAnimationHistogram);
  auto label_reporter = calendar_metrics::CreateAnimationReporter(
      current_label_, kLabelViewOpenEventListAnimationHistogram);
  auto event_list_reporter = calendar_metrics::CreateAnimationReporter(
      event_list_view_, kEventListViewOpenEventListAnimationHistogram);
  auto calendar_sliding_surface_reporter =
      calendar_metrics::CreateAnimationReporter(
          calendar_sliding_surface_,
          kCalendarSlidingSurfaceOpenEventListAnimationHistogram);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&CalendarView::OnOpenEventListAnimationComplete,
                              weak_factory_.GetWeakPtr()))
      .OnAborted(base::BindOnce(&CalendarView::OnOpenEventListAnimationComplete,
                                weak_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(calendar_utils::kAnimationDurationForMoving)
      .SetTransform(current_month_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(current_label_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(next_label_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(next_month_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(next_next_label_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(next_next_month_, month_moving, gfx::Tween::EASE_OUT_2)
      .At(base::Milliseconds(0))
      .SetDuration(kAnimationDurationForEventsMoving)
      .SetInterpolatedTransform(calendar_sliding_surface_,
                                std::move(list_view_sliding_up),
                                gfx::Tween::EASE_IN_OUT_2);

  if (up_next_view_) {
    auto up_next_reporter = calendar_metrics::CreateAnimationReporter(
        up_next_view_, kUpNextViewOpenEventListAnimationHistogram);

    // Fade in `event_list_view_` and fade out `up_next_view_`.
    views::AnimationBuilder()
        .SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .Once()
        .SetOpacity(event_list_view_, 0.f)
        .SetOpacity(up_next_view_, 1.f)
        .At(base::Milliseconds(0))
        .SetDuration(kAnimationDurationForClosingEvents)
        .SetOpacity(event_list_view_, 1.f)
        .SetOpacity(up_next_view_, 0.f, gfx::Tween::EASE_IN);
  }
}

void CalendarView::CloseEventList() {
  // Don't allow the EventListView to close if an animation is
  // occurring. It is ok to animate the EventListView if the animation cooldown
  // is active.
  if (IsAnimating()) {
    return;
  }

  // Updates `scroll_view_`'s accessible name without the selected date.
  scroll_view_->GetViewAccessibility().OverrideName(l10n_util::GetStringFUTF16(
      IDS_ASH_CALENDAR_BUBBLE_ACCESSIBLE_DESCRIPTION,
      calendar_utils::GetMonthDayYearWeek(
          calendar_view_controller_->currently_shown_date())));
  scroll_view_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged,
                                         /*send_native_event=*/true);
  // Increase the scroll height before the animation starts, so that it's
  // already full height when animating the event list view sliding down.
  ClipScrollViewHeight(up_next_view_ ? ScrollViewState::UP_NEXT_SHOWING
                                     : ScrollViewState::FULL_HEIGHT);
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);

  // Move EventListView to the top of the up next view if showing, or off the
  // bottom of the CalendarView.
  const int previous_surface_y = calendar_sliding_surface_->y();
  SetCalendarSlidingSurfaceBounds(false);
  std::unique_ptr<ui::InterpolatedTranslation> list_view_sliding_down =
      std::make_unique<ui::InterpolatedTranslation>(
          gfx::PointF(0.f, previous_surface_y - calendar_sliding_surface_->y()),
          gfx::PointF());

  auto event_list_reporter = calendar_metrics::CreateAnimationReporter(
      event_list_view_, kCloseEventListAnimationHistogram);
  auto calendar_sliding_surface_reporter =
      calendar_metrics::CreateAnimationReporter(
          calendar_sliding_surface_,
          kCloseEventListCalendarSlidingSurfaceAnimationHistogram);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&CalendarView::OnCloseEventListAnimationComplete,
                              weak_factory_.GetWeakPtr()))
      .OnAborted(
          base::BindOnce(&CalendarView::OnCloseEventListAnimationComplete,
                         weak_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(kAnimationDurationForClosingEvents)
      .SetInterpolatedTransform(calendar_sliding_surface_,
                                std::move(list_view_sliding_down),
                                gfx::Tween::FAST_OUT_SLOW_IN)
      // Fade out the event list view.
      .At(kEventListAnimationStartDelay)
      .SetDuration(kAnimationDurationForClosingEvents)
      .SetOpacity(event_list_view_, 0.f, gfx::Tween::FAST_OUT_SLOW_IN);

  // Fade in the up next view.
  if (up_next_view_) {
    if (!up_next_view_->GetVisible()) {
      up_next_view_->SetVisible(true);
    }

    auto up_next_reporter = calendar_metrics::CreateAnimationReporter(
        up_next_view_, kCloseEventListUpNextViewAnimationHistogram);

    views::AnimationBuilder()
        .SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .Once()
        .SetOpacity(up_next_view_, 0.f)
        .At(kUpNextAnimationStartDelay)
        .SetDuration(kAnimationDurationForClosingEvents)
        .SetOpacity(up_next_view_, 1.f, gfx::Tween::EASE_OUT);
  }
}

void CalendarView::OnSelectedDateUpdated() {
  // If the event list is already open and the date cell is focused, moves the
  // focusing ring to the close button.
  if (event_list_view_ && IsDateCellViewFocused()) {
    RequestFocusForEventListCloseButton();
  }
}

void CalendarView::OnCalendarLoaded() {
  // We might have some cached upcoming events so we can show the
  // `up_next_view_` as soon as the calendar has loaded i.e. before waiting for
  // the event fetch to complete.
  MaybeShowUpNextView();
}

void CalendarView::ScrollUpOneMonth() {
  is_scrolling_up_ = true;
  calendar_view_controller_->UpdateMonth(
      calendar_view_controller_->GetPreviousMonthFirstDayUTC(1));
  content_view_->RemoveChildViewT(next_next_label_.get());
  content_view_->RemoveChildViewT(next_next_month_.get());

  next_next_label_ = next_label_;
  next_next_month_ = next_month_;
  next_label_ = current_label_;
  next_month_ = current_month_;
  current_label_ = previous_label_;
  current_month_ = previous_month_;

  previous_month_ =
      AddMonth(calendar_view_controller_->GetPreviousMonthFirstDayUTC(1),
               /*add_at_front=*/true);
  if (IsDateCellViewFocused()) {
    previous_month_->EnableFocus();
  }
  previous_label_ = AddLabelWithId(LabelType::PREVIOUS,
                                   /*add_at_front=*/true);

  // After adding a new month in the content, the current position stays the
  // same but below the added view the each view's position has changed to
  // [original position + new month's height]. So we need to add the height of
  // the newly added month to keep the current view's position.
  int added_height = previous_month_->GetPreferredSize().height() +
                     previous_label_->GetPreferredSize().height();
  int position = added_height + scroll_view_->GetVisibleRect().y();

  base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(), position);

  MaybeResetContentViewFocusBehavior();

  if (current_month_->has_events()) {
    calendar_view_controller_->EventsDisplayedToUser();
  }
}

void CalendarView::ScrollDownOneMonth() {
  is_scrolling_up_ = false;
  // Renders the next month if the next month label is moving up and passing
  // the top of the visible area, or the next month body's bottom is passing
  // the bottom of the visible area.
  int removed_height = previous_month_->GetPreferredSize().height() +
                       previous_label_->GetPreferredSize().height();

  calendar_view_controller_->UpdateMonth(
      calendar_view_controller_->GetNextMonthFirstDayUTC(1));

  content_view_->RemoveChildViewT(previous_label_.get());
  content_view_->RemoveChildViewT(previous_month_.get());

  previous_label_ = current_label_;
  previous_month_ = current_month_;
  current_label_ = next_label_;
  current_month_ = next_month_;
  next_label_ = next_next_label_;
  next_month_ = next_next_month_;

  next_next_label_ = AddLabelWithId(LabelType::NEXTNEXT);
  next_next_month_ = AddMonth(
      calendar_view_controller_->GetNextMonthFirstDayUTC(/*num_months=*/2));
  if (IsDateCellViewFocused()) {
    next_next_month_->EnableFocus();
  }

  // Same as adding previous views. We need to remove the height of the
  // deleted month to keep the current view's position.
  int position = scroll_view_->GetVisibleRect().y() - removed_height;

  base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(), position);

  MaybeResetContentViewFocusBehavior();

  if (current_month_->has_events()) {
    calendar_view_controller_->EventsDisplayedToUser();
  }
}

void CalendarView::ScrollOneMonthAndAutoScroll(bool scroll_up) {
  if (is_resetting_scroll_) {
    return;
  }

  base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);
  RestoreMonthStatus();
  if (scroll_up) {
    ScrollUpOneMonth();
  } else {
    ScrollDownOneMonth();
  }
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 PositionOfCurrentMonth());
}

void CalendarView::ScrollOneMonthWithAnimation(bool scroll_up) {
  user_has_scrolled_ = true;
  is_scrolling_up_ = scroll_up;

  if (event_list_view_) {
    // If it is animating to open this `event_list_view_`, disable the up/down
    // buttons.
    if (!should_months_animate_ || !should_header_animate_) {
      return;
    }
    ScrollOneRowWithAnimation(scroll_up);
    return;
  }

  // If there's already an existing animation, restores each layer's visibility
  // and position.
  if (!should_months_animate_ || !should_header_animate_) {
    set_should_months_animate(false);
    set_should_header_animate(false);
    RestoreHeadersStatus();
    is_resetting_scroll_ = false;
    scroll_view_->SetVerticalScrollBarMode(
        views::ScrollView::ScrollBarMode::kHiddenButEnabled);
    ScrollOneMonthAndAutoScroll(scroll_up);
    return;
  }

  if (is_resetting_scroll_) {
    return;
  }

  // Starts to show the month and header animation.
  SetShouldMonthsAnimateAndScrollEnabled(false);
  set_should_header_animate(false);

  gfx::Vector2dF moving_up_location = gfx::Vector2dF(
      0, previous_month_->GetPreferredSize().height() +
             current_label_->GetPreferredSize().height() +
             (scroll_view_->GetVisibleRect().y() - current_month_->y()));
  gfx::Vector2dF moving_down_location = gfx::Vector2dF(
      0, -current_month_->GetPreferredSize().height() -
             next_label_->GetPreferredSize().height() +
             (scroll_view_->GetVisibleRect().y() - current_month_->y()));
  gfx::Transform month_moving;
  month_moving.Translate(scroll_up ? moving_up_location : moving_down_location);

  const std::u16string temp_month =
      scroll_up ? calendar_view_controller_->GetPreviousMonthName()
                : calendar_view_controller_->GetNextMonthName();
  const std::u16string temp_year = calendar_utils::GetYear(
      scroll_up ? calendar_view_controller_->GetPreviousMonthFirstDayUTC(
                      /*num_months=*/1)
                : calendar_view_controller_->GetNextMonthFirstDayUTC(
                      /*num_months=*/1));
  gfx::Transform header_moving = GetHeaderMovingAndPrepareAnimation(
      scroll_up, kHeaderViewScrollOneMonthAnimationHistogram, temp_month,
      temp_year);

  // Tracks animation smoothness. For now, we only track animation smoothness
  // for 1 month and 1 label since all 3 month views and 3 label views are
  // similar and perform the same animation. If this is not the case in the
  // future, we should add additional metrics for the rest.
  auto month_reporter = calendar_metrics::CreateAnimationReporter(
      current_month_, kMonthViewScrollOneMonthAnimationHistogram);
  auto label_reporter = calendar_metrics::CreateAnimationReporter(
      current_label_, kLabelViewScrollOneMonthAnimationHistogram);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&CalendarView::OnScrollMonthAnimationComplete,
                              weak_factory_.GetWeakPtr(), scroll_up))
      .OnAborted(base::BindOnce(&CalendarView::OnScrollMonthAnimationComplete,
                                weak_factory_.GetWeakPtr(), scroll_up))
      .Once()
      .SetDuration(calendar_utils::kAnimationDurationForMonthMoving)
      .SetTransform(current_month_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(current_label_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(previous_month_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(previous_label_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(next_month_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(next_label_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(next_next_month_, month_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(next_next_label_, month_moving, gfx::Tween::EASE_OUT_2)
      .At(kDelayHeaderAnimationDuration)
      .SetDuration(calendar_utils::kAnimationDurationForMoving)
      .SetTransform(header_, header_moving, gfx::Tween::EASE_OUT_2)
      .SetTransform(temp_header_, gfx::Transform(), gfx::Tween::EASE_OUT_2)
      .At(kDelayHeaderAnimationDuration)
      .SetDuration(calendar_utils::kAnimationDurationForVisibility)
      .SetOpacity(header_, 0.0f)
      .At(kDelayHeaderAnimationDuration +
          calendar_utils::kAnimationDurationForVisibility)
      .SetDuration(calendar_utils::kAnimationDurationForVisibility)
      .SetOpacity(temp_header_, 1.0f);
}

gfx::Transform CalendarView::GetHeaderMovingAndPrepareAnimation(
    bool scroll_up,
    const std::string& animation_name,
    const std::u16string& temp_month,
    const std::u16string& temp_year) {
  const int header_height = header_->GetPreferredSize().height();
  const gfx::Vector2dF header_moving_location =
      gfx::Vector2dF(0, (header_height / 2) * (scroll_up ? 1 : -1));
  gfx::Transform header_moving;
  header_moving.Translate(header_moving_location);

  // Tracks animation smoothness.
  auto header_reporter =
      calendar_metrics::CreateAnimationReporter(header_, animation_name);

  // Update the temp header label with the new header's month and year.
  temp_header_->UpdateHeaders(temp_month, temp_year);
  temp_header_->layer()->SetOpacity(0.0f);
  gfx::Transform initial_state;
  initial_state.Translate(
      gfx::Vector2dF(0, (header_height / 2) * (scroll_up ? -1 : 1)));
  temp_header_->layer()->SetTransform(initial_state);
  temp_header_->SetVisible(true);

  return header_moving;
}

void CalendarView::ScrollOneRowWithAnimation(bool scroll_up) {
  if (is_resetting_scroll_) {
    return;
  }

  is_scrolling_up_ = scroll_up;
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);

  // Scrolls to the last row of the previous month if it's currently on the
  // first row and scrolling up.
  if (scroll_up && calendar_view_controller_->GetExpandedRowIndex() == 0) {
    ScrollUpOneMonth();
    SetExpandedRowThenDisableScroll(current_month_->last_row_index());
    return;
  }

  // Scrolls to the first row of the next month if it's currently on the
  // last row and scrolling down.
  if (!scroll_up && calendar_view_controller_->GetExpandedRowIndex() ==
                        current_month_->last_row_index()) {
    ScrollDownOneMonth();
    SetExpandedRowThenDisableScroll(0);
    return;
  }

  SetExpandedRowThenDisableScroll(
      calendar_view_controller_->GetExpandedRowIndex() + (scroll_up ? -1 : 1));
  return;
}

void CalendarView::OnEvent(ui::Event* event) {
  if (!event->IsKeyEvent()) {
    TrayDetailedView::OnEvent(event);
    return;
  }

  auto* key_event = event->AsKeyEvent();
  auto key_code = key_event->key_code();
  auto* focus_manager = GetFocusManager();

  bool is_tab_key_pressed =
      key_event->type() == ui::EventType::ET_KEY_PRESSED &&
      views::FocusManager::IsTabTraversalKeyEvent(*key_event);

  if (is_tab_key_pressed) {
    RecordCalendarKeyboardNavigation(
        calendar_metrics::CalendarKeyboardNavigationSource::kTab);
  }

  if (!IsDateCellViewFocused()) {
    if (is_tab_key_pressed) {
      // If the current focused view is the last focusable view in the focus
      // list, then make an attempt to navigate to the previous widget (most
      // likely to the message center). Stop the propagation of the event if
      // the attempt was successful.
      const auto* next_view = focus_manager->GetNextFocusableView(
          focus_manager->GetFocusedView(), GetWidget(),
          /*reverse=*/key_event->IsShiftDown(),
          /*dont_loop=*/true);
      auto* unified_system_tray_bubble =
          RootWindowController::ForWindow(GetWidget()->GetNativeWindow())
              ->GetStatusAreaWidget()
              ->unified_system_tray()
              ->bubble();

      if (!next_view && unified_system_tray_bubble &&
          unified_system_tray_bubble->unified_system_tray_controller()
              ->FocusOut(/*reverse=*/true)) {
        event->StopPropagation();
      }
    }
    TrayDetailedView::OnEvent(event);
    return;
  }

  // When tab key is pressed, stops focusing on any `CalendarDateCellView` and
  // goes to the next focusable button in the header.
  if (is_tab_key_pressed) {
    // Focus the whole scroll view so `focus_manager->AdvanceFocus()` moves the
    // focus out of the scroll view, to the next view in the focus order. This
    // avoids focusing the next date cell view.
    scroll_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
    scroll_view_->RequestFocus();

    current_month_->DisableFocus();
    previous_month_->DisableFocus();
    next_month_->DisableFocus();
    next_next_month_->DisableFocus();

    TrayDetailedView::OnEvent(event);

    // Should move the focus out of the scroll view (the whole scroll view
    // temporarily grabbed focus in place of the initially focused date cell
    // view).
    focus_manager->AdvanceFocus(/*reverse=*/key_event->IsShiftDown());
    scroll_view_->SetFocusBehavior(FocusBehavior::NEVER);
    event->StopPropagation();
    content_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
    return;
  }

  if (key_event->type() != ui::EventType::ET_KEY_PRESSED ||
      (key_code != ui::VKEY_UP && key_code != ui::VKEY_DOWN &&
       key_code != ui::VKEY_LEFT && key_code != ui::VKEY_RIGHT)) {
    TrayDetailedView::OnEvent(event);
    return;
  }

  switch (key_code) {
    case ui::VKEY_UP:
    case ui::VKEY_DOWN: {
      RecordCalendarKeyboardNavigation(
          calendar_metrics::CalendarKeyboardNavigationSource::kArrowKeys);

      auto* current_focusable_view = focus_manager->GetFocusedView();
      // Enable the scroll bar mode, in case it is disabled when the event list
      // is showing.
      scroll_view_->SetVerticalScrollBarMode(
          views::ScrollView::ScrollBarMode::kHiddenButEnabled);

      // Moving 7 (`kDateInOneWeek`) steps will focus on the cell which is right
      // above or below the current cell, since each row has 7 days.
      for (int count = 0; count < calendar_utils::kDateInOneWeek; count++) {
        auto* next_focusable_view = focus_manager->GetNextFocusableView(
            current_focusable_view, GetWidget(),
            /*reverse=*/key_code == ui::VKEY_UP,
            /*dont_loop=*/false);
        current_focusable_view = next_focusable_view;
        // Sometimes the position of the upper row cells, which should be
        // focused next, are above (and hidden behind) the header buttons. So
        // this loop skips those buttons.
        while (
            current_focusable_view &&
            !views::IsViewClass<CalendarDateCellView>(current_focusable_view)) {
          current_focusable_view = focus_manager->GetNextFocusableView(
              current_focusable_view, GetWidget(),
              /*reverse=*/key_code == ui::VKEY_UP,
              /*dont_loop=*/false);
        }
      }
      focus_manager->SetFocusedView(current_focusable_view);

      // After focusing on the new cell the view should have scrolled already
      // if needed, but there's an offset compared with scrolled by
      // `ScrollOneRowWithAnimation`. Manually scroll the view then disable the
      // scroll bar mode if the even list is showing.
      if (event_list_view_) {
        const int current_height =
            scroll_view_->GetVisibleRect().y() - PositionOfCurrentMonth();
        SetExpandedRowThenDisableScroll(
            current_height / calendar_view_controller_->row_height());
      }

      AdjustDateCellVoxBounds();

      return;
    }
    case ui::VKEY_LEFT:
    case ui::VKEY_RIGHT: {
      RecordCalendarKeyboardNavigation(
          calendar_metrics::CalendarKeyboardNavigationSource::kArrowKeys);

      // Enable the scroll bar mode, in case it is disabled when the event list
      // is showing.
      scroll_view_->SetVerticalScrollBarMode(
          views::ScrollView::ScrollBarMode::kHiddenButEnabled);
      bool is_reverse = base::i18n::IsRTL() ? key_code == ui::VKEY_RIGHT
                                            : key_code == ui::VKEY_LEFT;
      focus_manager->AdvanceFocus(/*reverse=*/is_reverse);

      // After focusing on the new cell the view should have scrolled already
      // if needed, but there's an offset compared with scrolled by
      // `ScrollOneRowWithAnimation`. Manually scroll the view then disable the
      // scroll bar mode if the even list is showing.
      if (event_list_view_) {
        const int current_height =
            scroll_view_->GetVisibleRect().y() - PositionOfCurrentMonth();
        SetExpandedRowThenDisableScroll(
            current_height / calendar_view_controller_->row_height());
      }

      AdjustDateCellVoxBounds();

      return;
    }
    default:
      NOTREACHED();
  }
}

void CalendarView::SetExpandedRowThenDisableScroll(int row_index) {
  DCHECK(event_list_view_);
  calendar_view_controller_->set_expanded_row_index(row_index);

  const int row_height = calendar_view_controller_->GetExpandedRowIndex() *
                         calendar_view_controller_->row_height();
  scroll_view_->ScrollToPosition(
      scroll_view_->vertical_scroll_bar(),
      PositionOfCurrentMonth() + row_height + GetExpandedCalendarPadding());

  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
}

void CalendarView::OnContentsScrolled() {
  base::AutoReset<bool> set_is_scrolling(&is_calendar_view_scrolling_, true);

  // The scroll position is reset because it's adjusting the position when
  // adding or removing views from the `scroll_view_`. It should scroll to the
  // position we want, so we don't need to check the visible area position.
  if (is_resetting_scroll_) {
    return;
  }

  user_has_scrolled_ = true;

  base::AutoReset<bool> disable_header_animation(&should_header_animate_,
                                                 false);

  // Reset the timer to update the `on_screen_month_` map after scrolling.
  scrolling_settled_timer_.Reset();

  // Scrolls to the previous month if the current label is moving down and
  // passing the top of the visible area.
  if (scroll_view_->GetVisibleRect().y() <= current_label_->y()) {
    ScrollUpOneMonth();
  } else if (scroll_view_->GetVisibleRect().y() >= next_label_->y()) {
    ScrollDownOneMonth();
  }
}

void CalendarView::OnMonthArrowButtonActivated(bool up,
                                               const ui::Event& event) {
  calendar_metrics::RecordMonthArrowButtonActivated(up, event);

  ScrollOneMonthWithAnimation(up);
  content_view_->OnMonthChanged();
}

void CalendarView::AdjustDateCellVoxBounds() {
  auto* focused_view = GetFocusManager()->GetFocusedView();
  DCHECK(views::IsViewClass<CalendarDateCellView>(focused_view));

  // When the Chrome Vox focusing box is in a `ScrollView`, the hidden content
  // height, which is `scroll_view_->GetVisibleRect().y()` should also be added.
  // Otherwise the position of the Chrome Vox box is off.
  gfx::Rect bounds = focused_view->GetBoundsInScreen();
  focused_view->GetViewAccessibility().OverrideBounds(
      gfx::RectF(bounds.x(), bounds.y() + scroll_view_->GetVisibleRect().y(),
                 bounds.width(), bounds.height()));
}

void CalendarView::OnScrollMonthAnimationComplete(bool scroll_up) {
  set_should_header_animate(true);
  SetShouldMonthsAnimateAndScrollEnabled(true);
  ScrollOneMonthAndAutoScroll(scroll_up);
  temp_header_->SetVisible(false);
  header_->layer()->SetOpacity(1.0f);
  header_->layer()->SetTransform(gfx::Transform());
}

void CalendarView::OnOpenEventListAnimationComplete() {
  if (is_destroying_) {
    return;
  }

  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  // Scrolls to the next month if the selected date is in the `next_month_`, so
  // that the `current_month_`is updated to the next month.
  if (!calendar_view_controller_->IsSelectedDateInCurrentMonth()) {
    ScrollDownOneMonth();
  }
  // If still not in this month, it's in the `next_next_month_`. Doing this in a
  // while loop may cause a potential infinite loop. For example when the time
  // difference is not calculated or applied correctly, which may cause some
  // dates cannot be found in the months.
  if (!calendar_view_controller_->IsSelectedDateInCurrentMonth()) {
    ScrollDownOneMonth();
  }
  base::AutoReset<bool> is_resetting_scrolling(&is_resetting_scroll_, true);
  RestoreMonthStatus();
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 PositionOfSelectedDate());
  // Clip the height to a bit more than the height of a row.
  ClipScrollViewHeight(ScrollViewState::EVENT_LIST_SHOWING);

  if (up_next_view_) {
    // Once the animation is complete, the `up_next_view_` needs to be invisible
    // otherwise ChromeVox will pick it up.
    up_next_view_->SetVisible(false);
  }

  if (!should_months_animate_) {
    months_animation_restart_timer_.Reset();
  }
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  calendar_view_controller_->OnEventListOpened();

  // Moves focusing ring to the close button of the event list if it's opened
  // from the date cell view focus or from the `up_next_view_`.
  if (IsDateCellViewFocused() || up_next_view_) {
    RequestFocusForEventListCloseButton();
  }

  up_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_CALENDAR_UP_BUTTON_EVENT_LIST_ACCESSIBLE_DESCRIPTION));
  down_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_CALENDAR_DOWN_BUTTON_EVENT_LIST_ACCESSIBLE_DESCRIPTION));
}

void CalendarView::OnCloseEventListAnimationComplete() {
  if (is_destroying_) {
    return;
  }

  // GetFocusManager() can be nullptr if `CalendarView` is destroyed when the
  // closing animation hasn't finished.
  auto* focused_view =
      GetFocusManager() ? GetFocusManager()->GetFocusedView() : nullptr;

  // Restore focus before removing `event_list_view_`. This is necessary because
  // showing `event_list_view_` scrolls the `scroll_view_` with custom padding
  // which is hard to detect after the fact. If `event_list_view_` doesn't
  // exist, it's not clear the padding exists, and this can result in the wrong
  // CalendarDateCellView being focused.
  if (focused_view && Contains(focused_view)) {
    FocusPreferredDateCellViewOrFirstVisible(/*prefer_today=*/false);
  }
  calendar_sliding_surface_->RemoveChildViewT(event_list_view_.get());
  event_list_view_ = nullptr;
  calendar_view_controller_->OnEventListClosed();

  MaybeShowUpNextView();

  up_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_CALENDAR_UP_BUTTON_ACCESSIBLE_DESCRIPTION));
  down_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_CALENDAR_DOWN_BUTTON_ACCESSIBLE_DESCRIPTION));
}

void CalendarView::RequestFocusForEventListCloseButton() {
  DCHECK(event_list_view_);
  event_list_view_->RequestCloseButtonFocus();
  current_month_->DisableFocus();
  previous_month_->DisableFocus();
  next_month_->DisableFocus();
  next_next_month_->DisableFocus();
  content_view_->SetFocusBehavior(FocusBehavior::ALWAYS);
}

void CalendarView::OnResetToTodayAnimationComplete() {
  SetShouldMonthsAnimateAndScrollEnabled(/*enabled=*/true);
  ResetToToday();
  FadeInCurrentMonth();
  // There's a corner case when the `current_month_` doesn't change,
  // the `on_screen_month_` map won't be updated since
  // `OnMonthChanged` won't be called and the timer won't be reset. So
  // we manually call the timer to update `on_screen_month_`.
  reset_scrolling_settled_timer();
}

void CalendarView::OnResetToTodayFadeInAnimationComplete() {
  set_should_months_animate(true);
  set_should_header_animate(true);
  is_resetting_scroll_ = false;
  scroll_view_->SetVerticalScrollBarMode(
      event_list_view_ ? views::ScrollView::ScrollBarMode::kDisabled
                       : views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  SetHeaderAndContentViewOpacity(/*opacity=*/1.0f);
}

void CalendarView::FocusPreferredDateCellViewOrFirstVisible(bool prefer_today) {
  previous_month_->EnableFocus();
  current_month_->EnableFocus();
  next_month_->EnableFocus();
  next_next_month_->EnableFocus();

  CalendarDateCellView* to_be_focused_cell =
      GetTargetDateCellViewOrFirstFocusable(
          prefer_today ? calendar_view_controller_->todays_date_cell_view()
                       : calendar_view_controller_->selected_date_cell_view());
  if (to_be_focused_cell) {
    to_be_focused_cell->SetFirstOnFocusedAccessibilityLabel();
    GetFocusManager()->SetFocusedView(to_be_focused_cell);
  } else {
    // If there's no visible row of the current month on the screen, focus on
    // the first visible non-grayed-out date of the next month.
    GetFocusManager()->SetFocusedView(next_month_->focused_cells().front());
  }

  AdjustDateCellVoxBounds();

  content_view_->SetFocusBehavior(FocusBehavior::NEVER);
}

CalendarDateCellView* CalendarView::GetTargetDateCellViewOrFirstFocusable(
    CalendarDateCellView* target_date_cell_view) {
  // When focusing on the `content_view_`, we decide which is the to-be-focused
  // cell based on the current position.
  const int visible_window_y_in_content_view =
      scroll_view_->GetVisibleRect().y();
  const int row_height = calendar_view_controller_->row_height();

  // Check whether at least one row of the current month is visible on the
  // screen. The to-be-focused cell should be the first non-grayed date cell
  // that is visible, or today's cell if today is in the current month and
  // visible.
  if (visible_window_y_in_content_view >=
      (next_label_->y() - row_height - kMonthVerticalPadding -
       kLabelVerticalPadding)) {
    return nullptr;
  }

  const int first_visible_row = CalculateFirstFullyVisibleRow();
  if (target_date_cell_view &&
      (current_month_ == target_date_cell_view->parent()) &&
      (first_visible_row <= target_date_cell_view->row_index())) {
    return target_date_cell_view;
  }
  return current_month_->focused_cells()[first_visible_row];
}

int CalendarView::CalculateFirstFullyVisibleRow() {
  const int visible_window_y_in_content_view =
      scroll_view_->GetVisibleRect().y();
  int row_index = 0;

  // Get first visible row index. If `event_list_view_` is showing, account
  // for the extra padding added to `scroll_view_`'s visible window.
  while (visible_window_y_in_content_view >
         (PositionOfCurrentMonth() +
          row_index * calendar_view_controller_->row_height() +
          (event_list_view_ ? GetExpandedCalendarPadding() : 0))) {
    ++row_index;
    if (row_index > kMaxRowsInOneMonth) {
      NOTREACHED() << "CalendarMonthView's cannot have more than "
                   << kMaxRowsInOneMonth << " rows.";
      return kMaxRowsInOneMonth;
    }
  }
  return row_index;
}

void CalendarView::SetCalendarSlidingSurfaceBounds(bool event_list_view_open) {
  const int x_position = scroll_view_->x() + kEventListViewHorizontalOffset;
  const int width = scroll_view_->GetVisibleRect().width() -
                    kEventListViewHorizontalOffset * 2;
  const int event_list_view_height = GetBoundsInScreen().bottom() -
                                     scroll_view_->GetBoundsInScreen().y() -
                                     GetSingleVisibleRowHeight();

  // If the event list view is showing, position the calendar sliding surface
  // where the opened event list view will be.
  if (event_list_view_open) {
    calendar_sliding_surface_->SetBounds(
        x_position, scroll_view_->y() + GetSingleVisibleRowHeight(), width,
        event_list_view_height);
    return;
  }

  // If the event list view is not showing and the up next view is showing,
  // position the calendar sliding surface where the up next view will be.
  if (up_next_view_) {
    const int up_next_view_preferred_height =
        up_next_view_->GetPreferredSize().height();
    calendar_sliding_surface_->SetBounds(
        x_position, GetLocalBounds().bottom() - up_next_view_preferred_height,
        width, event_list_view_height);
    return;
  }

  // If neither event list nor up next are showing, position the calendar
  // sliding surface off the bottom of the screen.
  calendar_sliding_surface_->SetBounds(x_position, GetVisibleBounds().bottom(),
                                       width, event_list_view_height);
}

void CalendarView::MaybeShowUpNextView() {
  if (calendar_view_controller_->UpcomingEvents().empty()) {
    RemoveUpNextView();
    return;
  }

  if (up_next_view_) {
    up_next_view_->RefreshEvents();
    return;
  }

  // If the `event_list_view_` is currently showing and the `up_next_view_` is
  // not, then early return. In this scenario we want the up next view to be
  // there when the user closes the `event_list_view_` but we don't want all the
  // scrollview clipping or bounds changing to happen.
  if (event_list_view_) {
    return;
  }

  up_next_view_ = calendar_sliding_surface_->AddChildView(
      std::make_unique<CalendarUpNextView>(
          calendar_view_controller_.get(),
          base::BindRepeating(&CalendarView::OpenEventListForTodaysDate,
                              base::Unretained(this))));

  // If the event list and up next views aren't currently displayed, then
  // construct the view and put the calendar into the state of showing the up
  // next view.
  ClipScrollViewHeight(ScrollViewState::UP_NEXT_SHOWING);
  SetCalendarSlidingSurfaceBounds(false);

  // Translate the up next view `kUpNextAnimationYOffset` off the screen and
  // animate sliding up.
  std::unique_ptr<ui::InterpolatedTranslation> up_next_sliding_up =
      std::make_unique<ui::InterpolatedTranslation>(
          gfx::PointF(0.f, kUpNextAnimationYOffset), gfx::PointF());

  auto up_next_view_reporter = calendar_metrics::CreateAnimationReporter(
      up_next_view_, kShowUpNextViewAnimationHistogram);

  // Animate the `up_next_view_` in.
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnAborted(base::BindOnce(&CalendarView::OnShowUpNextAnimationEnded,
                                weak_factory_.GetWeakPtr()))
      .OnEnded(base::BindOnce(&CalendarView::OnShowUpNextAnimationEnded,
                              weak_factory_.GetWeakPtr()))
      .Once()
      .SetOpacity(up_next_view_, 0.f)
      .At(base::Milliseconds(0))
      .SetDuration(kAnimationDurationForClosingEvents)
      .SetOpacity(up_next_view_, 1.f)
      .SetInterpolatedTransform(calendar_sliding_surface_,
                                std::move(up_next_sliding_up),
                                gfx::Tween::FAST_OUT_SLOW_IN_2);
}

void CalendarView::OnShowUpNextAnimationEnded() {
  // If todays date cell is null or the user has scrolled at all, then don't
  // auto scroll.
  if (!calendar_view_controller_->todays_date_cell_view() ||
      user_has_scrolled_) {
    return;
  }

  // If todays date cell is not visible in the `scroll_view_`, i.e. it's hidden
  // behind the up next view, then smooth scroll to it.
  if (!scroll_view_->GetBoundsInScreen().Intersects(
          calendar_view_controller_->todays_date_cell_view()
              ->GetBoundsInScreen())) {
    const int offset = calendar_view_controller_->todays_date_cell_view()
                           ->GetBoundsInScreen()
                           .bottom() -
                       calendar_sliding_surface_->GetBoundsInScreen().y();
    AnimateScrollByOffset(offset);
  }
}

void CalendarView::RemoveUpNextView() {
  if (!up_next_view_) {
    return;
  }

  calendar_sliding_surface_->RemoveChildViewT(up_next_view_.get());
  up_next_view_ = nullptr;

  SetCalendarSlidingSurfaceBounds(event_list_view_);
  ClipScrollViewHeight(event_list_view_ ? ScrollViewState::EVENT_LIST_SHOWING
                                        : ScrollViewState::FULL_HEIGHT);
}

void CalendarView::OpenEventListForTodaysDate() {
  const auto upcoming_events = calendar_view_controller_->UpcomingEvents();
  const base::Time upcoming_event_start_time =
      !upcoming_events.empty() ? upcoming_events.back().start_time().date_time()
                               : base::Time::Now();

  if (!current_month_->has_today()) {
    ResetToToday();
  }

  calendar_view_controller_->ShowEventListView(
      /*selected_calendar_date_cell_view=*/calendar_view_controller_
          ->todays_date_cell_view(),
      /*selected_date=*/upcoming_event_start_time,
      /*row_index=*/calendar_view_controller_->today_row() - 1);
}

void CalendarView::ClipScrollViewHeight(ScrollViewState state_to_change_to) {
  switch (state_to_change_to) {
    case ScrollViewState::FULL_HEIGHT:
      scroll_view_->ClipHeightTo(0, INT_MAX);
      break;
    case ScrollViewState::UP_NEXT_SHOWING:
      scroll_view_->ClipHeightTo(
          0, GetBoundsInScreen().bottom() -
                 scroll_view_->GetBoundsInScreen().y() -
                 up_next_view_->GetPreferredSize().height() +
                 calendar_utils::kUpNextOverlapInPx);
      break;
    case ScrollViewState::EVENT_LIST_SHOWING:
      scroll_view_->ClipHeightTo(0, GetSingleVisibleRowHeight());
      break;
  }
}

void CalendarView::AnimateScrollByOffset(int offset) {
  if (offset == 0) {
    return;
  }

  if (IsAnimating()) {
    RestoreMonthStatus();
    scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                   scroll_view_->GetVisibleRect().y() + offset);
    return;
  }

  SetShouldMonthsAnimateAndScrollEnabled(false);
  gfx::Vector2dF moving_up_location = gfx::Vector2dF(0, -offset);
  gfx::Transform month_moving;
  month_moving.Translate(moving_up_location);

  auto month_reporter = calendar_metrics::CreateAnimationReporter(
      current_month_, kSmoothScrollMonthViewWhenShowingTodaysDateCell);
  auto label_reporter = calendar_metrics::CreateAnimationReporter(
      current_label_, kSmoothScrollLabelViewWhenShowingTodaysDateCell);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&CalendarView::OnAnimateScrollByOffsetComplete,
                              weak_factory_.GetWeakPtr(), offset))
      .OnAborted(base::BindOnce(&CalendarView::OnAnimateScrollByOffsetComplete,
                                weak_factory_.GetWeakPtr(), offset))
      .Once()
      .SetDuration(kAnimationDurationForClosingEvents)
      .SetTransform(current_month_, month_moving,
                    gfx::Tween::ACCEL_20_DECEL_100)
      .SetTransform(current_label_, month_moving,
                    gfx::Tween::ACCEL_20_DECEL_100)
      .SetTransform(next_label_, month_moving, gfx::Tween::ACCEL_20_DECEL_100)
      .SetTransform(next_month_, month_moving, gfx::Tween::ACCEL_20_DECEL_100)
      .SetTransform(next_next_label_, month_moving,
                    gfx::Tween::ACCEL_20_DECEL_100)
      .SetTransform(next_next_month_, month_moving,
                    gfx::Tween::ACCEL_20_DECEL_100);
}

void CalendarView::OnAnimateScrollByOffsetComplete(int offset) {
  if (is_destroying_) {
    return;
  }

  SetShouldMonthsAnimateAndScrollEnabled(true);
  RestoreMonthStatus();
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 scroll_view_->GetVisibleRect().y() + offset);
}

int CalendarView::GetSingleVisibleRowHeight() {
  return calendar_view_controller_->row_height() +
         kCalendarEventListViewOpenMargin;
}

BEGIN_METADATA(CalendarView, GlanceableTrayChildBubble)
END_METADATA

}  // namespace ash
