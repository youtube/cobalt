// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_dialog_manager_delegate.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/javascript_dialogs/tab_modal_dialog_view.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

// A window-modal JavaScript dialog parented to the standalone PiP widget.
//
// This mirrors JavaScriptTabModalDialogViewViews but does not depend on
// tabs::TabInterface / TabStripModel: the standalone PiP child WebContents is
// not a tab, so there is no tab-modal lock to acquire. The dialog is instead
// shown as a window-modal dialog parented directly to the PiP widget.
class DocumentPipJavaScriptDialogView
    : public javascript_dialogs::TabModalDialogView,
      public views::DialogDelegate,
      public views::WidgetObserver {
 public:
  DocumentPipJavaScriptDialogView(
      views::Widget* pip_widget,
      const std::u16string& title,
      content::JavaScriptDialogType dialog_type,
      const std::u16string& message_text,
      const std::u16string& default_prompt_text,
      content::JavaScriptDialogManager::DialogClosedCallback dialog_callback,
      base::OnceClosure dialog_force_closed_callback,
      base::OnceCallback<void(views::Widget::ClosedReason)> close_callback)
      : title_(title),
        message_text_(message_text),
        default_prompt_text_(default_prompt_text),
        dialog_callback_(std::move(dialog_callback)),
        dialog_force_closed_callback_(std::move(dialog_force_closed_callback)) {
    SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    SetModalType(ui::mojom::ModalType::kWindow);
    SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kOk));
    const bool is_alert = dialog_type == content::JAVASCRIPT_DIALOG_TYPE_ALERT;
    SetButtons(
        // Alerts only have an OK button, no Cancel, because there is no choice
        // being made.
        is_alert ? static_cast<int>(ui::mojom::DialogButton::kOk)
                 : static_cast<int>(ui::mojom::DialogButton::kOk) |
                       static_cast<int>(ui::mojom::DialogButton::kCancel));

    // Safe: `this` is the DialogDelegate that owns these accept/cancel
    // callbacks, so they cannot outlive `this`.
    SetAcceptCallback(base::BindOnce(
        [](DocumentPipJavaScriptDialogView* dialog) {
          // Remove the force-close callback to indicate that we were closed as
          // a result of user action.
          dialog->dialog_force_closed_callback_ = base::OnceClosure();
          if (dialog->dialog_callback_) {
            std::move(dialog->dialog_callback_)
                .Run(true,
                     std::u16string(dialog->message_box_view_->GetInputText()));
          }
        },
        base::Unretained(this)));
    SetCancelCallback(base::BindOnce(
        [](DocumentPipJavaScriptDialogView* dialog) {
          // Remove the force-close callback to indicate that we were closed as
          // a result of user action.
          dialog->dialog_force_closed_callback_ = base::OnceClosure();
          if (dialog->dialog_callback_) {
            std::move(dialog->dialog_callback_).Run(false, std::u16string());
          }
        },
        base::Unretained(this)));
    // Observe the PiP widget so ResizePipToContainDialog/RestorePipBounds can
    // detect if the PiP window is destroyed before this dialog finishes
    // closing.
    pip_widget_observation_.Observe(pip_widget);

    auto message_box_view = std::make_unique<views::MessageBoxView>(
        message_text, /*detect_directionality=*/true);
    if (dialog_type == content::JAVASCRIPT_DIALOG_TYPE_PROMPT) {
      message_box_view->SetPromptField(default_prompt_text);
    }

    message_box_view_ = message_box_view.get();
    SetContentsView(std::move(message_box_view));

    // Show the dialog window-modal to the PiP widget, parented to the PiP
    // widget's native view. Ownership of both this delegate and the widget is
    // kept by DocumentPipDialogManagerDelegate; the widget uses
    // CLIENT_OWNS_WIDGET and routes Close() through `close_callback`.
    dialog_widget_.reset(views::DialogDelegate::CreateDialogWidget(
        this, gfx::NativeWindow(), pip_widget->GetNativeView()));
    dialog_widget_->MakeCloseSynchronous(std::move(close_callback));
    // Grow the PiP window to contain the dialog before showing, so the dialog
    // isn't clipped/overflowing the small PiP window.
    ResizePipToContainDialog(dialog_widget_.get());
    dialog_widget_->Show();
  }

  DocumentPipJavaScriptDialogView(const DocumentPipJavaScriptDialogView&) =
      delete;
  DocumentPipJavaScriptDialogView& operator=(
      const DocumentPipJavaScriptDialogView&) = delete;

  ~DocumentPipJavaScriptDialogView() override {
    HandleDialogClosing();
    dialog_widget_.reset();
  }

  void HandleDialogClosing() {
    if (did_handle_dialog_closing_) {
      return;
    }
    did_handle_dialog_closing_ = true;

    // If the force-close callback still exists at this point we're not closed
    // due to a user action (would've been caught in Accept/Cancel). This fires
    // when the dialog is closed externally, e.g. when the PiP widget itself is
    // torn down mid-dialog.
    if (dialog_force_closed_callback_) {
      std::move(dialog_force_closed_callback_).Run();
    }
    // Shrink the PiP window back to its pre-dialog size now that the dialog is
    // going away.
    RestorePipBounds();
    // The contents view is destroyed when `dialog_widget_` is reset. Drop this
    // raw_ptr before that happens to avoid dangling raw_ptr detection.
    message_box_view_ = nullptr;
  }

  // javascript_dialogs::TabModalDialogView:
  void CloseDialogWithoutCallback() override {
    dialog_callback_.Reset();
    dialog_force_closed_callback_.Reset();
    if (dialog_widget_) {
      dialog_widget_->Close();
    }
  }
  std::u16string GetUserInput() override {
    return std::u16string(message_box_view_->GetInputText());
  }

  // views::DialogDelegate:
  std::u16string GetWindowTitle() const override { return title_; }

  // views::WidgetDelegate:
  bool ShouldShowCloseButton() const override { return false; }
  views::View* GetInitiallyFocusedView() override {
    auto* text_box = message_box_view_->GetVisiblePromptField();
    return text_box ? text_box
                    : views::DialogDelegate::GetInitiallyFocusedView();
  }

  // views::WidgetDelegate:
  void OnWidgetInitialized() override {
    if (!message_text_.empty()) {
      GetWidget()->GetRootView()->GetViewAccessibility().SetDescription(
          message_text_);
    }
#if BUILDFLAG(IS_MAC)
    // On Mac, the platform accessibility API automatically calculates the name
    // of the native window based on the child RootView. Override that
    // calculation so we can present both the title (e.g. "url.com says") and
    // the message text, since the accessible description is ignored.
    message_box_view_->GetViewAccessibility().OverrideNativeWindowTitle(
        l10n_util::GetStringFUTF16(IDS_CONCAT_TWO_STRINGS_WITH_COMMA,
                                   GetWindowTitle(), message_text_));
#endif
  }

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override {
    // The PiP widget is going away; clear the growth flag so RestorePipBounds()
    // skips the (now-dangling) widget. ScopedObservation removes the observer.
    did_grow_pip_ = false;
  }

  base::WeakPtr<DocumentPipJavaScriptDialogView> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  views::Widget* GetDialogWidget() { return dialog_widget_.get(); }

 private:
  // Grows the standalone PiP window so this window-modal dialog is fully
  // contained, then places it at the top of the PiP client area (just under
  // the frame view). The browser-backed PiP does this via its frame view's
  // child-dialog observer; the standalone dialog is a separate desktop widget,
  // so we enlarge the PiP and re-position the dialog explicitly. Records the
  // pre-dialog bounds so they can be restored when the dialog closes.
  void ResizePipToContainDialog(views::Widget* dialog_widget) {
    views::Widget* pip_widget = pip_widget_observation_.GetSource();
    if (!pip_widget || pip_widget->IsClosed()) {
      return;
    }

    const gfx::Rect pip_window = pip_widget->GetWindowBoundsInScreen();
    const gfx::Rect pip_client = pip_widget->GetClientAreaBoundsInScreen();

    gfx::Size dialog_size = dialog_widget->GetWindowBoundsInScreen().size();
    // The root view's minimum size is the dialog's preferred size; never grow
    // for anything smaller than that.
    dialog_size.SetToMax(dialog_widget->GetRootView()->GetMinimumSize());

    // Non-client chrome around the PiP client area (title bar + borders).
    const gfx::Size non_client_extra(pip_window.width() - pip_client.width(),
                                     pip_window.height() - pip_client.height());

    // Match the PiP client width exactly to the dialog width, and ensure the
    // client height is at least tall enough for the dialog (no centering,
    // no extra margins).
    gfx::Size required(
        std::max(pip_window.width(),
                 dialog_size.width() + non_client_extra.width()),
        std::max(pip_window.height(),
                 dialog_size.height() + non_client_extra.height()));

    if (required != pip_window.size()) {
      saved_pip_bounds_ = pip_window;
      did_grow_pip_ = true;
      gfx::Rect new_pip_bounds = pip_window;
      new_pip_bounds.set_size(required);
      pip_widget->SetBoundsConstrained(new_pip_bounds);
    }

    // Position the dialog at the top-left of the PiP client area, immediately
    // below the non-client frame/title area.
    const gfx::Rect client = pip_widget->GetClientAreaBoundsInScreen();
    gfx::Rect dialog_bounds(dialog_size);
    dialog_bounds.set_origin(client.origin());
    dialog_widget->SetBounds(dialog_bounds);
  }

  // Restores the PiP window to its pre-dialog size when the dialog closes, if
  // it was grown to contain the dialog and the PiP window is still alive.
  // Preserves the window's current origin so that any move the user performed
  // while the dialog was open is retained; only the size is restored.
  void RestorePipBounds() {
    views::Widget* pip_widget = pip_widget_observation_.GetSource();
    if (!did_grow_pip_ || !pip_widget || pip_widget->IsClosed()) {
      return;
    }
    did_grow_pip_ = false;
    gfx::Rect current_bounds = pip_widget->GetWindowBoundsInScreen();
    current_bounds.set_size(saved_pip_bounds_.size());
    pip_widget->SetBoundsConstrained(current_bounds);
  }

  // Observes the standalone PiP widget; cleared automatically if the PiP
  // window is destroyed before this dialog finishes closing.
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      pip_widget_observation_{this};
  // Pre-dialog PiP window bounds, restored when the dialog closes if the
  // window was grown to contain the dialog.
  gfx::Rect saved_pip_bounds_;
  bool did_grow_pip_ = false;

  std::u16string title_;
  std::u16string message_text_;
  std::u16string default_prompt_text_;
  content::JavaScriptDialogManager::DialogClosedCallback dialog_callback_;
  base::OnceClosure dialog_force_closed_callback_;
  bool did_handle_dialog_closing_ = false;

  std::unique_ptr<views::Widget> dialog_widget_;

  // The message box view whose commands we handle. Owned by the views
  // hierarchy.
  raw_ptr<views::MessageBoxView> message_box_view_ = nullptr;

  base::WeakPtrFactory<DocumentPipJavaScriptDialogView> weak_factory_{this};
};

DocumentPipDialogManagerDelegate::DocumentPipDialogManagerDelegate(
    views::Widget* pip_widget)
    : pip_widget_(*pip_widget) {}

DocumentPipDialogManagerDelegate::~DocumentPipDialogManagerDelegate() = default;

base::WeakPtr<javascript_dialogs::TabModalDialogView>
DocumentPipDialogManagerDelegate::CreateNewDialog(
    content::WebContents* alerting_web_contents,
    const std::u16string& title,
    content::JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    content::JavaScriptDialogManager::DialogClosedCallback dialog_callback,
    base::OnceClosure dialog_closed_callback) {
  active_dialog_ = std::make_unique<DocumentPipJavaScriptDialogView>(
      &pip_widget_.get(), title, dialog_type, message_text, default_prompt_text,
      std::move(dialog_callback), std::move(dialog_closed_callback),
      base::BindOnce(
          [](base::WeakPtr<DocumentPipDialogManagerDelegate> delegate,
             views::Widget::ClosedReason) {
            if (!delegate || !delegate->active_dialog_) {
              return;
            }
            // Destroying the dialog runs ~DocumentPipJavaScriptDialogView,
            // which calls HandleDialogClosing() (idempotent).
            delegate->active_dialog_.reset();
          },
          weak_factory_.GetWeakPtr()));
  return active_dialog_->GetWeakPtr();
}

void DocumentPipDialogManagerDelegate::WillRunDialog() {}

void DocumentPipDialogManagerDelegate::DidCloseDialog() {}

void DocumentPipDialogManagerDelegate::SetTabNeedsAttention(bool attention) {
  // The standalone PiP window has no tab strip to pulse for attention.
}

bool DocumentPipDialogManagerDelegate::IsWebContentsForemost() {
  // The PiP window is its own top-level modal context. Do not gate this on
  // Widget::IsActive(), since standalone PiP is outside the browser/tab
  // activation model used by TabModalDialogManager; using native activation
  // here suppresses confirm()/prompt() even when the user clicked inside PiP.
  return !pip_widget_->IsClosed();
}

bool DocumentPipDialogManagerDelegate::IsApp() {
  return false;
}

bool DocumentPipDialogManagerDelegate::CanShowModalUI() {
  // The PiP window is always its own exclusive modal context; there is no tab
  // strip whose modal-UI lock could be contended.
  return true;
}

views::Widget*
DocumentPipDialogManagerDelegate::GetActiveDialogWidgetForTesting() {
  return active_dialog_ ? active_dialog_->GetDialogWidget() : nullptr;
}
