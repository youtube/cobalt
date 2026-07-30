// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PrefsBrowserProxy, PrefService, PrefServiceObserverMixin} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

const TestPolymerObserverBase = PrefServiceObserverMixin(PolymerElement);
class TestPolymerObserverElement extends TestPolymerObserverBase {
  static get is() {
    return 'test-polymer-observer';
  }

  static get properties() {
    return {
      foo: Object,
      bar: Object,
    };
  }

  declare foo?: chrome.settingsPrivate.PrefObject<number>;
  declare bar?: chrome.settingsPrivate.PrefObject<string>;
}

customElements.define(
    TestPolymerObserverElement.is, TestPolymerObserverElement);

suite('PrefServiceObserverMixin', function() {
  let proxy: TestPrefsBrowserProxy;
  let service: PrefService;
  let element: TestPolymerObserverElement;

  const initialPrefs = [
    {
      key: 'test.number',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 10,
    },
    {
      key: 'test.string',
      type: chrome.settingsPrivate.PrefType.STRING,
      value: 'hello',
    },
  ];

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    proxy = new TestPrefsBrowserProxy(initialPrefs);
    PrefsBrowserProxy.setInstance(proxy);
    PrefService.resetInstanceForTesting();
    service = PrefService.getInstance();
    await service.whenInitialized();

    element = document.createElement('test-polymer-observer') as
        TestPolymerObserverElement;
    document.body.appendChild(element);
  });


  test('mirrorPref', async function() {
    element.mirrorPref('test.number', 'foo');
    await Promise.resolve();
    assertTrue(!!element.foo);
    assertEquals(10, element.foo.value);

    proxy.fakeApi.sendPrefChanges([{key: 'test.number', value: 20}]);
    await Promise.resolve();
    assertEquals(20, element.foo.value);
  });

  test('mirrorPrefs', async function() {
    element.mirrorPrefs({
      'test.number': 'foo',
      'test.string': 'bar',
    });
    await Promise.resolve();
    assertTrue(!!element.foo);
    assertEquals(10, element.foo.value);
    assertTrue(!!element.bar);
    assertEquals('hello', element.bar.value);

    proxy.fakeApi.sendPrefChanges([
      {key: 'test.number', value: 30},
      {key: 'test.string', value: 'world'},
    ]);
    await Promise.resolve();
    assertEquals(30, element.foo.value);
    assertEquals('world', element.bar.value);
  });

  test('removePrefObserver', async function() {
    const id = element.addPrefObserver<number>('test.number', pref => {
      element.foo = pref;
    });
    await Promise.resolve();
    assertTrue(!!element.foo);
    assertEquals(10, element.foo.value);

    element.removePrefObserver(id);

    proxy.fakeApi.sendPrefChanges([{key: 'test.number', value: 20}]);
    await Promise.resolve();
    // Should not have updated.
    assertEquals(10, element.foo.value);
  });

  test('disconnectedCallback', async function() {
    element.mirrorPref('test.number', 'foo');
    await Promise.resolve();
    assertTrue(!!element.foo);
    assertEquals(10, element.foo.value);

    element.remove();

    proxy.fakeApi.sendPrefChanges([{key: 'test.number', value: 20}]);
    await Promise.resolve();
    assertEquals(10, element.foo.value);
  });
});
