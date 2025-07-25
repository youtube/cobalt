// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the Settings advanced page. */

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrSettingsPrefs, SettingsBasicPageElement, SettingsSectionElement} from 'chrome://settings/settings.js';
// <if expr="_google_chrome">
import {loadTimeData} from 'chrome://settings/settings.js';
// </if>
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {getPage, getSection} from './settings_page_test_util.js';

// clang-format on

suite('AdvancedPage', function() {
  let basicPage: SettingsBasicPageElement;

  suiteSetup(function() {
    // <if expr="_google_chrome">
    loadTimeData.overrideValues({showGetTheMostOutOfChromeSection: true});
    // </if>
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const settingsUi = document.createElement('settings-ui');
    document.body.appendChild(settingsUi);
    return CrSettingsPrefs.initialized
        .then(() => {
          return getPage('basic');
        })
        .then(page => {
          basicPage = page as SettingsBasicPageElement;
          const settingsMain =
              settingsUi.shadowRoot!.querySelector('settings-main');
          assertTrue(!!settingsMain);
          settingsMain!.advancedToggleExpanded = true;
          flush();
        });
  });

  /**
   * Verifies that a section is rendered but hidden, including all its subpages.
   * @param section The DOM node for the section.
   */
  function verifySectionWithSubpagesHidden(section: SettingsSectionElement) {
    // Check if there are sub-pages to verify.
    const pages = section.firstElementChild!.shadowRoot!.querySelector(
        'settings-animated-pages');
    if (!pages) {
      return;
    }

    const children =
        pages.shadowRoot!.querySelector('slot')!.assignedNodes({flatten: true})
            .filter(n => n.nodeType === Node.ELEMENT_NODE) as HTMLElement[];

    const stampedChildren = children.filter(function(element) {
      return element.tagName !== 'TEMPLATE';
    });

    // The section's main child should be hidden since only the section
    // corresponding to the current route should be visible.
    const main = stampedChildren.filter(function(element) {
      return element.getAttribute('route-path') === 'default';
    });
    const sectionName = section.section;
    assertEquals(
        main.length, 1, 'default card not found for section ' + sectionName);
    assertEquals(main[0]!.offsetHeight, 0);

    // Any other stamped subpages should also be hidden.
    const subpages = stampedChildren.filter(function(element) {
      return element.getAttribute('route-path') !== 'default';
    });
    for (const subpage of subpages) {
      assertEquals(
          subpage.offsetHeight, 0,
          'Expected subpage #' + subpage.id + ' in ' + sectionName +
              ' not to be visible.');
    }
  }

  test('load page', function() {
    // This will fail if there are any asserts or errors in the Settings page.
  });

  test('advanced pages', function() {
    const sections = ['a11y', 'languages', 'downloads', 'reset'];
    // <if expr="_google_chrome">
    sections.push('getMostChrome');
    // </if>
    for (let i = 0; i < sections.length; i++) {
      const section = getSection(basicPage, sections[i]!);
      assertTrue(!!section);
      verifySectionWithSubpagesHidden(section!);
    }
  });
});
