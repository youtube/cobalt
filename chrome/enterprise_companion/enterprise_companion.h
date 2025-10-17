// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_H_
#define CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_H_

#include <optional>

#include "base/files/file_path.h"

namespace enterprise_companion {

int EnterpriseCompanionMain(int argc, const char* const* argv);

std::optional<base::FilePath> GetLogFilePath();

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_ENTERPRISE_COMPANION_H_
