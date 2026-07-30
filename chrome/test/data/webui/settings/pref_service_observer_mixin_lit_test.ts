// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {PrefsBrowserProxy, PrefService, PrefServiceObserverMixinLit} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

const TestLitObserverBase = PrefServiceObserverMixinLit(CrLitElement);
class TestLitObserverElement extends TestLitObserverBase {
  static get is() {
    return 'test-lit-observer';
  }

  static override get properties() {
    return {
      foo: {type: Object},
      bar: {type: Object},
    };
  }

  accessor foo: chrome.settingsPrivate.PrefObject<number>|undefined;
  accessor bar: chrome.settingsPrivate.PrefObject<string>|undefined;
}

customElements.define(TestLitObserverElement.is, TestLitObserverElement);

suite('PrefServiceObserverMixinLit', function() {
  let proxy: TestPrefsBrowserProxy;
  let service: PrefService;
  let element: TestLitObserverElement;

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

    element =
        document.createElement('test-lit-observer') as TestLitObserverElement;
    document.body.appendChild(element);
  });


  test('mirrorPref', async function() {
    element.mirrorPref('test.number', 'foo');
    await microtasksFinished();
    assertTrue(!!element.foo);
    assertEquals(10, element.foo.value);

    proxy.fakeApi.sendPrefChanges([{key: 'test.number', value: 20}]);
    await microtasksFinished();
    assertEquals(20, element.foo.value);
  });

  test('mirrorPrefs', async function() {
    element.mirrorPrefs({
      'test.number': 'foo',
      'test.string': 'bar',
    });
    await microtasksFinished();
    assertTrue(!!element.foo);
    assertEquals(10, element.foo.value);
    assertTrue(!!element.bar);
    assertEquals('hello', element.bar.value);

    proxy.fakeApi.sendPrefChanges([
      {key: 'test.number', value: 30},
      {key: 'test.string', value: 'world'},
    ]);
    await microtasksFinished();
    assertEquals(30, element.foo.value);
    assertEquals('world', element.bar.value);
  });

  test('removePrefObserver', async function() {
    const id = element.addPrefObserver<number>('test.number', pref => {
      element.foo = pref;
    });
    await microtasksFinished();
    assertTrue(!!element.foo);
    assertEquals(10, element.foo.value);

    element.removePrefObserver(id);

    proxy.fakeApi.sendPrefChanges([{key: 'test.number', value: 20}]);
    await microtasksFinished();
    assertEquals(10, element.foo.value);
  });

  test('disconnectedCallback', async function() {
    element.mirrorPref('test.number', 'foo');
    await microtasksFinished();
    assertTrue(!!element.foo);
    assertEquals(10, element.foo.value);

    element.remove();

    proxy.fakeApi.sendPrefChanges([{key: 'test.number', value: 20}]);
    await microtasksFinished();
    assertEquals(10, element.foo.value);
  });
});
