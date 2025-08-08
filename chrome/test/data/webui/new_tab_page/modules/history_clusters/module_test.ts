// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Cart} from 'chrome://new-tab-page/cart.mojom-webui.js';
import {Discount} from 'chrome://new-tab-page/discount.mojom-webui.js';
import {Cluster, URLVisit} from 'chrome://new-tab-page/history_cluster_types.mojom-webui.js';
import {PageHandlerRemote} from 'chrome://new-tab-page/history_clusters.mojom-webui.js';
import {LayoutType} from 'chrome://new-tab-page/history_clusters_layout_type.mojom-webui.js';
import {DismissModuleEvent, HistoryClusterElementType, HistoryClusterImageDisplayState, historyClustersDescriptor, HistoryClustersModuleElement, HistoryClustersProxyImpl, LAYOUT_1_MIN_IMAGE_VISITS, LAYOUT_1_MIN_VISITS, LAYOUT_2_MIN_IMAGE_VISITS, LAYOUT_2_MIN_VISITS, LAYOUT_3_MIN_IMAGE_VISITS, LAYOUT_3_MIN_VISITS, PageImageServiceBrowserProxy, TileModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$, NewTabPageProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {PageRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {PageImageServiceHandlerRemote} from 'chrome://resources/cr_components/page_image_service/page_image_service.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {installMock} from '../../test_support.js';

import {assertModuleHeaderTitle, createRelatedSearches, createSampleVisits, GOOGLE_SEARCH_BASE_URL, MIN_RELATED_SEARCHES} from './test_support.js';

const DISPLAY_LAYOUT_METRIC_NAME = 'NewTabPage.HistoryClusters.DisplayLayout';

function assertLayoutSet(
    moduleElement: HistoryClustersModuleElement, layoutType: LayoutType) {
  const layoutElements = moduleElement.shadowRoot!.querySelectorAll('.layout');
  assertEquals(layoutType, moduleElement.layoutType);
  assertEquals(layoutElements.length, 1);
  assertEquals(layoutElements[0]!.id, `layout${layoutType}`);
}

function createLayoutSuitableSampleVisits(
    layoutType: LayoutType = LayoutType.kLayout1): URLVisit[] {
  switch (layoutType) {
    case LayoutType.kLayout1:
      return createSampleVisits(LAYOUT_1_MIN_VISITS, LAYOUT_1_MIN_IMAGE_VISITS);
    case LayoutType.kLayout2:
      return createSampleVisits(LAYOUT_2_MIN_VISITS, LAYOUT_2_MIN_IMAGE_VISITS);
    case LayoutType.kLayout3:
      return createSampleVisits(LAYOUT_3_MIN_VISITS, LAYOUT_3_MIN_IMAGE_VISITS);
  }
  return [];
}

function createLayoutSuitableSampleCluster(
    layout: LayoutType, numRelatedSearches: number = MIN_RELATED_SEARCHES) {
  return createSampleCluster({
    visits: createLayoutSuitableSampleVisits(layout),
    relatedSearches: createRelatedSearches(numRelatedSearches),
  });
}

function checkInfoDialogContent(
    moduleElement: HistoryClustersModuleElement, id: string) {
  const expectedInfo =
      loadTimeData.getString(id).replaceAll('\n', '').replaceAll('<br>', '');
  const actualInfo =
      moduleElement.$.infoDialogRender.get().textContent!.replaceAll('\n', '');
  assertEquals(expectedInfo, actualInfo);
}

function createSampleCluster(overrides: Partial<Cluster>): Cluster {
  return Object.assign(
      {
        id: BigInt(111),
        label: '',
        tabGroupName: 'My Tab Group Name',
        labelMatchPositions: [],
        visits: [],
        relatedSearches: [],
        imageUrl: undefined,
        fromPersistence: false,
        debugInfo: undefined,
      },
      overrides);
}

suite('NewTabPageModulesHistoryClustersModuleTest', () => {
  let handler: TestMock<PageHandlerRemote>;
  let imageServiceHandler: TestMock<PageImageServiceHandlerRemote>;
  let metrics: MetricsTracker;
  let newTabPageCallbackRouterRemote: PageRemote;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        PageHandlerRemote,
        mock => HistoryClustersProxyImpl.setInstance(
            new HistoryClustersProxyImpl(mock)));
    imageServiceHandler = installMock(
        PageImageServiceHandlerRemote,
        mock => PageImageServiceBrowserProxy.setInstance(
            new PageImageServiceBrowserProxy(mock)));
    metrics = fakeMetricsPrivate();
    newTabPageCallbackRouterRemote =
        NewTabPageProxy.getInstance()
            .callbackRouter.$.bindNewPipeAndPassRemote();
  });

  async function initializeModule(
      clusters: Cluster[], cart: Cart|null = null,
      discounts: Map<Url, Discount[]> =
          new Map<Url, Discount[]>()): Promise<HistoryClustersModuleElement> {
    handler.setResultFor('getClusters', Promise.resolve({clusters}));
    handler.setResultFor('getCartForCluster', Promise.resolve({cart}));
    handler.setResultFor(
        'getDiscountsForCluster', Promise.resolve({discounts}));

    const moduleElement = await historyClustersDescriptor.initialize(0) as
        HistoryClustersModuleElement;
    await handler.whenCalled('getClusters');
    document.body.append(moduleElement);
    await waitAfterNextRender(moduleElement);
    return moduleElement;
  }

  suite('Core', () => {
    test('No module created if no history cluster data', async () => {
      // Arrange.
      const moduleElement = await initializeModule([]);

      // Assert.
      assertEquals(null, moduleElement);
      assertEquals(1, metrics.count(DISPLAY_LAYOUT_METRIC_NAME));
      assertEquals(
          1, metrics.count(DISPLAY_LAYOUT_METRIC_NAME, LayoutType.kNone));
    });

    test('No module created when data does not match layouts', async () => {
      // Arrange.
      const cluster: Partial<Cluster> = {
        visits: createSampleVisits(2, 0),
        relatedSearches: createRelatedSearches(),
      };
      const moduleElement =
          await initializeModule([createSampleCluster(cluster)]);

      // Assert.
      assertEquals(null, moduleElement);
    });

    test('Header element populated with correct data', async () => {
      // Arrange.
      const sampleClusterLabel = '"Sample Journey"';
      const moduleElement = await initializeModule([createSampleCluster({
        label: sampleClusterLabel,
        visits: createLayoutSuitableSampleVisits(LayoutType.kLayout1),
        relatedSearches: createRelatedSearches(),
      })]);

      // Assert.
      assertTrue(!!moduleElement);
      const headerElement = $$(moduleElement, 'ntp-module-header');
      assertTrue(!!headerElement);

      assertEquals(
          'Show all',
          headerElement.querySelector('#showAllButton')!.innerHTML.trim());
      assertModuleHeaderTitle(
          headerElement, `Resume browsing for ${sampleClusterLabel}`);
    });

    test('Header info button click opens info dialog', async () => {
      // Arrange.
      const sampleClusterLabel = '"Sample Journey"';
      const moduleElement = await initializeModule([createSampleCluster({
        label: sampleClusterLabel,
        visits: createLayoutSuitableSampleVisits(LayoutType.kLayout1),
        relatedSearches: createRelatedSearches(),
      })]);

      // Act.
      assertTrue(!!moduleElement);
      const headerElement = $$(moduleElement, 'ntp-module-header');
      assertTrue(!!headerElement);

      headerElement!.dispatchEvent(new Event('info-button-click'));

      // Assert.
      assertTrue(!!$$(moduleElement, 'ntp-info-dialog'));
    });

    test('Backend is notified when Show all button is triggered', async () => {
      const sampleClusterUnquotedLabel = 'Sample Journey';
      const sampleClusterLabel = `"${sampleClusterUnquotedLabel}"`;
      const moduleElement = await initializeModule([createSampleCluster({
        label: sampleClusterLabel,
        visits: createLayoutSuitableSampleVisits(LayoutType.kLayout1),
        relatedSearches: createRelatedSearches(),
      })]);
      assertTrue(!!moduleElement);

      const headerElement = $$(moduleElement, 'ntp-module-header');
      assertTrue(!!headerElement);
      const showAllButton =
          headerElement.querySelector('#showAllButton') as HTMLElement;
      assertTrue(!!showAllButton);

      const waitForUsageEvent = eventToPromise('usage', moduleElement);
      showAllButton.click();

      const query = await handler.whenCalled('showJourneysSidePanel');
      assertEquals(sampleClusterUnquotedLabel, query);
      assertEquals(
          1,
          metrics.count(
              `NewTabPage.HistoryClusters.Layout${LayoutType.kLayout1}.Click`));
      assertEquals(
          1,
          metrics.count(
              `NewTabPage.HistoryClusters.Layout${LayoutType.kLayout1}.Click`,
              HistoryClusterElementType.SHOW_ALL));
      const clusterId = await handler.whenCalled('recordClick');
      assertEquals(BigInt(111), clusterId);
      await waitForUsageEvent;
    });

    test(
        'Backend is notified when "Open all in tab group" is triggered',
        async () => {
          const sampleCluster =
              createLayoutSuitableSampleCluster(LayoutType.kLayout1);
          const moduleElement = await initializeModule([sampleCluster]);
          assertTrue(!!moduleElement);

          const openAllButton =
              moduleElement.shadowRoot!.querySelector('ntp-module-header')!
                  .querySelector<HTMLElement>('#openAllInTabGroupButton')!;
          assertEquals(
              (loadTimeData.getString(
                  'modulesJourneysOpenAllInNewTabGroupButtonText')),
              openAllButton.innerText.trim());

          const waitForUsageEvent = eventToPromise('usage', moduleElement);
          openAllButton.click();

          const [urls, tabGroupName] =
              await handler.whenCalled('openUrlsInTabGroup');
          assertEquals(3, urls.length);
          assertEquals(`${GOOGLE_SEARCH_BASE_URL}?q=foo`, urls[0].url);
          assertEquals('https://www.foo.com/1', urls[1].url);
          assertEquals('https://www.foo.com/2', urls[2].url);
          assertEquals('My Tab Group Name', tabGroupName);
          await waitForUsageEvent;
        });

    test('Backend is notified when module is disabled', async () => {
      // Arrange.
      const moduleElement = await initializeModule(
          [createLayoutSuitableSampleCluster(LayoutType.kLayout1)]);
      assertTrue(!!moduleElement);

      // Act.
      const disableButton =
          moduleElement.shadowRoot!.querySelector('ntp-module-header')!
              .shadowRoot!.querySelector<HTMLElement>('#disableButton')!;
      disableButton.click();

      // Assert.
      const clusterId = await handler.whenCalled('recordDisabled');
      assertEquals(BigInt(111), clusterId);
    });

    test('Backend is notified when module is dismissed', async () => {
      // Arrange.
      const sampleClusterLabel = '"Sample Journey"';
      const sampleCluster = createSampleCluster({
        label: sampleClusterLabel,
        visits: createLayoutSuitableSampleVisits(LayoutType.kLayout1),
        relatedSearches: createRelatedSearches(),
      });
      const moduleElement = await initializeModule([sampleCluster]);
      assertTrue(!!moduleElement);

      // Act.
      const waitForDismissEvent =
          eventToPromise('dismiss-module', moduleElement);
      const dismissButton =
          moduleElement.shadowRoot!.querySelector('ntp-module-header')!
              .shadowRoot!.querySelector<HTMLElement>('#dismissButton')!;
      dismissButton.click();

      // Assert.
      const dismissEvent: DismissModuleEvent = await waitForDismissEvent;
      assertEquals(
          `${sampleCluster.label!} hidden`, dismissEvent.detail.message);
      const [visits, clusterId] = await handler.whenCalled('dismissCluster');
      assertEquals(3, visits.length);
      visits.forEach((visit: URLVisit, index: number) => {
        assertEquals(index, Number(visit.visitId));
      });
      assertEquals(BigInt(111), clusterId);
    });
  });

  suite('Layouts', () => {
    function removeHrefAndClick(element: HTMLElement) {
      element.removeAttribute('href');
      element.click();
    }

    [LayoutType.kLayout1, LayoutType.kLayout2, LayoutType.kLayout3].forEach(
        layoutType => {
          test('Scrollable content when overflowing', async () => {
            loadTimeData.overrideValues({
              modulesOverflowScrollbarEnabled: true,
            });

            const clusters = [createLayoutSuitableSampleCluster(layoutType)];
            handler.setResultFor('getClusters', Promise.resolve({clusters}));
            handler.setResultFor(
                'getCartForCluster', Promise.resolve({cart: null}));
            handler.setResultFor(
                'getDiscountsForCluster',
                Promise.resolve({discounts: new Map<Url, Discount[]>()}));
            const moduleElement = await historyClustersDescriptor.initialize(
                                      0) as HistoryClustersModuleElement;
            await handler.whenCalled('getClusters');

            const overflowWidth = 766;
            const containerWidth = 360;

            const containerElement = document.createElement('div');
            containerElement.style.maxWidth = `${containerWidth}px`;
            containerElement.appendChild(moduleElement);
            document.body.append(containerElement);
            await waitAfterNextRender(containerElement);

            assertEquals(containerWidth, containerElement.offsetWidth);
            assertEquals(
                overflowWidth,
                moduleElement.shadowRoot!.querySelector(
                                             '.layout')!.scrollWidth);
          });

          test(`Layout ${layoutType}: Visit tile click metrics`, async () => {
            // Arrange.
            const moduleElement = await initializeModule(
                [createLayoutSuitableSampleCluster(layoutType)]);

            // Assert.
            assertTrue(!!moduleElement);
            const tileElement =
                $$(moduleElement, 'ntp-history-clusters-tile') as HTMLElement;
            assertTrue(!!tileElement);

            const waitForUsageEvent = eventToPromise('usage', moduleElement);
            removeHrefAndClick($$(tileElement, '#content') as HTMLElement);
            assertEquals(
                1,
                metrics.count(`NewTabPage.HistoryClusters.Layout${
                    layoutType}.VisitTile.ClickIndex`));
            assertEquals(
                1,
                metrics.count(
                    `NewTabPage.HistoryClusters.Layout${layoutType}.Click`));
            assertEquals(
                1,
                metrics.count(
                    `NewTabPage.HistoryClusters.Layout${layoutType}.Click`,
                    HistoryClusterElementType.VISIT));
            const clusterId = await handler.whenCalled('recordClick');
            assertEquals(BigInt(111), clusterId);
            await waitForUsageEvent;
          });

          test(`Layout ${layoutType}: Suggest tile click metrics`, async () => {
            // Arrange.
            const moduleElement = await initializeModule(
                [createLayoutSuitableSampleCluster(layoutType)]);

            // Assert.
            assertTrue(!!moduleElement);
            const suggestTileElement =
                $$(moduleElement, 'ntp-history-clusters-suggest-tile');
            assertTrue(!!suggestTileElement);

            const waitForUsageEvent = eventToPromise('usage', moduleElement);
            removeHrefAndClick(
                $$(suggestTileElement, '.related-search') as HTMLElement);
            assertEquals(
                1,
                metrics.count(`NewTabPage.HistoryClusters.Layout${
                    layoutType}.SuggestTile.ClickIndex`));
            assertEquals(
                1,
                metrics.count(
                    `NewTabPage.HistoryClusters.Layout${layoutType}.Click`));
            assertEquals(
                1,
                metrics.count(
                    `NewTabPage.HistoryClusters.Layout${layoutType}.Click`,
                    HistoryClusterElementType.SUGGEST));
            const clusterId = await handler.whenCalled('recordClick');
            assertEquals(BigInt(111), clusterId);
            await waitForUsageEvent;
          });

          const LAYOUT_MIN_VISITS =
              [LAYOUT_1_MIN_VISITS, LAYOUT_2_MIN_VISITS, LAYOUT_3_MIN_VISITS];
          const LAYOUT_MIN_IMAGE_VISITS = [
            LAYOUT_1_MIN_IMAGE_VISITS,
            LAYOUT_2_MIN_IMAGE_VISITS,
            LAYOUT_3_MIN_IMAGE_VISITS,
          ];

          test(`Layout ${layoutType} is used`, async () => {
            // Arrange.
            const moduleElement = await initializeModule(
                [createLayoutSuitableSampleCluster(layoutType)]);

            // Assert.
            assertTrue(!!moduleElement);
            assertLayoutSet(moduleElement, layoutType);
            // Check that metrics are set.
            assertEquals(1, metrics.count(DISPLAY_LAYOUT_METRIC_NAME));
            assertEquals(
                1, metrics.count(DISPLAY_LAYOUT_METRIC_NAME, layoutType));

            const [recordedLayoutType, clusterId] =
                await handler.whenCalled('recordLayoutTypeShown');
            assertEquals(layoutType, recordedLayoutType);
            assertEquals(BigInt(111), clusterId);
            // Check that the visits are processed and set properly.
            const visits = moduleElement.cluster.visits;
            assertEquals(visits.length, LAYOUT_MIN_VISITS[layoutType - 1]);
            for (let i = 0; i < visits.length; i++) {
              assertTrue(!!visits[i]);
              if (i < LAYOUT_MIN_IMAGE_VISITS[layoutType - 1]!) {
                assertTrue(visits[i]!.hasUrlKeyedImage);
              }
            }
            // TODO(romanarora): Assert the visit elements have a non zero title
            // length.
          });
        });

    test(
        'Layout renders correctly for no leading image sample visits',
        async () => {
          const expectedLayout = LayoutType.kLayout1;
          const moduleElement = await initializeModule([createSampleCluster({
            visits: createSampleVisits(
                LAYOUT_3_MIN_VISITS - 1, LAYOUT_1_MIN_IMAGE_VISITS, false),
          })]);
          assertTrue(!!moduleElement);
          assertLayoutSet(moduleElement, expectedLayout);

          const visitElements = moduleElement.shadowRoot!.querySelectorAll(
              'ntp-history-clusters-tile')!;
          assertEquals(LAYOUT_1_MIN_VISITS, visitElements.length);
          visitElements.forEach(visitElement => {
            const title = $$(visitElement, '#title')!.textContent!.trim();
            assertTrue(!!title);
          });
        });
  });

  suite('PagehideMetricNoImages', () => {
    test('Module records no images state metric on pagehide', async () => {
      imageServiceHandler.setResultFor(
          'getPageImageUrl', Promise.resolve(null));

      const moduleElement = await initializeModule(
          [createLayoutSuitableSampleCluster(LayoutType.kLayout1)]);
      assertTrue(!!moduleElement);
      await waitAfterNextRender(moduleElement);

      window.dispatchEvent(new Event('pagehide'));

      assertEquals(2, imageServiceHandler.getCallCount('getPageImageUrl'));
      assertEquals(
          1,
          metrics.count(`NewTabPage.HistoryClusters.Layout${
              LayoutType.kLayout1}.ImageDisplayState`));
      assertEquals(
          1,
          metrics.count(
              `NewTabPage.HistoryClusters.Layout${
                  LayoutType.kLayout1}.ImageDisplayState`,
              HistoryClusterImageDisplayState.NONE));
    });
  });

  suite('PagehideMetricAllImages', () => {
    test('Module records all images state metric on pagehide', async () => {
      imageServiceHandler.setResultFor('getPageImageUrl', Promise.resolve({
        result: {imageUrl: {url: 'https://example.com/image.png'}},
      }));

      const moduleElement = await initializeModule(
          [createLayoutSuitableSampleCluster(LayoutType.kLayout1)]);
      assertTrue(!!moduleElement);
      await waitAfterNextRender(moduleElement);

      window.dispatchEvent(new Event('pagehide'));

      assertEquals(2, imageServiceHandler.getCallCount('getPageImageUrl'));
      assertEquals(
          1,
          metrics.count(`NewTabPage.HistoryClusters.Layout${
              LayoutType.kLayout1}.ImageDisplayState`));
      assertEquals(
          1,
          metrics.count(
              `NewTabPage.HistoryClusters.Layout${
                  LayoutType.kLayout1}.ImageDisplayState`,
              HistoryClusterImageDisplayState.ALL));
    });
  });

  suite('CartTileRendering', () => {
    test('Cart tile is not rendererd when feature is disabled', async () => {
      loadTimeData.overrideValues({
        modulesChromeCartInHistoryClustersModuleEnabled: false,
      });

      const moduleElement = await initializeModule(
          [createLayoutSuitableSampleCluster(LayoutType.kLayout1)], null);

      assertEquals(0, handler.getCallCount('getCartForCluster'));
      assertTrue(!!moduleElement);
      await waitAfterNextRender(moduleElement);
      const cartTile = moduleElement.shadowRoot!.getElementById('cartTile');
      assertFalse(!!cartTile);
      assertFalse(!!moduleElement.cart);
    });

    test(
        'Cart tile is not rendererd when feature is enabled but no cart',
        async () => {
          loadTimeData.overrideValues({
            modulesChromeCartInHistoryClustersModuleEnabled: true,
          });

          const moduleElement = await initializeModule(
              [createLayoutSuitableSampleCluster(LayoutType.kLayout1)], null);

          assertEquals(1, handler.getCallCount('getCartForCluster'));
          assertTrue(!!moduleElement);
          await waitAfterNextRender(moduleElement);
          const cartTile = moduleElement.shadowRoot!.getElementById('cartTile');
          assertFalse(!!cartTile);
          assertFalse(!!moduleElement.cart);
        });

    test('Cart tile is correctly rendered', async () => {
      loadTimeData.overrideValues({
        modulesChromeCartInHistoryClustersModuleEnabled: true,
      });

      const cart: Cart = Object.assign({
        domain: 'foo.com',
        merchant: 'Foo',
        cartUrl: {url: 'https://foo.com'},
        productImageUrls: [],
        discountText: '',
        relativeDate: '6 mins ago',
      });
      const moduleElement = await initializeModule(
          [createLayoutSuitableSampleCluster(LayoutType.kLayout1)], cart);

      assertEquals(1, handler.getCallCount('getCartForCluster'));
      assertTrue(!!moduleElement);
      await waitAfterNextRender(moduleElement);
      const cartTile = moduleElement.shadowRoot!.getElementById('cartTile');
      assertTrue(!!cartTile);
      assertTrue(!!moduleElement.cart);
    });

    test('Cart tile controlled by settings', async () => {
      loadTimeData.overrideValues({
        modulesChromeCartInHistoryClustersModuleEnabled: true,
      });

      // Arrange.
      const cart: Cart = Object.assign({
        domain: 'foo.com',
        merchant: 'Foo',
        cartUrl: {url: 'https://foo.com'},
        productImageUrls: [],
        discountText: '',
        relativeDate: '6 mins ago',
      });
      const moduleElement = await initializeModule(
          [createLayoutSuitableSampleCluster(LayoutType.kLayout1)], cart);

      assertEquals(1, handler.getCallCount('getCartForCluster'));
      assertTrue(!!moduleElement);
      await waitAfterNextRender(moduleElement);
      let cartTile = moduleElement.shadowRoot!.getElementById('cartTile');
      let questTiles = moduleElement.shadowRoot!.querySelectorAll(
          'ntp-history-clusters-tile');
      assertTrue(!!cartTile);
      assertTrue(!!moduleElement.cart);
      assertEquals(1, questTiles.length);

      // Act.
      newTabPageCallbackRouterRemote.setDisabledModules(false, ['chrome_cart']);
      await waitAfterNextRender(moduleElement);

      // Assert.
      cartTile = moduleElement.shadowRoot!.getElementById('cartTile');
      questTiles = moduleElement.shadowRoot!.querySelectorAll(
          'ntp-history-clusters-tile');
      assertTrue(!cartTile);
      assertTrue(!moduleElement.cart);
      assertEquals(2, questTiles.length);

      // Act.
      newTabPageCallbackRouterRemote.setDisabledModules(false, []);
      await waitAfterNextRender(moduleElement);

      // Assert.
      cartTile = moduleElement.shadowRoot!.getElementById('cartTile');
      questTiles = moduleElement.shadowRoot!.querySelectorAll(
          'ntp-history-clusters-tile');
      assertTrue(!!cartTile);
      assertTrue(!!moduleElement.cart);
      assertEquals(1, questTiles.length);
    });

    test('Cart tile clicking metrics are collected', async () => {
      loadTimeData.overrideValues({
        modulesChromeCartInHistoryClustersModuleEnabled: true,
      });

      const cart: Cart = Object.assign({
        domain: 'foo.com',
        merchant: 'Foo',
        cartUrl: {url: 'https://foo.com'},
        productImageUrls: [],
        discountText: '',
        relativeDate: '6 mins ago',
      });
      const moduleElement = await initializeModule(
          [createLayoutSuitableSampleCluster(LayoutType.kLayout1)], cart);

      assertEquals(1, handler.getCallCount('getCartForCluster'));
      assertTrue(!!moduleElement);
      await waitAfterNextRender(moduleElement);
      const cartTile = moduleElement.shadowRoot!.getElementById('cartTile');
      assertTrue(!!cartTile);
      assertTrue(!!moduleElement.cart);

      // Act.
      const waitForUsageEvent = eventToPromise('usage', moduleElement);
      cartTile.click();

      // Assert.
      assertEquals(
          1,
          metrics.count(
              `NewTabPage.HistoryClusters.Layout1.Click`,
              HistoryClusterElementType.CART));
      const clusterId = await handler.whenCalled('recordClick');
      assertEquals(BigInt(111), clusterId);
      await waitForUsageEvent;
    });
  });

  suite('DiscountChipRendering', () => {
    test('Discount is not initialized when feature is disabled', async () => {
      loadTimeData.overrideValues({
        historyClustersModuleDiscountsEnabled: false,
      });

      const moduleElement = await initializeModule(
          [createLayoutSuitableSampleCluster(LayoutType.kLayout1)]);

      assertEquals(0, handler.getCallCount('getDiscountsForCluster'));
      assertTrue(!!moduleElement);
      await waitAfterNextRender(moduleElement);
      assertEquals(moduleElement.discounts.length, LAYOUT_1_MIN_VISITS);
      for (const discount of moduleElement.discounts) {
        assertEquals('', discount);
      }
      const contentElement =
          moduleElement.shadowRoot!.querySelector('ntp-history-clusters-tile')!
              .shadowRoot!.querySelector('#content')! as HTMLElement;
      assertEquals(
          contentElement.getAttribute('aria-label'),
          'Test Title 1, foo.com, 1 min ago');
      checkInfoDialogContent(moduleElement, 'modulesJourneysInfo');
      assertEquals(0, metrics.count(`NewTabPage.HistoryClusters.HasDiscount`));
    });

    test('Discount initialization', async () => {
      loadTimeData.overrideValues({
        historyClustersModuleDiscountsEnabled: true,
      });

      const cluster = createLayoutSuitableSampleCluster(LayoutType.kLayout3);
      // Ignore SRP visit when compare length.
      assertEquals(LAYOUT_3_MIN_VISITS, cluster.visits.length - 1);
      const discoutMap = new Map<Url, Discount[]>();
      discoutMap.set(cluster.visits[2]!.normalizedUrl, [{
                       valueInText: '15% off',
                       annotatedVisitUrl: {url: 'https://www.annotated.com/1'},
                     }]);
      discoutMap.set(cluster.visits[3]!.normalizedUrl, [{
                       valueInText: '$10 off',
                       annotatedVisitUrl: {url: 'https://www.annotated.com/2'},
                     }]);

      const moduleElement = await initializeModule([cluster], null, discoutMap);

      assertEquals(1, handler.getCallCount('getDiscountsForCluster'));
      assertTrue(!!moduleElement);
      await waitAfterNextRender(moduleElement);
      assertEquals(moduleElement.discounts.length, LAYOUT_3_MIN_VISITS);
      assertEquals(1, metrics.count(`NewTabPage.HistoryClusters.HasDiscount`));

      const expectedDiscounts = ['', '15% off', '$10 off', ''];
      const visitTiles: TileModuleElement[] =
          Array.from(moduleElement.shadowRoot!.querySelectorAll(
              'ntp-history-clusters-tile'));
      for (let i = 0; i < moduleElement.discounts.length; i++) {
        assertEquals(expectedDiscounts[i], moduleElement.discounts[i]);
        assertEquals(expectedDiscounts[i], visitTiles[i]!.discount);
      }
      assertEquals(
          'https://www.foo.com/1', visitTiles[0]!.visit.normalizedUrl.url);
      assertEquals('', visitTiles[0]!.discount);
      let contentElement =
          visitTiles[0]!.shadowRoot!.querySelector('#content')! as HTMLElement;
      assertEquals(
          contentElement.getAttribute('aria-label'),
          'Test Title 1, foo.com, 1 min ago');

      assertEquals(
          'https://www.annotated.com/1',
          visitTiles[1]!.visit.normalizedUrl.url);
      assertEquals('15% off', visitTiles[1]!.discount);
      contentElement =
          visitTiles[1]!.shadowRoot!.querySelector('#content')! as HTMLElement;
      assertEquals(
          contentElement.getAttribute('aria-label'),
          'Test Title 2, annotated.com, 1 min ago, 15% off');

      assertEquals(
          'https://www.annotated.com/2',
          visitTiles[2]!.visit.normalizedUrl.url);
      assertEquals('$10 off', visitTiles[2]!.discount);
      contentElement =
          visitTiles[2]!.shadowRoot!.querySelector('#content')! as HTMLElement;
      assertEquals(
          contentElement.getAttribute('aria-label'),
          'Test Title 3, annotated.com, 1 min ago, $10 off');

      checkInfoDialogContent(moduleElement, 'modulesHistoryWithDiscountInfo');
    });

    test('Metrics for Discount click', async () => {
      loadTimeData.overrideValues({
        historyClustersModuleDiscountsEnabled: true,
      });

      const cluster = createLayoutSuitableSampleCluster(LayoutType.kLayout3);
      // Ignore SRP visit when compare length.
      assertEquals(LAYOUT_3_MIN_VISITS, cluster.visits.length - 1);
      const discoutMap = new Map<Url, Discount[]>();
      discoutMap.set(cluster.visits[2]!.normalizedUrl, [{
                       valueInText: '15% off',
                       annotatedVisitUrl: {url: 'https://www.annotated.com/1'},
                     }]);

      const moduleElement = await initializeModule([cluster], null, discoutMap);

      assertEquals(1, handler.getCallCount('getDiscountsForCluster'));
      assertTrue(!!moduleElement);
      await waitAfterNextRender(moduleElement);
      assertEquals(moduleElement.discounts.length, LAYOUT_3_MIN_VISITS);
      assertEquals(1, metrics.count(`NewTabPage.HistoryClusters.HasDiscount`));

      const visitTiles: TileModuleElement[] =
          Array.from(moduleElement.shadowRoot!.querySelectorAll(
              'ntp-history-clusters-tile'));

      visitTiles[0]!.click();
      assertEquals(
          0, metrics.count(`NewTabPage.HistoryClusters.DiscountClicked`));

      visitTiles[1]!.click();
      assertEquals(
          1, metrics.count(`NewTabPage.HistoryClusters.DiscountClicked`));
    });
  });
});
