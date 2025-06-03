// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/actions_example.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textarea/textarea.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/examples/example_combobox_model.h"
#include "ui/views/examples/examples_action_id.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"

namespace views::examples {

class ActionButton : public MdTextButton {
 public:
  METADATA_HEADER(ActionButton);
  explicit ActionButton(actions::ActionItem* action_item = nullptr,
                        int button_context = style::CONTEXT_BUTTON_MD);
  ActionButton(const ActionButton&) = delete;
  ActionButton& operator=(const ActionButton&) = delete;
  ~ActionButton() override = default;
  actions::ActionItem* GetActionItem() const;
  void SetActionItem(actions::ActionItem* action_item);
  std::u16string GetName() const;
  void SetName(const std::u16string& name);

 private:
  void ActionItemChanged();
  void TriggerAction();
  std::u16string name_;
  raw_ptr<actions::ActionItem> action_item_ = nullptr;
  base::CallbackListSubscription action_changed_subscription_;
};

ActionButton::ActionButton(actions::ActionItem* action_item, int button_context)
    : MdTextButton(base::BindRepeating(&ActionButton::TriggerAction,
                                       base::Unretained(this)),
                   std::u16string(),
                   button_context) {
  SetActionItem(action_item);
}

actions::ActionItem* ActionButton::GetActionItem() const {
  return action_item_.get();
}

void ActionButton::SetActionItem(actions::ActionItem* action_item) {
  if (action_item_.get() == action_item) {
    return;
  }
  action_item_ = action_item;
  action_changed_subscription_ = {};
  if (action_item_) {
    action_changed_subscription_ =
        action_item_->AddActionChangedCallback(base::BindRepeating(
            &ActionButton::ActionItemChanged, base::Unretained(this)));
    ActionItemChanged();
  }
  OnPropertyChanged(&action_item_,
                    static_cast<PropertyEffects>(kPropertyEffectsLayout |
                                                 kPropertyEffectsPaint));
}

std::u16string ActionButton::GetName() const {
  return name_;
}

void ActionButton::SetName(const std::u16string& name) {
  if (name == name_) {
    return;
  }
  name_ = name;
  if (GetText().empty()) {
    SetText(name_);
  }
  OnPropertyChanged(&name, kPropertyEffectsNone);
}

void ActionButton::ActionItemChanged() {
  SetText(action_item_->GetText());
  SetEnabled(action_item_->GetEnabled());
  SetVisible(action_item_->GetVisible());
  SetTooltipText(action_item_->GetTooltipText());
  SetImageModel(Button::ButtonState::STATE_NORMAL, action_item_->GetImage());
}

void ActionButton::TriggerAction() {
  if (action_item_.get()) {
    action_item_->InvokeAction();
  }
}

BEGIN_METADATA(ActionButton, MdTextButton)
END_METADATA

BEGIN_VIEW_BUILDER(, ActionButton, MdTextButton)
VIEW_BUILDER_PROPERTY(actions::ActionItem*, ActionItem)
VIEW_BUILDER_PROPERTY(std::u16string, Name)
END_VIEW_BUILDER

class ActionCheckbox : public Checkbox {
 public:
  METADATA_HEADER(ActionCheckbox);
  ActionCheckbox();
  ActionCheckbox(const ActionCheckbox&) = delete;
  ActionCheckbox& operator=(const ActionCheckbox&) = delete;
  ~ActionCheckbox() override = default;
  actions::ActionItem* GetActionItem() const;
  void SetActionItem(actions::ActionItem* action_item);
  std::u16string GetName() const;
  void SetName(const std::u16string& name);

 private:
  void ActionItemChanged();
  void TriggerAction();
  std::u16string name_;
  raw_ptr<actions::ActionItem> action_item_ = nullptr;
  base::CallbackListSubscription action_changed_subscription_;
};

ActionCheckbox::ActionCheckbox()
    : Checkbox(std::u16string(),
               base::BindRepeating(&ActionCheckbox::TriggerAction,
                                   base::Unretained(this))) {}

actions::ActionItem* ActionCheckbox::GetActionItem() const {
  return action_item_.get();
}

void ActionCheckbox::SetActionItem(actions::ActionItem* action_item) {
  if (action_item_.get() == action_item) {
    return;
  }
  action_item_ = action_item;
  action_changed_subscription_ = {};
  if (action_item_) {
    action_changed_subscription_ =
        action_item_->AddActionChangedCallback(base::BindRepeating(
            &ActionCheckbox::ActionItemChanged, base::Unretained(this)));
    ActionItemChanged();
  }
  OnPropertyChanged(&action_item_,
                    static_cast<PropertyEffects>(kPropertyEffectsLayout |
                                                 kPropertyEffectsPaint));
}

std::u16string ActionCheckbox::GetName() const {
  return name_;
}

void ActionCheckbox::SetName(const std::u16string& name) {
  if (name_ == name) {
    return;
  }
  name_ = name;
  if (GetText().empty()) {
    SetText(name_);
  }
  OnPropertyChanged(&name_, kPropertyEffectsNone);
}

void ActionCheckbox::ActionItemChanged() {
  SetText(action_item_->GetText());
  SetEnabled(action_item_->GetEnabled());
  SetVisible(action_item_->GetVisible());
  SetTooltipText(action_item_->GetTooltipText());
  SetChecked(action_item_->GetChecked());
}

void ActionCheckbox::TriggerAction() {
  if (action_item_) {
    action_item_->SetChecked(GetChecked());
    action_item_->InvokeAction();
  }
}

BEGIN_METADATA(ActionCheckbox, Checkbox)
END_METADATA

BEGIN_VIEW_BUILDER(, ActionCheckbox, Checkbox)
VIEW_BUILDER_PROPERTY(actions::ActionItem*, ActionItem)
VIEW_BUILDER_PROPERTY(std::u16string, Name)
END_VIEW_BUILDER

}  // namespace views::examples

DEFINE_VIEW_BUILDER(, views::examples::ActionButton)
DEFINE_VIEW_BUILDER(, views::examples::ActionCheckbox)

namespace views::examples {

namespace {

constexpr int kActionExampleVerticalSpacing = 3;
constexpr int kActionExampleLeftPadding = 8;
constexpr gfx::Insets kSeparatorPadding = gfx::Insets::TLBR(5, 0, 5, 0);
const char* kBoolStrings[2] = {"false", "true"};

#define MAP_ACTION_IDS_TO_STRINGS
#include "ui/actions/action_id_macros.inc"

// clang-format off
constexpr auto kActionIdStrings =
    base::MakeFixedFlatMap<actions::ActionId, const char*>({
        ACTION_IDS
        EXAMPLES_ACTION_IDS
    });
// clang-format on

#include "ui/actions/action_id_macros.inc"  // NOLINT(build/include)
#undef STRINGIZE_ACTION_IDS

class ViewsComboboxModel : public ui::ComboboxModel, public ViewObserver {
 public:
  explicit ViewsComboboxModel(View* container);
  ViewsComboboxModel(const ViewsComboboxModel&) = delete;
  ViewsComboboxModel& operator=(const ViewsComboboxModel&) = delete;
  ~ViewsComboboxModel() override = default;

  // ui::ComboboxModel overrides.
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  absl::optional<size_t> GetDefaultIndex() const override;

  View* GetViewItemAt(size_t index) const;

 protected:
  // ViewObserver overrides
  void OnViewIsDeleting(View* observed_view) override;

 private:
  raw_ptr<View> container_;
  base::ScopedObservation<View, ViewObserver> container_observation_{this};
};

ViewsComboboxModel::ViewsComboboxModel(View* container)
    : container_(container) {
  container_observation_.Observe(container_);
}

size_t ViewsComboboxModel::GetItemCount() const {
  size_t size = container_->children().size();
  return size ? size : 1;
}

std::u16string ViewsComboboxModel::GetItemAt(size_t index) const {
  if (container_->children().size()) {
    View* view = container_->children()[index];
    if (IsViewClass<ActionButton>(view)) {
      return AsViewClass<ActionButton>(view)->GetName();
    }
    if (IsViewClass<ActionCheckbox>(view)) {
      return AsViewClass<ActionCheckbox>(view)->GetName();
    }
  }
  return u"<Unknown>";
}

absl::optional<size_t> ViewsComboboxModel::GetDefaultIndex() const {
  return {0};
}

View* ViewsComboboxModel::GetViewItemAt(size_t index) const {
  if (container_->children().size()) {
    return container_->children()[index];
  }
  return nullptr;
}

void ViewsComboboxModel::OnViewIsDeleting(View* observed_view) {
  if (observed_view == container_.get()) {
    container_observation_.Reset();
    container_ = nullptr;
  }
}

class ActionItemComboboxModel : public ui::ComboboxModel {
 public:
  explicit ActionItemComboboxModel(actions::ActionItem* action_scope);
  ActionItemComboboxModel(const ActionItemComboboxModel&) = delete;
  ActionItemComboboxModel& operator=(const ActionItemComboboxModel&) = delete;
  ~ActionItemComboboxModel() override = default;

  // ui::ComboboxModel overrides.
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;

  actions::ActionItem* GetActionItemAt(size_t index) const;

 private:
  actions::ActionItemVector items_;
};

ActionItemComboboxModel::ActionItemComboboxModel(
    actions::ActionItem* action_scope) {
  actions::ActionManager::Get().GetActions(items_, action_scope);
}

size_t ActionItemComboboxModel::GetItemCount() const {
  return items_.size();
}

std::u16string ActionItemComboboxModel::GetItemAt(size_t index) const {
  return GetActionItemAt(index)->GetText();
}

actions::ActionItem* ActionItemComboboxModel::GetActionItemAt(
    size_t index) const {
  return items_[index];
}

class ControlTypeComboboxModel : public ui::ComboboxModel {
 public:
  ControlTypeComboboxModel() = default;
  ControlTypeComboboxModel(const ControlTypeComboboxModel&) = delete;
  ControlTypeComboboxModel& operator=(const ControlTypeComboboxModel&) = delete;
  ~ControlTypeComboboxModel() override = default;

  // ui::ComboboxModel overrides.
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
};

size_t ControlTypeComboboxModel::GetItemCount() const {
  return 2;
}

std::u16string ControlTypeComboboxModel::GetItemAt(size_t index) const {
  switch (index) {
    case 0:
      return u"Button";
    case 1:
      return u"Checkbox";
    default:
      NOTREACHED();
      return u"";
  }
}

class FlowLayout : public LayoutManagerBase {
 public:
  FlowLayout() = default;
  FlowLayout(const FlowLayout&) = delete;
  FlowLayout& operator=(const FlowLayout&) = delete;
  ~FlowLayout() override = default;

 protected:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override;
};

ProposedLayout FlowLayout::CalculateProposedLayout(
    const SizeBounds& size_bounds) const {
  ProposedLayout layout;
  int x = 0;
  int y = 0;
  int max_height = 0;
  for (auto* view : host_view()->children()) {
    bool view_visible = view->GetVisible();
    gfx::Size preferred_size = view->GetPreferredSize();
    if (view_visible) {
      max_height = std::max(max_height, preferred_size.height());
      if (x > 0 && (x + preferred_size.width() > size_bounds.width())) {
        x = 0;
        y += max_height;
        max_height = 0;
      }
    }
    gfx::Rect proposed_bounds = gfx::Rect(gfx::Point(x, y), preferred_size);
    SizeBounds available_bounds = SizeBounds(preferred_size);
    ChildLayout child_layout = {view, view_visible, proposed_bounds,
                                available_bounds};
    layout.child_layouts.push_back(child_layout);
    if (view_visible) {
      x += preferred_size.width();
    }
  }
  return layout;
}

}  // namespace

ActionsExample::ActionsExample() : ExampleBase("Actions") {
  subscriptions_.push_back(
      actions::ActionManager::Get().AppendActionItemInitializer(
          base::BindRepeating(&ActionsExample::CreateActions,
                              base::Unretained(this))));
}

ActionsExample::~ActionsExample() = default;

void ActionsExample::CreateExampleView(View* container) {
  Builder<View>(container)
      .SetUseDefaultFillLayout(true)
      .AddChild(
          Builder<BoxLayoutView>()
              .SetOrientation(BoxLayout::Orientation::kHorizontal)
              .AddChildren(
                  Builder<View>()
                      .CopyAddressTo(&action_panel_)
                      .SetLayoutManager(std::make_unique<FlowLayout>())
                      .SetBorder(CreateThemedSolidBorder(
                          1, ui::kColorFocusableBorderUnfocused)),
                  Builder<BoxLayoutView>()
                      .CopyAddressTo(&control_panel_)
                      .SetOrientation(BoxLayout::Orientation::kVertical)
                      .SetInsideBorderInsets(
                          gfx::Insets::VH(kActionExampleVerticalSpacing,
                                          kActionExampleLeftPadding))
                      .SetBetweenChildSpacing(kActionExampleVerticalSpacing))
              .AfterBuild(base::BindOnce(
                  [](raw_ptr<View>* action_panel,
                     raw_ptr<BoxLayoutView>* control_panel,
                     BoxLayoutView* box) {
                    box->SetFlexForView((*action_panel).get(), 4);
                    box->SetFlexForView((*control_panel).get(), 1);
                  },
                  &action_panel_, &control_panel_)))
      .BuildChildren();

  auto add_row = [this]() {
    auto* row = control_panel_->AddChildView(
        Builder<BoxLayoutView>()
            .SetOrientation(BoxLayout::Orientation::kHorizontal)
            .SetBetweenChildSpacing(kActionExampleVerticalSpacing)
            .SetCrossAxisAlignment(BoxLayout::CrossAxisAlignment::kCenter)
            .Build());
    return row;
  };

  auto add_combobox_row = [&add_row](
                              std::unique_ptr<ui::ComboboxModel> model,
                              int label_text) -> std::pair<View*, Combobox*> {
    auto* row = add_row();
    Label* label = nullptr;
    Combobox* combobox = nullptr;
    Builder<BoxLayoutView>(row)
        .AddChildren(
            Builder<Label>().CopyAddressTo(&label).SetText(
                l10n_util::GetStringUTF16(label_text)),
            Builder<Combobox>(std::make_unique<Combobox>(std::move(model)))
                .CopyAddressTo(&combobox))
        .BuildChildren();
    combobox->SetAccessibleName(label->GetText());
    return {row, combobox};
  };

  auto add_combobox_and_button = [&add_combobox_row](
                                     std::unique_ptr<ui::ComboboxModel> model,
                                     int label_text,
                                     actions::ActionId action_id) {
    auto pair = add_combobox_row(std::move(model), label_text);
    pair.first->AddChildView(
        Builder<ActionButton>()
            .SetActionItem(actions::ActionManager::Get().FindAction(action_id))
            .Build());
    return pair.second;
  };

  auto add_checkbox_row = [this, &add_row](int checkbox_text,
                                           PropertyChangedCallback callback) {
    auto* row = add_row();
    auto* checkbox =
        row->AddChildView(Builder<Checkbox>()
                              .SetText(l10n_util::GetStringUTF16(checkbox_text))
                              .Build());
    subscriptions_.push_back(
        checkbox->AddCheckedChangedCallback(std::move(callback)));
    return checkbox;
  };

  auto add_textfield_row = [this, &add_row](int label_text,
                                            PropertyChangedCallback callback) {
    auto* row = add_row();
    Label* label = nullptr;
    Textfield* textfield = nullptr;
    Builder<BoxLayoutView>(row)
        .AddChildren(
            Builder<Label>().CopyAddressTo(&label).SetText(
                l10n_util::GetStringUTF16(label_text)),
            Builder<Textfield>()
                .CopyAddressTo(&textfield)
                .CustomConfigure(base::BindOnce(
                    [](std::vector<base::CallbackListSubscription>*
                           subscriptions,
                       PropertyChangedCallback callback, Textfield* textfield) {
                      subscriptions->push_back(
                          textfield->AddTextChangedCallback(
                              std::move(callback)));
                    },
                    &subscriptions_, std::move(callback))))
        .AfterBuild(base::BindOnce(
            [](Textfield** textfield, Label** label, BoxLayoutView* row) {
              row->SetFlexForView(*textfield, 1);
              (*textfield)->SetAccessibleName((*label)->GetText());
            },
            &textfield, &label))
        .BuildChildren();
    return textfield;
  };

  available_controls_ =
      add_combobox_and_button(std::make_unique<ControlTypeComboboxModel>(),
                              IDS_AVAILABLE_CONTROLS, kActionCreateControl);

  control_panel_->AddChildView(
      Builder<Separator>().SetProperty(kMarginsKey, kSeparatorPadding).Build());

  controls_ = add_combobox_row(
                  std::make_unique<ViewsComboboxModel>(action_panel_.get()),
                  IDS_CONTROLS)
                  .second;

  available_actions_ = add_combobox_and_button(
      std::make_unique<ActionItemComboboxModel>(example_actions_.get()),
      IDS_AVAILABLE_ACTIONS, kActionAssignAction);
  subscriptions_.push_back(
      available_actions_->AddSelectedIndexChangedCallback(base::BindRepeating(
          &ActionsExample::ActionSelected, base::Unretained(this))));

  action_visible_ = add_checkbox_row(
      IDS_ACTION_VISIBLE, base::BindRepeating(&ActionsExample::VisibleChanged,
                                              base::Unretained(this)));
  action_enabled_ = add_checkbox_row(
      IDS_ACTION_ENABLED, base::BindRepeating(&ActionsExample::EnabledChanged,
                                              base::Unretained(this)));
  action_checked_ = add_checkbox_row(
      IDS_ACTION_CHECKED, base::BindRepeating(&ActionsExample::CheckedChanged,
                                              base::Unretained(this)));
  action_text_ = add_textfield_row(
      IDS_ACTION_TEXT, base::BindRepeating(&ActionsExample::TextChanged,
                                           base::Unretained(this)));
  action_tooltip_text_ =
      add_textfield_row(IDS_ACTION_TOOLTIP_TEXT,
                        base::BindRepeating(&ActionsExample::TooltipTextChanged,
                                            base::Unretained(this)));
  auto* row = add_row();
  control_panel_->SetFlexForView(row, 1);
  actions_trigger_info_ =
      row->AddChildView(Builder<Textarea>()
                            .SetReadOnly(true)
                            .SetAccessibleName(l10n_util::GetStringUTF16(
                                IDS_ACTIONS_TRIGGER_INFO))
                            .Build());
  row->SetCrossAxisAlignment(BoxLayout::CrossAxisAlignment::kStretch);
  row->SetFlexForView(actions_trigger_info_.get(), 1);

  ActionSelected();
}

void ActionsExample::CreateActions(actions::ActionManager* manager) {
  manager->AddActions(
      actions::ActionItem::Builder()
          .CopyAddressTo(&example_actions_)
          .AddChildren(actions::ActionItem::Builder(
                           base::BindRepeating(&ActionsExample::ActionInvoked,
                                               base::Unretained(this)))
                           .SetText(u"Test Action 1")
                           .SetActionId(kActionTest1),
                       actions::ActionItem::Builder(
                           base::BindRepeating(&ActionsExample::ActionInvoked,
                                               base::Unretained(this)))
                           .SetText(u"Test Action 2")
                           .SetActionId(kActionTest2),
                       actions::ActionItem::Builder(
                           base::BindRepeating(&ActionsExample::ActionInvoked,
                                               base::Unretained(this)))
                           .SetText(u"Test Action 3")
                           .SetActionId(kActionTest2))
          .Build(),
      actions::ActionItem::Builder()
          .AddChildren(
              actions::ActionItem::Builder(
                  base::BindRepeating(&ActionsExample::AssignAction,
                                      base::Unretained(this)))
                  .SetActionId(kActionAssignAction)
                  .SetText(
                      l10n_util::GetStringUTF16(IDS_ASSIGN_ACTION_TO_CONTROL)),
              actions::ActionItem::Builder(
                  base::BindRepeating(&ActionsExample::CreateControl,
                                      base::Unretained(this)))
                  .SetActionId(kActionCreateControl)
                  .SetText(l10n_util::GetStringUTF16(IDS_CREATE_CONTROL)))
          .Build());
}

void ActionsExample::ActionInvoked(actions::ActionItem* action,
                                   actions::ActionInvocationContext context) {
  auto bool_to_string = [](bool value) {
    return value ? kBoolStrings[1] : kBoolStrings[0];
  };
  std::u16string text = actions_trigger_info_->GetText();
  if (!text.empty()) {
    text.append(u"\n");
  }
  text.append(base::ASCIIToUTF16(base::StringPrintf(
      "Action ID: %s; Text: \"%s\"; Visible: %s; Enabled: %s",
      kActionIdStrings.at(
          action->GetActionId().value_or(actions::kActionsStart)),
      base::UTF16ToASCII(action->GetText()).c_str(),
      bool_to_string(action->GetVisible()),
      bool_to_string(action->GetEnabled()))));
  // Manually append the text since Textfield::AppendText() is broken right now.
  // (crbug.com/1476989)
  actions_trigger_info_->SetText(text);
  if (GetSelectedAction() == action) {
    action_checked_->SetChecked(action->GetChecked());
  }
}

void ActionsExample::ActionSelected() {
  const actions::ActionItem* action_item = GetSelectedAction();
  action_checked_->SetChecked(action_item->GetChecked());
  action_visible_->SetChecked(action_item->GetVisible());
  action_enabled_->SetChecked(action_item->GetEnabled());
  action_text_->SetText(action_item->GetText());
  action_tooltip_text_->SetText(action_item->GetTooltipText());
}

void ActionsExample::AssignAction(actions::ActionItem* action,
                                  actions::ActionInvocationContext context) {
  auto index = controls_->GetSelectedIndex();
  if (!index || controls_->GetModel()->GetItemCount() == 0) {
    return;
  }
  ViewsComboboxModel* model =
      static_cast<ViewsComboboxModel*>(controls_->GetModel());
  View* view = model->GetViewItemAt(index.value());
  if (IsViewClass<ActionButton>(view)) {
    ActionButton* button = AsViewClass<ActionButton>(view);
    button->SetActionItem(GetSelectedAction());
  } else if (IsViewClass<ActionCheckbox>(view)) {
    ActionCheckbox* checkbox = AsViewClass<ActionCheckbox>(view);
    checkbox->SetActionItem(GetSelectedAction());
  }
}

void ActionsExample::CreateControl(actions::ActionItem* action,
                                   actions::ActionInvocationContext context) {
  static int control_num = 0;
  std::unique_ptr<View> new_view;
  const absl::optional<size_t> selected_index =
      available_controls_->GetSelectedIndex();
  switch (selected_index.value_or(0)) {
    case 0:
      new_view = Builder<ActionButton>()
                     .SetName(u"Button " + base::NumberToString16(control_num))
                     .Build();
      break;
    case 1:
      new_view =
          Builder<ActionCheckbox>()
              .SetName(u"Checkbox " + base::NumberToString16(control_num))
              .Build();
      break;
    default:
      NOTREACHED();
  }
  action_panel_->AddChildView(std::move(new_view));
  ++control_num;
}

actions::ActionItem* ActionsExample::GetSelectedAction() const {
  ActionItemComboboxModel* action_item_model =
      static_cast<ActionItemComboboxModel*>(available_actions_->GetModel());
  return action_item_model->GetActionItemAt(
      available_actions_->GetSelectedIndex().value_or(0));
}

void ActionsExample::EnabledChanged() {
  actions::ActionItem* action = GetSelectedAction();
  action->SetEnabled(action_enabled_->GetChecked());
}

void ActionsExample::CheckedChanged() {
  actions::ActionItem* action = GetSelectedAction();
  action->SetChecked(action_checked_->GetChecked());
}

void ActionsExample::TextChanged() {
  actions::ActionItem* action = GetSelectedAction();
  action->SetText(action_text_->GetText());
}

void ActionsExample::TooltipTextChanged() {
  actions::ActionItem* action = GetSelectedAction();
  action->SetTooltipText(action_tooltip_text_->GetText());
}

void ActionsExample::VisibleChanged() {
  actions::ActionItem* action = GetSelectedAction();
  action->SetVisible(action_visible_->GetChecked());
}

}  // namespace views::examples
