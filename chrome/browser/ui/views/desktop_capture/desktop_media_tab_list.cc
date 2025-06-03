// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_tab_list.h"

#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media/webrtc/desktop_media_list_layout_config.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

using content::BrowserThread;

namespace {

// Max stored length for the title of a previewed tab. The actual displayed
// length is likely shorter than this, as the Label will elide it to fit the UI.
constexpr const int kMaxPreviewTitleLength = 500;

// TODO(crbug.com/1224342): Refer to central Desktop UI constants rather than
// hardcoding this.
const int kListWidth = 346;

// Delay after the selection is changed before clearing the preview, to allow a
// direct switch from the old preview to the new one without flashing grey in
// between.
const base::TimeDelta kClearPreviewDelay = base::Milliseconds(200);

// ui::TableModel that wraps a DesktopMediaListController and listens for
// updates from it.
class TabListModel : public ui::TableModel,
                     public DesktopMediaListController::SourceListListener {
 public:
  explicit TabListModel(
      DesktopMediaListController* controller,
      base::RepeatingCallback<void(size_t)> preview_updated_callback);

  // ui::TableModel:
  size_t RowCount() override;
  std::u16string GetText(size_t row, int column) override;
  ui::ImageModel GetIcon(size_t row) override;
  void SetObserver(ui::TableModelObserver* observer) override;

  // DesktopMediaListController::SourceListListener:
  void OnSourceAdded(size_t index) override;
  void OnSourceRemoved(size_t index) override;
  void OnSourceMoved(size_t old_index, size_t new_index) override;
  void OnSourceNameChanged(size_t index) override;
  void OnSourceThumbnailChanged(size_t index) override;
  void OnSourcePreviewChanged(size_t index) override;
  void OnDelegatedSourceListSelection() override;

 private:
  TabListModel(const TabListModel&) = delete;
  TabListModel operator=(const TabListModel&) = delete;

  raw_ptr<DesktopMediaListController, DanglingUntriaged> controller_;
  raw_ptr<ui::TableModelObserver> observer_ = nullptr;
  base::RepeatingCallback<void(size_t)> preview_updated_callback_;
};

TabListModel::TabListModel(
    DesktopMediaListController* controller,
    base::RepeatingCallback<void(size_t)> preview_updated_callback)
    : controller_(controller),
      preview_updated_callback_(preview_updated_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

size_t TabListModel::RowCount() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return controller_->GetSourceCount();
}

std::u16string TabListModel::GetText(size_t row, int column) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return controller_->GetSource(row).name;
}

ui::ImageModel TabListModel::GetIcon(size_t row) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ui::ImageModel::FromImageSkia(controller_->GetSource(row).thumbnail);
}

void TabListModel::SetObserver(ui::TableModelObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_ = observer;
}

void TabListModel::OnSourceAdded(size_t index) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_->OnItemsAdded(index, 1);
}

void TabListModel::OnSourceRemoved(size_t index) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_->OnItemsRemoved(index, 1);
}

void TabListModel::OnSourceMoved(size_t old_index, size_t new_index) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_->OnItemsMoved(old_index, 1, new_index);
}

void TabListModel::OnSourceNameChanged(size_t index) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_->OnItemsChanged(index, 1);
  // Also invoke the preview updated callback, to ensure the preview's use of
  // the source's name is updated.
  preview_updated_callback_.Run(index);
}

void TabListModel::OnSourceThumbnailChanged(size_t index) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observer_->OnItemsChanged(index, 1);
}

void TabListModel::OnSourcePreviewChanged(size_t index) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  preview_updated_callback_.Run(index);
}

void TabListModel::OnDelegatedSourceListSelection() {
  NOTREACHED_NORETURN()
      << "Tab Lists are not delegated, so should not get a selection event.";
}

// TableViewObserver implementation that bridges between the actual TableView
// listing tabs and the DesktopMediaTabList.
class TabListViewObserver : public views::TableViewObserver {
 public:
  TabListViewObserver(DesktopMediaListController* controller,
                      base::RepeatingClosure selection_changed_callback);

  void OnSelectionChanged() override;
  void OnKeyDown(ui::KeyboardCode virtual_keycode) override;

 private:
  TabListViewObserver(const TabListViewObserver&) = delete;
  TabListViewObserver operator=(const TabListViewObserver&) = delete;

  const raw_ptr<DesktopMediaListController, DanglingUntriaged> controller_;
  base::RepeatingClosure selection_changed_callback_;
};

TabListViewObserver::TabListViewObserver(
    DesktopMediaListController* controller,
    base::RepeatingClosure selection_changed_callback)
    : controller_(controller),
      selection_changed_callback_(std::move(selection_changed_callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void TabListViewObserver::OnSelectionChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  controller_->OnSourceSelectionChanged();
  selection_changed_callback_.Run();
}

void TabListViewObserver::OnKeyDown(ui::KeyboardCode virtual_keycode) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (virtual_keycode == ui::VKEY_RETURN)
    controller_->AcceptSource();
}

std::unique_ptr<views::ScrollView> CreateScrollViewWithTable(
    std::unique_ptr<views::TableView> table) {
  if (base::FeatureList::IsEnabled(kDisplayMediaPickerRedesign) &&
      features::IsChromeRefresh2023()) {
    auto scroll_view = std::make_unique<views::ScrollView>(
        views::ScrollView::ScrollWithLayers::kEnabled);
    scroll_view->SetDrawOverflowIndicator(false);
    scroll_view->SetViewportRoundedCornerRadius(gfx::RoundedCornersF(8));
    scroll_view->SetContents(std::move(table));
    scroll_view->SetBorder(nullptr);
    return scroll_view;
  } else {
    return views::TableView::CreateScrollViewWithTable(std::move(table));
  }
}

}  // namespace

DesktopMediaTabList::DesktopMediaTabList(DesktopMediaListController* controller,
                                         const std::u16string& accessible_name)
    : controller_(controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // The thumbnail size isn't allowed to be smaller than gfx::kFaviconSize by
  // the underlying media list. TableView requires that the icon size be exactly
  // ui::TableModel::kIconSize; if it's not, rendering of the TableView breaks.
  // This DCHECK enforces that kIconSize is an acceptable size for the source
  // list.
  DCHECK_GE(ui::TableModel::kIconSize, gfx::kFaviconSize);

  controller_->SetThumbnailSize(
      gfx::Size(ui::TableModel::kIconSize, ui::TableModel::kIconSize));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  model_ = std::make_unique<TabListModel>(
      controller_, base::BindRepeating(&DesktopMediaTabList::OnPreviewUpdated,
                                       weak_factory_.GetWeakPtr()));
  auto selection_changed_callback = base::BindRepeating(
      &DesktopMediaTabList::OnSelectionChanged, weak_factory_.GetWeakPtr());
  view_observer_ = std::make_unique<TabListViewObserver>(
      controller_, selection_changed_callback);

  auto table = std::make_unique<views::TableView>(
      model_.get(), std::vector<ui::TableColumn>(1), views::ICON_AND_TEXT,
      true);
  table->set_observer(view_observer_.get());
  table->GetViewAccessibility().OverrideName(accessible_name);
  table_ = table.get();

  AddChildView(BuildUI(std::move(table)));
}

std::unique_ptr<views::View> DesktopMediaTabList::BuildUI(
    std::unique_ptr<views::TableView> table) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto preview_wrapper = std::make_unique<views::View>();
  preview_wrapper->SetPreferredSize(desktopcapture::kPreviewSize);

  auto preview = std::make_unique<views::ImageView>();
  preview->SetVisible(false);
  preview->SetSize(desktopcapture::kPreviewSize);
  preview->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_DESKTOP_MEDIA_PICKER_PREVIEW_ACCESSIBLE_NAME));
  preview_ = preview_wrapper->AddChildView(std::move(preview));

  auto empty_preview_label = std::make_unique<views::Label>();
  empty_preview_label->SetText(
      l10n_util::GetStringUTF16(IDS_DESKTOP_MEDIA_PICKER_EMPTY_PREVIEW));
  empty_preview_label->SetMultiLine(true);
  empty_preview_label->SetPreferredSize(desktopcapture::kPreviewSize);
  empty_preview_label->SetSize(desktopcapture::kPreviewSize);
  empty_preview_label_ =
      preview_wrapper->AddChildView(std::move(empty_preview_label));

  auto preview_label = std::make_unique<views::Label>();
  preview_label->SetMultiLine(true);
  preview_label->SetMaximumWidth(desktopcapture::kPreviewSize.width());
  preview_label->SetMaxLines(2);
  preview_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  auto preview_sidebar = std::make_unique<views::View>();
  preview_wrapper_ = preview_sidebar->AddChildView(std::move(preview_wrapper));
  preview_label_ = preview_sidebar->AddChildView(std::move(preview_label));

  preview_sidebar->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      /*between_child_spacing=*/11));

  std::unique_ptr<views::View> full_panel = std::make_unique<views::View>();

  scroll_view_ =
      full_panel->AddChildView(CreateScrollViewWithTable(std::move(table)));
  scroll_view_->SetPreferredSize(gfx::Size(kListWidth, 0));
  full_panel->AddChildView(std::move(preview_sidebar));

  const gfx::Insets kFullPannelInset =
      base::FeatureList::IsEnabled(kDisplayMediaPickerRedesign)
          ? gfx::Insets(16)
          : gfx::Insets::TLBR(15, 0, 0, 0);
  const int kChildSpacing =
      base::FeatureList::IsEnabled(kDisplayMediaPickerRedesign) ? 16 : 12;
  views::BoxLayout* layout =
      full_panel->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kFullPannelInset,
          kChildSpacing, true));
  layout->SetFlexForView(scroll_view_, 1);

  auto container = std::make_unique<View>();
  container->SetUseDefaultFillLayout(true);
  container->AddChildView(std::move(full_panel));
  return container;
}

DesktopMediaTabList::~DesktopMediaTabList() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  table_->SetModel(nullptr);
}

gfx::Size DesktopMediaTabList::CalculatePreferredSize() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If the DisplayMediaPickerRedesign flag is active, height should be 9 rows
  // to allow space for the audio-toggle controller, otherwise default to 10
  // rows.
  const int preferred_item_count =
      base::FeatureList::IsEnabled(kDisplayMediaPickerRedesign) ? 9 : 10;
  return gfx::Size(0, table_->GetRowHeight() * preferred_item_count);
}

int DesktopMediaTabList::GetHeightForWidth(int width) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If this method isn't overridden here, the default implementation would fall
  // back to FillLayout's GetHeightForWidth, which would ask the TableView,
  // which would return something based on the total number of rows, since
  // TableView expects to always be sized by its container. Avoid even asking it
  // by using the same height as CalculatePreferredSize().
  return CalculatePreferredSize().height();
}

void DesktopMediaTabList::OnThemeChanged() {
  DesktopMediaListController::ListView::OnThemeChanged();

  const ui::ColorProvider* const color_provider = GetColorProvider();
  if (features::IsChromeRefresh2023()) {
    table_->SetBorder(nullptr);
  } else {
    table_->SetBorder(views::CreateSolidBorder(
        /*thickness=*/1,
        color_provider->GetColor(kColorDesktopMediaTabListBorder)));
  }

  if (base::FeatureList::IsEnabled(kDisplayMediaPickerRedesign) &&
      features::IsChromeRefresh2023()) {
    scroll_view_->SetBackground(views::CreateRoundedRectBackground(
        GetColorProvider()->GetColor(ui::kColorSysSurface4), 8));
    const SkColor background_color =
        color_provider->GetColor(ui::kColorSysTonalContainer);
    preview_wrapper_->SetBackground(
        views::CreateRoundedRectBackground(background_color, 8));
    empty_preview_label_->SetBackground(
        views::CreateRoundedRectBackground(background_color, 8));
    empty_preview_label_->SetBackgroundColor(background_color);
  } else {
    const SkColor background_color =
        color_provider->GetColor(kColorDesktopMediaTabListPreviewBackground);
    preview_wrapper_->SetBackground(
        views::CreateSolidBackground(background_color));
    empty_preview_label_->SetBackground(
        views::CreateSolidBackground(background_color));
    empty_preview_label_->SetBackgroundColor(background_color);
  }
}

absl::optional<content::DesktopMediaID> DesktopMediaTabList::GetSelection() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  absl::optional<size_t> row = table_->GetFirstSelectedRow();
  if (!row.has_value())
    return absl::nullopt;
  return controller_->GetSource(row.value()).id;
}

DesktopMediaListController::SourceListListener*
DesktopMediaTabList::GetSourceListListener() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return model_.get();
}

void DesktopMediaTabList::ClearSelection() {
  // Changing the selection in the list will ensure that all appropriate change
  // events are fired.
  table_->Select(absl::nullopt);
}

void DesktopMediaTabList::ClearPreview() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  preview_label_->SetText(u"");
  preview_->SetImage(nullptr);
  preview_->SetVisible(false);
  empty_preview_label_->SetVisible(true);
}

void DesktopMediaTabList::OnSelectionChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  absl::optional<size_t> row = table_->GetFirstSelectedRow();
  if (!row.has_value()) {
    ClearPreview();
    controller_->SetPreviewedSource(absl::nullopt);
    return;
  }
  const DesktopMediaList::Source& source = controller_->GetSource(row.value());

  const std::u16string truncated_title =
      source.name.substr(0, kMaxPreviewTitleLength);
  preview_label_->SetText(truncated_title);

  // Trigger a preview update to either show a previous snapshot for this source
  // if we have one, or clear it if we don't.
  OnPreviewUpdated(row.value());

  // Update the source for which previews are generated.
  controller_->SetPreviewedSource(source.id);

  preview_->SetVisible(true);
  empty_preview_label_->SetVisible(false);
}

void DesktopMediaTabList::ClearPreviewImageIfUnchanged(
    size_t previous_preview_set_count) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (preview_set_count_ == previous_preview_set_count) {
    // preview_ has not been set to a new image since this was scheduled. Clear
    // it.
    preview_->SetImage(nullptr);
  }
}

void DesktopMediaTabList::OnPreviewUpdated(size_t index) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (index != table_->GetFirstSelectedRow()) {
    return;
  }

  const DesktopMediaList::Source& source = controller_->GetSource(index);
  if (!source.preview.isNull()) {
    preview_->SetImage(source.preview);
    ++preview_set_count_;
  } else {
    // Clear the preview after a short time.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DesktopMediaTabList::ClearPreviewImageIfUnchanged,
                       weak_factory_.GetWeakPtr(), preview_set_count_),
        kClearPreviewDelay);
  }
  preview_label_->SetText(source.name.substr(0, kMaxPreviewTitleLength));
}

BEGIN_METADATA(DesktopMediaTabList, DesktopMediaListController::ListView)
END_METADATA
