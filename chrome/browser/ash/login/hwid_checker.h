// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_HWID_CHECKER_H_
#define CHROME_BROWSER_ASH_LOGIN_HWID_CHECKER_H_

#include "base/strings/string_piece_forward.h"

namespace ash {

// Checks if given HWID correct.
bool IsHWIDCorrect(base::StringPiece hwid);

// Checks if current machine has correct HWID.
bool IsMachineHWIDCorrect();

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_HWID_CHECKER_H_
