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

#ifndef STARBOARD_SHARED_WIN32_DIALOG_H_
#define STARBOARD_SHARED_WIN32_DIALOG_H_

#include <windows.h>

#include <functional>
#include <string>

namespace starboard {
namespace shared {
namespace win32 {

typedef std::function<void()> DialogCallback;

// Shows a modeless OK/Cancel-style dialog. Only one dialog may be shown at a
// time.
bool ShowOkCancelDialog(HWND hwnd,
                        const std::string& title,
                        const std::string& message,
                        const std::string& ok_message,
                        DialogCallback ok_callback,
                        const std::string& cancel_message,
                        DialogCallback cancel_callback);

// Cancels the current dialog that is showing, if there is one.
void CancelDialog();

bool DialogHandleMessage(MSG* msg);

}  //  namespace win32
}  //  namespace shared
}  //  namespace starboard

#endif  // STARBOARD_SHARED_WIN32_DIALOG_H_
