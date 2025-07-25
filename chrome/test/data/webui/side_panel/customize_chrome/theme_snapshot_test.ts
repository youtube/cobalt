// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/theme_snapshot.js';

import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {CustomizeThemeType, ThemeSnapshotElement} from 'chrome://customize-chrome-side-panel.top-chrome/theme_snapshot.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {$$, assertStyle, createBackgroundImage, createTheme, installMock} from './test_support.js';

suite('ThemeSnapshotTest', () => {
  let themeSnapshotElement: ThemeSnapshotElement;
  let callbackRouterRemote: CustomizeChromePageRemote;
  let handler: TestMock<CustomizeChromePageHandlerRemote>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    callbackRouterRemote = CustomizeChromeApiProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
  });

  async function createThemeSnapshotElement(): Promise<void> {
    handler.setResultFor('updateTheme', Promise.resolve({
      theme: null,
    }));
    themeSnapshotElement =
        document.createElement('customize-chrome-theme-snapshot');
    document.body.appendChild(themeSnapshotElement);
    await handler.whenCalled('updateTheme');
  }

  test('setting theme updates theme snapshot', async () => {
    // Arrange.
    createThemeSnapshotElement();
    const theme = createTheme();
    theme.backgroundImage = createBackgroundImage('chrome://theme/foo');
    theme.backgroundImage.title = 'foo';

    // Act.
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertEquals(1, handler.getCallCount('updateTheme'));
    const shownPages =
        themeSnapshotElement.shadowRoot!.querySelectorAll('.iron-selected');
    assertTrue(!!shownPages);
    assertEquals(shownPages.length, 1);
    assertEquals(
        shownPages[0]!.getAttribute('theme-type'),
        CustomizeThemeType.CUSTOM_THEME);
    assertEquals(
        $$<HTMLImageElement>(
            themeSnapshotElement, '.snapshot-container #customThemeImage')!
            .getAttribute('aria-labelledby'),
        'customThemeTitle');
    assertEquals(
        'foo',
        $$(themeSnapshotElement,
           '.snapshot-container #customThemeTitle')!.textContent!.trim());
    assertEquals(
        'chrome://theme/foo',
        $$<HTMLImageElement>(
            themeSnapshotElement, '.snapshot-container img')!.src);
  });

  test('not setting a theme updates preview background color', async () => {
    // Arrange.
    createThemeSnapshotElement();
    const theme = createTheme();
    theme.backgroundColor = {value: 4279522202};

    // Act.
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertEquals(1, handler.getCallCount('updateTheme'));
    const shownPages =
        themeSnapshotElement.shadowRoot!.querySelectorAll('.iron-selected');
    assertTrue(!!shownPages);
    assertEquals(shownPages.length, 1);
    assertEquals(
        shownPages[0]!.getAttribute('theme-type'),
        CustomizeThemeType.CLASSIC_CHROME);
    assertEquals(
        $$<HTMLImageElement>(
            themeSnapshotElement, '#classicChromeBackground img')!.src,
        'chrome://customize-chrome-side-panel.top-chrome/icons/' +
            'mini_new_tab_page.svg');
    assertEquals(
        $$<HTMLImageElement>(
            themeSnapshotElement,
            '#classicChromeBackground img')!.getAttribute('aria-labelledby'),
        'classicChromeThemeTitle');
    assertEquals(
        'Default Chrome',
        $$(themeSnapshotElement,
           '.snapshot-container #classicChromeThemeTitle')!.textContent!
            .trim());
    assertStyle(
        $$(themeSnapshotElement,
           '.snapshot-container #classicChromeBackground')!,
        'background-color', 'rgb(20, 83, 154)');
  });

  test('uploading a background updates theme snapshot', async () => {
    // Arrange.
    createThemeSnapshotElement();
    const theme = createTheme();
    theme.backgroundImage = createBackgroundImage('chrome://theme/foo');
    theme.backgroundImage.isUploadedImage = true;

    // Act.
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    // Assert.
    assertEquals(1, handler.getCallCount('updateTheme'));
    const shownPages =
        themeSnapshotElement.shadowRoot!.querySelectorAll('.iron-selected');
    assertTrue(!!shownPages);
    assertEquals(shownPages.length, 1);
    assertEquals(
        shownPages[0]!.getAttribute('theme-type'),
        CustomizeThemeType.UPLOADED_IMAGE);
    assertEquals(
        $$(themeSnapshotElement, '.snapshot-container #uploadedThemeImage')!
            .getAttribute('aria-labelledby'),
        'uploadedThemeTitle');
    assertEquals(
        'Uploaded image',
        $$(themeSnapshotElement,
           '.snapshot-container #uploadedThemeTitle')!.textContent!.trim());
  });

  test(
      'clicking snapshot with chrome-refresh-2023 toggled off ' +
          'does not create an edit-theme-click event',
      async () => {
        // Arrange.
        document.documentElement.toggleAttribute('chrome-refresh-2023', false);
        createThemeSnapshotElement();
        let clicked = false;
        themeSnapshotElement.addEventListener(
            'edit-theme-click', () => clicked = true);
        // Act
        $$<HTMLElement>(
            themeSnapshotElement,
            '.snapshot-container #classicChromeBackground')!.click();
        // Assert
        assertFalse(clicked);
      });

  suite('chrome refresh 2023', () => {
    suiteSetup(() => {
      document.documentElement.toggleAttribute('chrome-refresh-2023', true);
    });

    test('classic chrome snapshot shows correct image', async () => {
      // Arrange.
      createThemeSnapshotElement();
      const theme = createTheme();

      // Act.
      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      // Assert.
      assertEquals(1, handler.getCallCount('updateTheme'));
      const shownPages =
          themeSnapshotElement.shadowRoot!.querySelectorAll('.iron-selected');
      assertTrue(!!shownPages);
      assertEquals(shownPages.length, 1);
      assertEquals(
          shownPages[0]!.getAttribute('theme-type'),
          CustomizeThemeType.CLASSIC_CHROME);
      assertEquals(
          $$<SVGUseElement>(
              themeSnapshotElement,
              '#classicChromeBackground svg use')!.href.baseVal,
          'icons/gm3_mini_new_tab_page.svg#miniNewTabPage');
    });

    test(
        'clicking classic chrome snapshot creates edit-theme-click event',
        async () => {
          // Arrange.
          createThemeSnapshotElement();
          let clicked = false;
          themeSnapshotElement.addEventListener(
              'edit-theme-click', () => clicked = true);
          // Act
          $$<HTMLElement>(
              themeSnapshotElement,
              '.snapshot-container #classicChromeBackground')!.click();
          // Assert
          assertTrue(clicked);
        });

    test(
        'clicking custom theme snapshot creates edit-theme-click event',
        async () => {
          // Arrange.
          createThemeSnapshotElement();
          let clicked = false;
          themeSnapshotElement.addEventListener(
              'edit-theme-click', () => clicked = true);
          // Act
          $$<HTMLElement>(
              themeSnapshotElement,
              '.snapshot-container #customThemeImageBackground')!.click();
          // Assert
          assertTrue(clicked);
        });

    test(
        'clicking uploaded snapshot creates edit-theme-click event',
        async () => {
          // Arrange.
          createThemeSnapshotElement();
          let clicked = false;
          themeSnapshotElement.addEventListener(
              'edit-theme-click', () => clicked = true);
          // Act
          $$<HTMLElement>(
              themeSnapshotElement,
              '.snapshot-container #uploadedThemeImageBackground')!.click();
          // Assert
          assertTrue(clicked);
        });
  });
});
