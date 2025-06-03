// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/splash_screen.h"

#include <cstdint>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#include "chrome/updater/win/ui/ui.h"
#include "chrome/updater/win/ui/ui_constants.h"
#include "chrome/updater/win/ui/ui_util.h"

namespace updater::ui {

namespace {

constexpr int kClosingTimerID = 1;

// Frequency of alpha blending value changes during window fading state.
constexpr int kTimerInterval = 100;

// Alpha blending values for the fading effect.
constexpr int kDefaultAlphaScale = 100;
constexpr int kAlphaScales[] = {0, 30, 47, 62, 75, 85, 93, kDefaultAlphaScale};

uint8_t AlphaScaleToAlphaValue(int alpha_scale) {
  CHECK(alpha_scale >= 0 && alpha_scale <= 100);
  return static_cast<uint8_t>(alpha_scale * 255 / 100);
}

}  // namespace

void SilentSplashScreen::Show() {}

void SilentSplashScreen::Dismiss(base::OnceClosure callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

SplashScreen::SplashScreen(const std::u16string& bundle_name)
    : timer_created_(false), alpha_index_(0) {
  title_ = GetInstallerDisplayName(bundle_name);
  SwitchToState(WindowState::STATE_CREATED);
}

SplashScreen::~SplashScreen() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SplashScreen::Show() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(WindowState::STATE_CREATED, state_);

  if (FAILED(Initialize())) {
    return;
  }

  CHECK(IsWindow());
  ShowWindow(SW_SHOWNORMAL);
  SwitchToState(WindowState::STATE_SHOW_NORMAL);
}

void SplashScreen::Dismiss(base::OnceClosure on_close_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // After the splash screen is dismissed, but before the progress UI is shown,
  // there is a brief period of time when there are no windows for the current
  // process.
  //
  // By default, Windows gives the previous foreground process (say the command
  // line window) the foreground window at this point.
  //
  // To allow the subsequent progress UI to get foreground, the following call
  // to `::LockSetForegroundWindow(LSFW_LOCK)` is made before closing the splash
  // screen to prevent other applications from making a foreground change in
  // between.
  //
  // To complete the cycle, the progress UI calls
  // `::LockSetForegroundWindow(LSFW_UNLOCK)` before it calls
  // `::SetForegroundWindow`.
  ::LockSetForegroundWindow(LSFW_LOCK);

  on_close_closure_ = std::move(on_close_closure);
  switch (state_) {
    case WindowState::STATE_CREATED:
      SwitchToState(WindowState::STATE_CLOSED);
      break;
    case WindowState::STATE_SHOW_NORMAL:
      SwitchToState(WindowState::STATE_FADING);
      break;
    case WindowState::STATE_CLOSED:
    case WindowState::STATE_FADING:
    case WindowState::STATE_INITIALIZED:
      break;
  }
}

HRESULT SplashScreen::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!IsWindow());
  CHECK_EQ(state_, WindowState::STATE_CREATED);

  if (!Create(nullptr)) {
    return E_FAIL;
  }

  HideWindowChildren(*this);

  SetWindowText(title_.c_str());

  EnableSystemButtons(false);

  CWindow text_wnd = GetDlgItem(IDC_INSTALLER_STATE_TEXT);
  text_wnd.SetWindowText(
      GetLocalizedString(IDS_SPLASH_SCREEN_MESSAGE_BASE).c_str());
  text_wnd.ShowWindow(SW_SHOWNORMAL);

  InitProgressBar();

  ::SetLayeredWindowAttributes(
      m_hWnd, 0, AlphaScaleToAlphaValue(kDefaultAlphaScale), LWA_ALPHA);

  CenterWindow(nullptr);
  HRESULT hr = ui::SetWindowIcon(
      m_hWnd, IDI_APP,
      base::win::ScopedGDIObject<HICON>::Receiver(hicon_).get());
  if (FAILED(hr)) {
    VLOG(1) << "SetWindowIcon failed " << hr;
  }

  default_font_.CreatePointFont(90, kDialogFont);
  SendMessageToDescendants(
      WM_SETFONT, reinterpret_cast<WPARAM>(static_cast<HFONT>(default_font_)),
      0);

  font_.CreatePointFont(150, kDialogFont);
  GetDlgItem(IDC_INSTALLER_STATE_TEXT).SetFont(font_);
  GetDlgItem(IDC_INFO_TEXT).SetFont(font_);
  GetDlgItem(IDC_COMPLETE_TEXT).SetFont(font_);
  GetDlgItem(IDC_ERROR_TEXT).SetFont(font_);

  CreateOwnerDrawTitleBar(m_hWnd, GetDlgItem(IDC_TITLE_BAR_SPACER), kBkColor);
  SetCustomDlgColors(kTextColor, kBkColor);

  EnableFlatButtons(m_hWnd);

  SwitchToState(WindowState::STATE_INITIALIZED);
  return S_OK;
}

void SplashScreen::EnableSystemButtons(bool enable) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  constexpr LONG kSysStyleMask = WS_MINIMIZEBOX | WS_SYSMENU | WS_MAXIMIZEBOX;

  if (enable) {
    SetWindowLong(GWL_STYLE, GetWindowLong(GWL_STYLE) | kSysStyleMask);
  } else {
    SetWindowLong(GWL_STYLE, GetWindowLong(GWL_STYLE) & ~kSysStyleMask);

    // Remove Close/Minimize/Maximize from the system menu.
    HMENU menu(::GetSystemMenu(*this, false));
    CHECK(menu);
    ::RemoveMenu(menu, SC_CLOSE, MF_BYCOMMAND);
    ::RemoveMenu(menu, SC_MINIMIZE, MF_BYCOMMAND);
    ::RemoveMenu(menu, SC_MAXIMIZE, MF_BYCOMMAND);
  }
}

void SplashScreen::InitProgressBar() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  progress_bar_.SubclassWindow(GetDlgItem(IDC_PROGRESS));

  LONG_PTR style = progress_bar_.GetWindowLongPtr(GWL_STYLE);
  style |= PBS_MARQUEE | WS_CHILD | WS_VISIBLE;
  progress_bar_.SetWindowLongPtr(GWL_STYLE, style);
  progress_bar_.SendMessage(PBM_SETMARQUEE, true, 0);
}

LRESULT SplashScreen::OnTimer(UINT, WPARAM, LPARAM, BOOL& handled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(state_, WindowState::STATE_FADING);
  CHECK_GT(alpha_index_, 0);
  if (--alpha_index_) {
    ::SetLayeredWindowAttributes(
        m_hWnd, 0, AlphaScaleToAlphaValue(kAlphaScales[alpha_index_]),
        LWA_ALPHA);
  } else {
    Close();
  }
  handled = true;
  return 0;
}

LRESULT SplashScreen::OnClose(UINT, WPARAM, LPARAM, BOOL& handled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SwitchToState(WindowState::STATE_CLOSED);
  DestroyWindow();
  handled = true;
  return 0;
}

LRESULT SplashScreen::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (timer_created_) {
    CHECK(IsWindow());
    KillTimer(kClosingTimerID);
  }
  if (on_close_closure_) {
    std::move(on_close_closure_).Run();
  }
  handled = true;
  return 0;
}

void SplashScreen::SwitchToState(WindowState new_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = new_state;
  switch (new_state) {
    case WindowState::STATE_CREATED:
    case WindowState::STATE_INITIALIZED:
      break;
    case WindowState::STATE_SHOW_NORMAL:
      alpha_index_ = std::size(kAlphaScales) - 1;
      break;
    case WindowState::STATE_FADING:
      CHECK(IsWindow());
      timer_created_ = SetTimer(kClosingTimerID, kTimerInterval, nullptr) != 0;
      if (!timer_created_) {
        Close();
      }
      break;
    case WindowState::STATE_CLOSED:
      break;
  }
}

void SplashScreen::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != WindowState::STATE_CLOSED && IsWindow()) {
    PostMessage(WM_CLOSE, 0, 0);
  }
}

}  // namespace updater::ui
