// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/side_panel/side_panel_app.js';

import type {SearchboxGhostLoaderElement} from 'chrome-untrusted://lens/lens/shared/searchbox_ghost_loader.js';
import type {LensSidePanelAppElement} from 'chrome-untrusted://lens/side_panel/side_panel_app.js';
import {SidePanelBrowserProxyImpl} from 'chrome-untrusted://lens/side_panel/side_panel_browser_proxy.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';

import {TestLensSidePanelBrowserProxy} from './test_side_panel_browser_proxy.js';

suite('SearchboxBackButton', () => {
  let testBrowserProxy: TestLensSidePanelBrowserProxy;
  let lensSidePanelElement: LensSidePanelAppElement;

  setup(async () => {
    testBrowserProxy = new TestLensSidePanelBrowserProxy();
    SidePanelBrowserProxyImpl.setInstance(testBrowserProxy);

    loadTimeData.overrideValues({showContextualSearchboxGhostLoader: true});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    lensSidePanelElement = document.createElement('lens-side-panel-app');
    document.body.appendChild(lensSidePanelElement);
    await waitAfterNextRender(lensSidePanelElement);
  });

  test('hide ghost loader', async () => {
    lensSidePanelElement.makeGhostLoaderVisibleForTesting();
    const ghostLoader = lensSidePanelElement.shadowRoot!
                            .querySelector<SearchboxGhostLoaderElement>(
                                'cr-searchbox-ghost-loader')!;
    await waitAfterNextRender(lensSidePanelElement);
    assertTrue(isVisible(ghostLoader));
    await waitAfterNextRender(lensSidePanelElement);
    // Notify side panel to hide ghost loader.
    testBrowserProxy.page.suppressGhostLoader();
    await waitAfterNextRender(lensSidePanelElement);
    assertFalse(isVisible(ghostLoader));
  });

  test('input resets ghost loader loading state', async () => {
    const ghostLoader = lensSidePanelElement.shadowRoot!
                            .querySelector<SearchboxGhostLoaderElement>(
                                'cr-searchbox-ghost-loader')!;
    ghostLoader.showErrorStateForTesting();
    lensSidePanelElement.makeGhostLoaderVisibleForTesting();
    await waitAfterNextRender(lensSidePanelElement);
    assertTrue(isVisible(ghostLoader.shadowRoot!.getElementById('errorState')));
    // Notify side panel to reset the ghost loader to loading state.
    assertTrue(isVisible(lensSidePanelElement.$.searchbox));
    // Mock sending input to the searchbox.
    lensSidePanelElement.$.searchbox.$.input.value = 'hello';
    lensSidePanelElement.dispatchEvent(new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      key: 'o',
    }));

    await waitAfterNextRender(lensSidePanelElement);
    // State should be switched back to loading state after any input.
    assertTrue(
        isVisible(ghostLoader.shadowRoot!.getElementById('loadingState')));
  });

  test('click resets ghost loader loading state', async () => {
    const ghostLoader = lensSidePanelElement.shadowRoot!
                            .querySelector<SearchboxGhostLoaderElement>(
                                'cr-searchbox-ghost-loader')!;
    ghostLoader.showErrorStateForTesting();
    lensSidePanelElement.makeGhostLoaderVisibleForTesting();
    await waitAfterNextRender(lensSidePanelElement);
    assertTrue(isVisible(ghostLoader.shadowRoot!.getElementById('errorState')));
    // Click into the searchbox.
    lensSidePanelElement.$.searchbox.dispatchEvent(
        new KeyboardEvent('mousedown', {
          bubbles: true,
          cancelable: true,
        }));

    await waitAfterNextRender(lensSidePanelElement);
    // State should be switched back to loading state clicking into searchbox.
    assertTrue(
        isVisible(ghostLoader.shadowRoot!.getElementById('loadingState')));
  });
});
