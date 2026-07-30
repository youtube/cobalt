// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/theme_handler.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/common/chrome_features.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

class ThemeHandlerTest : public testing::Test {
 protected:
  // Creates a dummy extension for the given theme dictionary.
  // TODO(crbug.com/41317803): Consider changing the return type to
  // base::expected<scoped_refptr<Extension>, std::u16string>.
  scoped_refptr<Extension> CreateExtension(base::DictValue&& theme_dict,
                                           std::u16string& error) {
    base::DictValue manifest;
    manifest.Set(keys::kManifestVersion, 3);
    manifest.Set(keys::kName, "My Theme");
    manifest.Set(keys::kVersion, "1.0");
    manifest.Set(keys::kTheme, std::move(theme_dict));

    return Extension::Create(base::FilePath(),
                             mojom::ManifestLocation::kInternal, manifest,
                             Extension::NO_FLAGS, &error);
  }
};

TEST_F(ThemeHandlerTest, EmptyThemeDictionary) {
  // Empty |theme| dictionary should be considered valid and thus create an
  // |extension|.
  base::DictValue theme = base::DictValue();
  std::u16string error;
  scoped_refptr<Extension> extension = CreateExtension(std::move(theme), error);
  EXPECT_TRUE(extension);
}


}  // namespace extensions
