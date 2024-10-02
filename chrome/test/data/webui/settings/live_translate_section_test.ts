// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CaptionsBrowserProxyImpl, LanguageHelper, SettingsLiveTranslateElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData} from 'chrome://settings/settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';

import {TestCaptionsBrowserProxy} from './test_captions_browser_proxy.js';


suite('LiveTranslateSection', function() {
  let languageHelper: LanguageHelper;
  let liveTranslateSection: SettingsLiveTranslateElement;
  let browserProxy: TestCaptionsBrowserProxy;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableLiveCaptionMultiLanguage: true,
      enableLiveTranslate: true,
    });
  });

  setup(async () => {
    const settingsPrefs = document.createElement('settings-prefs');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    const settingsLanguages = document.createElement('settings-languages');
    settingsLanguages.prefs = settingsPrefs.prefs;
    fakeDataBind(settingsPrefs, settingsLanguages, 'prefs');
    document.body.appendChild(settingsLanguages);

    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    // Set up test browser proxy.
    browserProxy = new TestCaptionsBrowserProxy();
    CaptionsBrowserProxyImpl.setInstance(browserProxy);

    liveTranslateSection = document.createElement('settings-live-translate');
    liveTranslateSection.prefs = settingsPrefs.prefs;
    fakeDataBind(settingsPrefs, liveTranslateSection, 'prefs');
    liveTranslateSection.languageHelper = settingsLanguages.languageHelper;
    fakeDataBind(settingsLanguages, liveTranslateSection, 'language-helper');
    document.body.appendChild(liveTranslateSection);

    flush();
    languageHelper = liveTranslateSection.languageHelper;
    return languageHelper.whenReady();
  });

  test('test translate.enable toggle', function() {
    const settingsToggle =
        liveTranslateSection.shadowRoot!.querySelector<HTMLElement>(
            '#liveTranslateToggleButton');
    assertTrue(!!settingsToggle);

    // Clicking on the toggle switches it to true.
    settingsToggle.click();
    let newToggleValue =
        liveTranslateSection
            .getPref('accessibility.captions.live_translate_enabled')
            .value;
    assertTrue(newToggleValue);

    // Clicking on the toggle switches it to false.
    settingsToggle.click();
    newToggleValue =
        liveTranslateSection
            .getPref('accessibility.captions.live_translate_enabled')
            .value;
    assertFalse(newToggleValue);
  });
});
