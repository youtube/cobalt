// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "build/chromeos_buildflags.h"');
GEN('#include "content/public/test/browser_test.h"');

GEN('#if !BUILDFLAG(IS_LINUX)');
GEN('#include "ui/base/ui_base_features.h"');
GEN('#endif');

var CssTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    throw new Error('Should be overriden by subclasses');
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @override */
  get webuiHost() {
    return 'dummyurl';
  }
};

var TextDefaultsTest = class extends CssTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=css/text_defaults_test.js';
  }
};

TEST_F('TextDefaultsTest', 'All', function() {
  runMochaSuite('TextDefaults')
});

GEN('#if !BUILDFLAG(IS_LINUX)');
var TextDefaultsSystemFontTest = class extends TextDefaultsTest {
  /** @override */
  get featureList() {
    return {
      enabled: [
        'features::kWebUiSystemFont',
      ],
    };
  }
}

TEST_F('TextDefaultsSystemFontTest', 'All', function() {
  runMochaSuite('TextDefaultsSystemFont')
});
GEN('#endif')

var ColorProviderCSSColorsTest = class extends CssTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=css/color_provider_css_colors_test.js';
  }
};

TEST_F('ColorProviderCSSColorsTest', 'All', function() {
  mocha.run();
});

GEN('#if BUILDFLAG(IS_CHROMEOS_ASH)');

var ColorProviderCSSColorsTestChromeOS =
    class extends ColorProviderCSSColorsTest {
  /** @override */
  get browsePreload() {
    return 'chrome://webui-test/test_loader.html?module=css/color_provider_css_colors_test_chromeos.js';
  }
};

TEST_F('ColorProviderCSSColorsTestChromeOS', 'All', function() {
  mocha.run();
});

GEN('#endif  // BUILDFLAG(IS_CHROMEOS_ASH)');
