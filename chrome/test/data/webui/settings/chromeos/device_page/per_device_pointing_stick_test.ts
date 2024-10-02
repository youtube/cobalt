// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakePointingSticks, fakePointingSticks2, SettingsPerDevicePointingStickElement} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<settings-per-device-pointing-stick>', function() {
  let perDevicePointingStickPage: SettingsPerDevicePointingStickElement;

  setup(async function initializePerDevicePointingStickPage() {
    perDevicePointingStickPage =
        document.createElement('settings-per-device-pointing-stick');
    assert(perDevicePointingStickPage);
    perDevicePointingStickPage.set('pointingSticks', fakePointingSticks);
    document.body.appendChild(perDevicePointingStickPage);
    await flushTasks();
  });

  teardown(() => {
    perDevicePointingStickPage.remove();
  });

  test('PointingStick page updates with new pointing sticks', async () => {
    let subsections = perDevicePointingStickPage.shadowRoot!.querySelectorAll(
        'settings-per-device-pointing-stick-subsection');
    assertEquals(fakePointingSticks.length, subsections.length);

    // Check the number of subsections when the pointing stick list is updated.
    perDevicePointingStickPage.set('pointingSticks', fakePointingSticks2);
    await flushTasks();
    subsections = perDevicePointingStickPage.shadowRoot!.querySelectorAll(
        'settings-per-device-pointing-stick-subsection');
    assertEquals(fakePointingSticks2.length, subsections.length);
  });

  test(
      'Display correct name used for internal/external pointing sticks', () => {
        const subsections =
            perDevicePointingStickPage.shadowRoot!.querySelectorAll(
                'settings-per-device-pointing-stick-subsection');
        for (let i = 0; i < subsections.length; i++) {
          const name =
              subsections[i]!.shadowRoot!.querySelector('h2')!.textContent;
          if (fakePointingSticks[i]!.isExternal) {
            assertEquals(fakePointingSticks[i]!.name, name);
          } else {
            assertTrue(subsections[i]!.i18nExists('builtInPointingStickName'));
            assertEquals('Built-in TrackPoint', name);
          }
        }
      });
});
