// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/win32/dialog.h"

#include <windef.h>
#include <windows.h>
#include <windowsx.h>

#include <functional>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/wchar_utils.h"

typedef std::function<void()> DialogCallback;

using starboard::shared::win32::DebugLogWinError;
using starboard::shared::win32::CStringToWString;

namespace {
HWND g_current_dialog_handle = nullptr;
DialogCallback g_ok_callback;
DialogCallback g_cancel_callback;
}  // namespace

// Wraps the win32 Dialog interface for building Dialogs at runtime.
// https://blogs.msdn.microsoft.com/oldnewthing/20050429-00/?p=35743
class DialogTemplateBuilder {
 public:
  LPCDLGTEMPLATE BuildTemplate() { return (LPCDLGTEMPLATE)&v[0]; }
  void AlignToDword() {
    if (v.size() % 4)
      Write(NULL, 4 - (v.size() % 4));
  }
  void Write(LPCVOID pvWrite, DWORD cbWrite) {
    v.insert(v.end(), cbWrite, 0);
    if (pvWrite)
      CopyMemory(&v[v.size() - cbWrite], pvWrite, cbWrite);
  }
  template <typename T>
  void Write(T t) {
    Write(&t, sizeof(T));
  }
  void WriteString(LPCWSTR psz) {
    Write(psz, (lstrlenW(psz) + 1) * sizeof(WCHAR));
  }

 private:
  std::vector<BYTE> v;
};

INT_PTR CALLBACK DialogProcedureCallback(HWND dialog_handle,
                                         UINT message,
                                         WPARAM w_param,
                                         LPARAM l_param) {
  SB_CHECK(!g_current_dialog_handle || dialog_handle == g_current_dialog_handle)
      << "Received callback on non-active dialog! Only one dialog at a time is "
         "supported.";
  switch (message) {
    case WM_INITDIALOG:
      return TRUE;
    case WM_COMMAND:
      auto command_id = GET_WM_COMMAND_ID(w_param, l_param);
      if (command_id == IDCANCEL) {
        g_cancel_callback();
      } else if (command_id == IDOK) {
        g_ok_callback();
      } else {
        return FALSE;
      }
      EndDialog(dialog_handle, 0);
      g_current_dialog_handle = nullptr;
      return TRUE;
  }
  return FALSE;
}

namespace starboard {
namespace shared {
namespace win32 {

bool ShowOkCancelDialog(HWND hwnd,
                        const std::string& title,
                        const std::string& message,
                        const std::string& ok_message,
                        DialogCallback ok_callback,
                        const std::string& cancel_message,
                        DialogCallback cancel_callback) {
  if (g_current_dialog_handle != nullptr) {
    SB_LOG(WARNING) << "Already showing a dialog; cancelling existing and "
                       "replacing with new dialog";
    CancelDialog();
  }
  g_ok_callback = ok_callback;
  g_cancel_callback = cancel_callback;
  // Get the device context (DC) and from the system so we can scale our fonts
  // correctly.
  HDC hdc = GetDC(NULL);
  SB_CHECK(hdc);
  NONCLIENTMETRICSW ncm = {sizeof(ncm)};
  bool retrieved_system_params =
      SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, 0, &ncm, 0);

  if (!retrieved_system_params) {
    DebugLogWinError();
    ReleaseDC(NULL, hdc);
    return false;
  }
  DialogTemplateBuilder dialog_template;
  const int help_id = 0;
  const int extended_style = 0;

  const int window_width = 200;
  const int window_height = 80;
  const int window_x = 32;
  const int window_y = 32;

  const int edge_padding = 7;

  const int button_height = 14;
  const int button_width = 50;
  const int button_y = window_height - button_height - edge_padding;
  const int left_button_x = window_width / 2 - button_width - edge_padding / 2;
  const int right_button_x = window_width / 2 + edge_padding / 2;
  const int text_width = window_width - edge_padding * 2;
  const int text_height = window_width - edge_padding * 3 - button_height;
  const int extra_data = 0;

  // Create a dialog template.
  // The following MSDN blogposts explains how this is all laid out:
  // https://blogs.msdn.microsoft.com/oldnewthing/20040623-00/?p=38753
  // https://blogs.msdn.microsoft.com/oldnewthing/20050429-00/?p=35743
  // More official documentation:
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms644996(v=vs.85).aspx#modeless_box
  dialog_template.Write<WORD>(1);       // dialog version
  dialog_template.Write<WORD>(0xFFFF);  // extended dialog template
  dialog_template.Write<DWORD>(help_id);
  dialog_template.Write<DWORD>(extended_style);
  dialog_template.Write<DWORD>(WS_CAPTION | WS_SYSMENU | DS_SETFONT |
                               DS_MODALFRAME);
  dialog_template.Write<WORD>(3);  // number of controls
  dialog_template.Write<WORD>(window_x);
  dialog_template.Write<WORD>(window_y);
  dialog_template.Write<WORD>(window_width);
  dialog_template.Write<WORD>(window_height);
  dialog_template.WriteString(L"");  // no menu
  dialog_template.WriteString(L"");  // default dialog class
  // Title.
  dialog_template.WriteString(
      (LPCWSTR)starboard::shared::win32::CStringToWString(title.c_str())
          .c_str());

  // See following for info on how the font styling is calculated:
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ff684173(v=vs.85).aspx
  if (ncm.lfMessageFont.lfHeight < 0) {
    ncm.lfMessageFont.lfHeight =
        -MulDiv(ncm.lfMessageFont.lfHeight, 72, GetDeviceCaps(hdc, LOGPIXELSY));
  }
  dialog_template.Write<WORD>((WORD)ncm.lfMessageFont.lfHeight);
  dialog_template.Write<WORD>((WORD)ncm.lfMessageFont.lfWeight);
  dialog_template.Write<BYTE>(ncm.lfMessageFont.lfItalic);
  dialog_template.Write<BYTE>(ncm.lfMessageFont.lfCharSet);
  dialog_template.WriteString(ncm.lfMessageFont.lfFaceName);

  // Message text.
  dialog_template.AlignToDword();
  dialog_template.Write<DWORD>(help_id);
  dialog_template.Write<DWORD>(extended_style);
  dialog_template.Write<DWORD>(WS_CHILD | WS_VISIBLE);
  dialog_template.Write<WORD>(edge_padding);
  dialog_template.Write<WORD>(edge_padding);
  dialog_template.Write<WORD>(text_width);
  dialog_template.Write<WORD>(text_height);
  dialog_template.Write<DWORD>((DWORD)-1);
  dialog_template.Write<DWORD>(0x0082FFFF);
  dialog_template.WriteString(
      (LPCWSTR)starboard::shared::win32::CStringToWString(message.c_str())
          .c_str());
  dialog_template.Write<WORD>(extra_data);

  // Cancel button.
  dialog_template.AlignToDword();
  dialog_template.Write<DWORD>(help_id);
  dialog_template.Write<DWORD>(extended_style);
  dialog_template.Write<DWORD>(WS_CHILD | WS_VISIBLE | WS_GROUP | WS_TABSTOP |
                               BS_DEFPUSHBUTTON);
  dialog_template.Write<WORD>(left_button_x);
  dialog_template.Write<WORD>(button_y);
  dialog_template.Write<WORD>(button_width);
  dialog_template.Write<WORD>(button_height);
  dialog_template.Write<DWORD>(IDCANCEL);
  dialog_template.Write<DWORD>(0x0080FFFF);
  dialog_template.WriteString(
      (LPCWSTR)starboard::shared::win32::CStringToWString(
          cancel_message.c_str())
          .c_str());
  dialog_template.Write<WORD>(extra_data);

  // Ok button.
  dialog_template.AlignToDword();
  dialog_template.Write<DWORD>(help_id);
  dialog_template.Write<DWORD>(extended_style);
  dialog_template.Write<DWORD>(WS_CHILD | WS_VISIBLE | WS_GROUP | WS_TABSTOP |
                               BS_DEFPUSHBUTTON);  // style
  dialog_template.Write<WORD>(right_button_x);
  dialog_template.Write<WORD>(button_y);
  dialog_template.Write<WORD>(button_width);
  dialog_template.Write<WORD>(button_height);
  dialog_template.Write<DWORD>(IDOK);
  dialog_template.Write<DWORD>(0x0080FFFF);
  dialog_template.WriteString(
      (LPCWSTR)starboard::shared::win32::CStringToWString(ok_message.c_str())
          .c_str());
  dialog_template.Write<WORD>(extra_data);

  ReleaseDC(NULL, hdc);
  // Template is ready - go display it.
  g_current_dialog_handle = CreateDialogIndirect(
      GetModuleHandle(nullptr), dialog_template.BuildTemplate(), hwnd,
      DialogProcedureCallback);
  ShowWindow(g_current_dialog_handle, SW_SHOW);
  return g_current_dialog_handle != nullptr;
}

bool DialogHandleMessage(MSG* msg) {
  return IsWindow(g_current_dialog_handle) &&
         IsDialogMessage(g_current_dialog_handle, msg);
}

void CancelDialog() {
  if (g_current_dialog_handle != nullptr) {
    EndDialog(g_current_dialog_handle, 0);
    g_current_dialog_handle = nullptr;
  }
}

}  //  namespace win32
}  //  namespace shared
}  //  namespace starboard
