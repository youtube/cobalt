// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Tests for the <emoji-picker> Polymer component.
 */

GEN_INCLUDE([
  '//chrome/test/data/webui/polymer_browser_test_base.js',
]);

GEN('#include "ash/constants/ash_features.h"');
GEN('#include "content/public/test/browser_test.h"');

// This file bootstraps the other tests written in Javascript.
class EmojiPickerBrowserTest extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://emoji-picker';
  }

  /** @override */
  get featureList() {
    return {enabled: ['ash::features::kImeSystemEmojiPickerClipboard']};
  }
}

// Tests behaviour of <emoji-picker> component.
var EmojiPickerMainTest = class extends EmojiPickerBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://emoji-picker/test_loader.html?module=' +
        'chromeos/emoji_picker/emoji_picker_test.js';
  }
};


// TODO(https://crbug.com/1179762): Re-enable once flakiness is fixed.
TEST_F('EmojiPickerMainTest', 'DISABLED_All', function() {
  mocha.run();
});

var EmojiPickerExtensionBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://emoji-picker/test_loader.html?module=' +
        'chromeos/emoji_picker/emoji_picker_extension_test.js';
  }
};

TEST_F('EmojiPickerExtensionBrowserTest', 'All', function() {
  mocha.run();
});

var EmojiPickerExtensionEmojiTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://emoji-picker/test_loader.html?module=' +
        'chromeos/emoji_picker/emoji_picker_extension_emoji_test.js';
  }
};

TEST_F('EmojiPickerExtensionEmojiTest', 'All', function() {
  mocha.run();
});

var EmojiPickerExtensionSymbolTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://emoji-picker/test_loader.html?module=' +
        'chromeos/emoji_picker/emoji_picker_extension_symbol_test.js';
  }
};

TEST_F('EmojiPickerExtensionSymbolTest', 'All', function() {
  mocha.run();
});

var EmojiPickerExtensionEmoticonTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://emoji-picker/test_loader.html?module=chromeos/' +
        'emoji_picker/emoji_picker_extension_emoticon_test.js';
  }
};

TEST_F('EmojiPickerExtensionEmoticonTest', 'All', function() {
  mocha.run();
});

var EmojiPickerExtensionSearchTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://emoji-picker/test_loader.html?module=' +
        'chromeos/emoji_picker/emoji_picker_search_test.js';
  }
};

TEST_F('EmojiPickerExtensionSearchTest', 'All', function() {
  mocha.run();
});

var EmojiPickerTrieTest = class extends PolymerTest {
  /** @override */
  get featureList() {
    return {
      enabled: ['ash::features::kImeSystemEmojiPickerSearchExtension'],
    };
  }
  /** @override */
  get browsePreload() {
    return 'chrome://emoji-picker/test_loader.html?module=' +
        'chromeos/emoji_picker/emoji_picker_trie_test.js';
  }
};

TEST_F('EmojiPickerTrieTest', 'All', function() {
  mocha.run();
});

var EmojiPickerPrefixSearchTest =
    class extends EmojiPickerExtensionBrowserTest {
  /** @override */
  get browsePreload() {
    return 'chrome://emoji-picker/test_loader.html?module=' +
        'chromeos/emoji_picker/emoji_picker_prefix_search_test.js';
  }
};

TEST_F('EmojiPickerPrefixSearchTest', 'All', function() {
  mocha.run();
});

var EmojiPickerGifTest = class extends PolymerTest {
  /** @override */
  get featureList() {
    return {enabled: ['ash::features::kImeSystemEmojiPickerGIFSupport']};
  }

  /** @override */
  get browsePreload() {
    return 'chrome://emoji-picker/test_loader.html?module=' +
        'chromeos/emoji_picker/emoji_picker_gif_test.js';
  }
};

TEST_F('EmojiPickerGifTest', 'All', function() {
  mocha.run();
});

var EmojiPickerGifValidationTest = class extends PolymerTest {
  /** @override */
  get featureList() {
    return {enabled: ['ash::features::kImeSystemEmojiPickerGIFSupport']};
  }

  /** @override */
  get browsePreload() {
    return 'chrome://emoji-picker/test_loader.html?module=' +
        'chromeos/emoji_picker/emoji_picker_validation_gif_test.js';
  }
};

TEST_F('EmojiPickerGifValidationTest', 'All', function() {
  mocha.run();
});

var EmojiPickerGifSearchTest = class extends PolymerTest {
  /** @override */
  get featureList() {
    return {enabled: ['ash::features::kImeSystemEmojiPickerGIFSupport']};
  }

  /** @override */
  get browsePreload() {
    return 'chrome://emoji-picker/test_loader.html?module=' +
        'chromeos/emoji_picker/emoji_picker_search_gif_test.js';
  }
};

TEST_F('EmojiPickerGifSearchTest', 'All', function() {
  mocha.run();
});

var EmojiPickerGifOfflineTest = class extends PolymerTest {
  /** @override */
  get featureList() {
    return {enabled: ['ash::features::kImeSystemEmojiPickerGIFSupport']};
  }

  /** @override */
  get browsePreload() {
    return 'chrome://emoji-picker/test_loader.html?module=' +
        'chromeos/emoji_picker/emoji_picker_offline_gif_test.js';
  }
};

TEST_F('EmojiPickerGifOfflineTest', 'All', function() {
  mocha.run();
});

var EmojiPickerGifHttpErrorTest = class extends PolymerTest {
  /** @override */
  get featureList() {
    return {enabled: ['ash::features::kImeSystemEmojiPickerGIFSupport']};
  }

  /** @override */
  get browsePreload() {
    return 'chrome://emoji-picker/test_loader.html?module=' +
        'chromeos/emoji_picker/emoji_picker_http_error_gif_test.js';
  }
};

TEST_F('EmojiPickerGifHttpErrorTest', 'All', function() {
  mocha.run();
});
