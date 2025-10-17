// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';

import type {WallpaperGridItemElement} from 'chrome://resources/ash/common/personalization/wallpaper_grid_item_element.js';
import {getSeaPenTemplates} from 'chrome://resources/ash/common/sea_pen/constants.js';
import type {SeaPenRouterElement} from 'chrome://resources/ash/common/sea_pen/sea_pen_router_element.js';
import {SeaPenPaths} from 'chrome://resources/ash/common/sea_pen/sea_pen_router_element.js';
import type {SeaPenTemplateQueryElement} from 'chrome://resources/ash/common/sea_pen/sea_pen_template_query_element.js';
import {setTransitionsEnabled} from 'chrome://resources/ash/common/sea_pen/transition.js';
import type {VcBackgroundApp} from 'chrome://vc-background/js/vc_background_app.js';
import type {VcBackgroundBreadcrumbElement} from 'chrome://vc-background/js/vc_background_breadcrumb_element.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('VcBackgroundUITest', () => {
  setup(() => {
    // Disables page transition by default.
    setTransitionsEnabled(false);
  });

  function getVcBackgroundApp(): VcBackgroundApp {
    const vcBackgroundApp = document.body.querySelector('vc-background-app');
    assertTrue(!!vcBackgroundApp, 'vc-background-app exists');
    return vcBackgroundApp;
  }

  function getVcBackgroundBreadcrumbs(): VcBackgroundBreadcrumbElement {
    const vcBackgroundBreadcrumb =
        getVcBackgroundApp().shadowRoot!.querySelector(
            'vc-background-breadcrumb')!;
    assertTrue(!!vcBackgroundBreadcrumb, 'vc-background-breadcrumb exists');
    return vcBackgroundBreadcrumb;
  }

  function getVcBackgroundBreadcrumbsText(): string[] {
    const breadcrumbElements =
        getVcBackgroundBreadcrumbs().shadowRoot!.querySelectorAll('cr-button');
    return Array.from(breadcrumbElements).map(el => el.textContent!);
  }

  function getSeaPenRouter(): SeaPenRouterElement {
    const seaPenRouter =
        getVcBackgroundApp().shadowRoot!.querySelector('sea-pen-router');
    assertTrue(!!seaPenRouter, 'sea-pen-router exists');
    return seaPenRouter;
  }

  function getSeaPenTemplateQuery(): SeaPenTemplateQueryElement|null {
    return getSeaPenRouter().shadowRoot!.querySelector(
        'sea-pen-template-query');
  }

  function getSeaPenTemplateElements(): WallpaperGridItemElement[] {
    const seaPenTemplates =
        getSeaPenRouter().shadowRoot?.querySelector('sea-pen-templates');
    assertTrue(!!seaPenTemplates, 'sea-pen-templates exists');
    return Array.from(seaPenTemplates.shadowRoot!.querySelectorAll(
        'wallpaper-grid-item:not([hidden])'));
  }

  test('initial breadcrumbs', () => {
    assertArrayEquals(
        getVcBackgroundBreadcrumbsText(),
        [getVcBackgroundBreadcrumbs().i18n('vcBackgroundLabel')]);
  });

  test('shows template options when template is clicked', async () => {
    assertFalse(
        !!getSeaPenTemplateQuery(),
        'sea-pen-template-query does not exist yet');

    const seaPenTemplateElements = getSeaPenTemplateElements();
    assertEquals(
        getSeaPenTemplates().length, seaPenTemplateElements.length,
        'each sea pen template is displayed');

    // Click the 'Classic art' template.
    seaPenTemplateElements
        .find(template => {
          const p = template.shadowRoot?.querySelector('p.primary-text');
          return p?.textContent?.trim() === 'Classic art';
        })!.click();
    await waitAfterNextRender(getSeaPenRouter());

    const seaPenTemplateQuery = getSeaPenTemplateQuery();
    assertTrue(!!seaPenTemplateQuery, 'sea-pen-template-query exists');

    const templateTokens =
        seaPenTemplateQuery.shadowRoot!.querySelectorAll<HTMLElement>(
            '.chip-text, .template-text');
    assertTrue(
        templateTokens.length > 0, 'template tokens should be available');

    const templateText: string[] = [];
    templateTokens.forEach((currentToken: HTMLElement) => {
      if (currentToken.classList.contains('chip-text')) {
        templateText.push(
            currentToken.shadowRoot?.getElementById('chipText')?.innerText ??
            '');
      } else {
        templateText.push(currentToken.innerText ?? '');
      }
    });

    assertEquals(
        'A painting of a field of flowers in the avant-garde style',
        templateText.join(' '),
        'Expected template text is shown for Classic art');

    assertEquals(
        'chrome://vc-background/results?seaPenTemplateId=104',
        window.location.href,
        'VC Background Classic art template id is added to url');

    // Breadcrumbs should show 'Classic art'.
    assertArrayEquals(getVcBackgroundBreadcrumbsText(), [
      getVcBackgroundBreadcrumbs().i18n('vcBackgroundLabel'),
      'Classic art',
    ]);
  });

  test(
      'verifies breadcrumbs when create button clicked in a template page',
      async () => {
        const seaPenRouter = getSeaPenRouter();
        seaPenRouter.goToRoute(SeaPenPaths.TEMPLATES);
        await waitAfterNextRender(seaPenRouter);

        const seaPenTemplateElements = getSeaPenTemplateElements();
        await waitAfterNextRender(seaPenRouter);

        // Click the 'Classic art' template.
        seaPenTemplateElements
            .find(template => {
              const p = template.shadowRoot?.querySelector('p.primary-text');
              return p?.textContent?.trim() === 'Classic art';
            })!.click();
        await waitAfterNextRender(seaPenRouter);

        const seaPenTemplateQuery = getSeaPenTemplateQuery();
        assertTrue(!!seaPenTemplateQuery, 'sea-pen-template-query exists');

        // Click on the 'Create' button.
        const createButton =
            seaPenTemplateQuery.shadowRoot?.querySelector<HTMLButtonElement>(
                'cr-button#searchButton')!;
        createButton.click();

        await waitAfterNextRender(seaPenTemplateQuery);

        assertEquals(
            'chrome://vc-background/results?seaPenTemplateId=104',
            window.location.href,
            'App is on /results and Classic art template id is added to url');

        // Breadcrumbs should show 'Classic art'.
        assertArrayEquals(getVcBackgroundBreadcrumbsText(), [
          getVcBackgroundBreadcrumbs().i18n('vcBackgroundLabel'),
          'Classic art',
        ]);
      });

  test('allows changing templates via breadcrumbs dropdown menu', async () => {
    // Navigate directly to a results page with template in breadcrumbs.
    const seaPenRouter = getSeaPenRouter();
    seaPenRouter.goToRoute(SeaPenPaths.RESULTS, {seaPenTemplateId: '104'});
    await waitAfterNextRender(seaPenRouter);

    const breadcrumbElement = getVcBackgroundBreadcrumbs();

    const classicArtTitle = 'Classic art';
    // Breadcrumbs should show 'Classic art'.
    assertArrayEquals(
        getVcBackgroundBreadcrumbsText(),
        [breadcrumbElement.i18n('vcBackgroundLabel'), classicArtTitle]);

    const dropdownMenu =
        breadcrumbElement.shadowRoot!.querySelector('cr-action-menu');
    assertTrue(!!dropdownMenu);
    assertFalse(dropdownMenu.open, 'the action menu should not open yet');

    // Verify the selected template.
    const selectedElement =
        dropdownMenu.querySelectorAll('button[aria-checked=\'true\']');
    assertEquals(
        1, selectedElement.length, 'there should be one template selected');
    assertEquals(
        classicArtTitle, (selectedElement[0] as HTMLElement).innerText.trim(),
        `selected template in the dropdown should be ${classicArtTitle}`);

    // Activate the dropdown menu.
    const lastBreadcrumb =
        breadcrumbElement.shadowRoot!.querySelector<HTMLElement>(
            'cr-button#breadcrumb1');
    assertTrue(!!lastBreadcrumb);
    lastBreadcrumb.click();

    await waitAfterNextRender(breadcrumbElement);

    assertTrue(dropdownMenu.open, 'the action menu should be open');

    // The dropdown should show the sea pen templates.
    const allMenuItems = dropdownMenu.querySelectorAll('button');
    assertEquals(allMenuItems.length, getSeaPenTemplates().length);

    // Click on the 'Office' template.
    const officeTitle = 'Stylish office';
    Array.from(allMenuItems)
        .find(el => el.textContent?.trim() === officeTitle)!.click();

    await waitAfterNextRender(breadcrumbElement);

    // We should be redirected to the new template page.
    assertEquals(
        'chrome://vc-background/results?seaPenTemplateId=101',
        window.location.href,
        'App is on /results and Office template id is added to url');

    // Breadcrumbs should now show 'Stylish office'.
    assertArrayEquals(
        getVcBackgroundBreadcrumbsText(),
        [getVcBackgroundBreadcrumbs().i18n('vcBackgroundLabel'), officeTitle]);
  });
});
