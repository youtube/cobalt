// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class EmojiPickerTest : public WebUIMochaBrowserTest {
 protected:
  EmojiPickerTest() { set_test_loader_host("emoji-picker"); }
};

// TODO(crbug.com/40749899): Re-enable once flakiness is fixed.
IN_PROC_BROWSER_TEST_F(EmojiPickerTest, DISABLED_Main) {
  RunTest("chromeos/emoji_picker/emoji_picker_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(EmojiPickerTest, Extension) {
  RunTest("chromeos/emoji_picker/emoji_picker_extension_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(EmojiPickerTest, ExtensionEmoji) {
  RunTest("chromeos/emoji_picker/emoji_picker_extension_emoji_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(EmojiPickerTest, ExtensionSymbol) {
  RunTest("chromeos/emoji_picker/emoji_picker_extension_symbol_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(EmojiPickerTest, ExtensionEmoticon) {
  RunTest("chromeos/emoji_picker/emoji_picker_extension_emoticon_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(EmojiPickerTest, Trie) {
  RunTest("chromeos/emoji_picker/emoji_picker_trie_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(EmojiPickerTest, PrefixSearch) {
  RunTest("chromeos/emoji_picker/emoji_picker_prefix_search_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(EmojiPickerTest, Gif) {
  RunTest("chromeos/emoji_picker/emoji_picker_gif_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(EmojiPickerTest, GifValidation) {
  RunTest("chromeos/emoji_picker/emoji_picker_validation_gif_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(EmojiPickerTest, GifSearch) {
  RunTest("chromeos/emoji_picker/emoji_picker_search_gif_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(EmojiPickerTest, GifOffline) {
  RunTest("chromeos/emoji_picker/emoji_picker_offline_gif_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(EmojiPickerTest, GifHttpError) {
  RunTest("chromeos/emoji_picker/emoji_picker_http_error_gif_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(EmojiPickerTest, Scroll) {
  RunTest("chromeos/emoji_picker/emoji_picker_scroll_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(EmojiPickerTest, Load) {
  RunTest("chromeos/emoji_picker/emoji_picker_load_test.js", "mocha.run()");
}

class EmojiPickerVariantGroupingTest : public EmojiPickerTest {
 protected:
  EmojiPickerVariantGroupingTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kImeSystemEmojiPickerVariantGrouping);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(b/295426497): Make tests work with GIF support on.
IN_PROC_BROWSER_TEST_F(EmojiPickerVariantGroupingTest, ExtensionSearch) {
  RunTest("chromeos/emoji_picker/emoji_picker_search_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(EmojiPickerVariantGroupingTest, GlobalVariants) {
  RunTest("chromeos/emoji_picker/emoji_picker_global_variants_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(EmojiPickerVariantGroupingTest, PreferenceStorage) {
  RunTest("chromeos/emoji_picker/emoji_picker_preference_storage_test.js",
          "mocha.run()");
}
