// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens-overlay/post_selection_renderer.js';

import type {BitmapMappedFromTrustedProcess} from '//resources/mojo/skia/public/mojom/bitmap.mojom-webui.js';
import type {PointF, RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import type {PostSelectionRendererElement} from 'chrome-untrusted://lens-overlay/post_selection_renderer.js';
import {RegionSource, SelectionOverlayBaseHandler} from 'chrome-untrusted://lens-overlay/selection_overlay_base_handler.js';
import type {SelectedRegion} from 'chrome-untrusted://lens-overlay/selection_overlay_base_handler.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';

const TEST_WIDTH = 800;
const TEST_HEIGHT = 500;

class FakeSelectionOverlayBaseHandler extends SelectionOverlayBaseHandler {
  multiRegionCallbacks: Array<(regions: SelectedRegion[]) => void> = [];
  clearAllCallbacks: Array<() => void> = [];
  clearRegionCallbacks: Array<() => void> = [];
  setPostRegionCallbacks: Array<(region: RectF) => void> = [];

  deletedRegions: Array<{id: string, source: RegionSource}> = [];
  adjustedRegions: Array<{rect: RectF, source: RegionSource, id?: string}> = [];

  addMultiRegionSelectionListener(
      callback: (regions: SelectedRegion[]) => void): number {
    this.multiRegionCallbacks.push(callback);
    return this.multiRegionCallbacks.length - 1;
  }

  removeListener(_id: number): boolean {
    return true;
  }

  addBackgroundBlur(): void {}
  addOnOverlayReshownListener(
      _callback: (screenshotData: BitmapMappedFromTrustedProcess) => void):
      number {
    return -1;
  }
  addNotifyOverlayClosingListener(_callback: () => void): number {
    return -1;
  }
  addScreenshotDataReceivedListener(
      _callback:
          (screenshotData: BitmapMappedFromTrustedProcess,
           isSidePanelOpen: boolean) => void,
      ): number {
    return -1;
  }
  addClearRegionSelectionListener(callback: () => void): number {
    this.clearRegionCallbacks.push(callback);
    return this.clearRegionCallbacks.length - 1;
  }
  addClearAllSelectionsListener(callback: () => void): number {
    this.clearAllCallbacks.push(callback);
    return this.clearAllCallbacks.length - 1;
  }
  addNotifyResultsPanelOpenedListener(_callback: () => void): number {
    return -1;
  }
  addSetPostRegionSelectionListener(callback: (region: RectF) => void): number {
    this.setPostRegionCallbacks.push(callback);
    return this.setPostRegionCallbacks.length - 1;
  }
  adjustRegionSelected(rect: RectF, source: RegionSource, id?: string): void {
    this.adjustedRegions.push({rect, source, id});
  }
  adjustPolylineSelected(
      _points: PointF[], _source: RegionSource, _id?: string): void {}
  deleteRegion(id: string, source: RegionSource): void {
    this.deletedRegions.push({id, source});
  }
  closePreselectionBubble(): void {}
  notifyOverlayInitialized(): void {}
  setLiveBlur(_enabled: boolean): void {}

  triggerMultiRegionSelectionUpdated(regions: SelectedRegion[]) {
    for (const cb of this.multiRegionCallbacks) {
      cb(regions);
    }
  }
}

suite('MultiRegionSelection', () => {
  let postSelectionRenderer: PostSelectionRendererElement;
  let fakeSelectionHandler: FakeSelectionOverlayBaseHandler;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      enableMultiRegionSelection: true,
    });

    fakeSelectionHandler = new FakeSelectionOverlayBaseHandler();
    SelectionOverlayBaseHandler.setInstance(fakeSelectionHandler);

    postSelectionRenderer = document.createElement('post-selection-renderer');
    postSelectionRenderer.setSelectionOverlayRectForTesting(
        new DOMRect(0, 0, TEST_WIDTH, TEST_HEIGHT));

    postSelectionRenderer.style.display = 'block';
    postSelectionRenderer.style.width = `${TEST_WIDTH}px`;
    postSelectionRenderer.style.height = `${TEST_HEIGHT}px`;

    document.body.appendChild(postSelectionRenderer);
    await waitAfterNextRender(postSelectionRenderer);
  });

  test('MultiRegionDisabled_NoStaticRegions', async () => {
    // Re-create the renderer with the feature flag disabled.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      enableMultiRegionSelection: false,
    });

    postSelectionRenderer = document.createElement('post-selection-renderer');
    postSelectionRenderer.setSelectionOverlayRectForTesting(
        new DOMRect(0, 0, TEST_WIDTH, TEST_HEIGHT));
    postSelectionRenderer.style.display = 'block';
    postSelectionRenderer.style.width = `${TEST_WIDTH}px`;
    postSelectionRenderer.style.height = `${TEST_HEIGHT}px`;
    document.body.appendChild(postSelectionRenderer);
    await waitAfterNextRender(postSelectionRenderer);

    const regions: SelectedRegion[] = [
      {
        id: 'region-a',
        region: {x: 0.2, y: 0.3, width: 0.1, height: 0.1},
      },
      {
        id: 'region-b',
        region: {x: 0.6, y: 0.6, width: 0.2, height: 0.2},
      },
    ];

    fakeSelectionHandler.triggerMultiRegionSelectionUpdated(regions);
    await waitAfterNextRender(postSelectionRenderer);

    // Verify staticRegions on the renderer is empty.
    const staticRegions =
        postSelectionRenderer.shadowRoot!.querySelectorAll('.static-region');
    assertEquals(0, staticRegions.length);
  });

  test('MultiRegionEnabled_RendersStaticRegions', async () => {
    const regions: SelectedRegion[] = [
      {
        id: 'region-a',
        region: {x: 0.2, y: 0.3, width: 0.1, height: 0.1},
      },
      {
        id: 'region-b',
        region: {x: 0.6, y: 0.6, width: 0.2, height: 0.2},
      },
    ];

    // Trigger update.
    fakeSelectionHandler.triggerMultiRegionSelectionUpdated(regions);
    postSelectionRenderer.setActiveRegionIdForTesting('region-a');
    await waitAfterNextRender(postSelectionRenderer);

    // activeRegionId should be region-a.
    // Static regions should contain region-b.
    const staticRegionElements =
        postSelectionRenderer.shadowRoot!.querySelectorAll('.static-region');
    assertEquals(1, staticRegionElements.length);
    assertEquals(
        'region-b', (staticRegionElements[0] as HTMLElement).dataset['id']);

    // Check that we can change active region to region-b, and region-a becomes
    // static.
    postSelectionRenderer.setActiveRegionIdForTesting('region-b');
    await waitAfterNextRender(postSelectionRenderer);

    const newStaticRegionElements =
        postSelectionRenderer.shadowRoot!.querySelectorAll('.static-region');
    assertEquals(1, newStaticRegionElements.length);
    assertEquals(
        'region-a', (newStaticRegionElements[0] as HTMLElement).dataset['id']);
  });

  test('PointerMove_TriggersActivateRegion', async () => {
    const regions: SelectedRegion[] = [
      {
        id: 'region-a',
        region: {x: 0.2, y: 0.3, width: 0.1, height: 0.1},
      },
      {
        id: 'region-b',
        region: {x: 0.6, y: 0.6, width: 0.2, height: 0.2},
      },
    ];

    fakeSelectionHandler.triggerMultiRegionSelectionUpdated(regions);
    postSelectionRenderer.setActiveRegionIdForTesting('region-a');
    await waitAfterNextRender(postSelectionRenderer);

    const staticRegionElement =
        postSelectionRenderer.shadowRoot!.querySelector<HTMLElement>(
            '.static-region');
    assertTrue(!!staticRegionElement);

    // Spy on the elementsFromPoint method to return our static region element.
    postSelectionRenderer.shadowRoot!.elementsFromPoint =
        (_x: number, _y: number) => {
          return [staticRegionElement];
        };

    let activatedRegionId = '';
    postSelectionRenderer.addEventListener('activate-region', (e: Event) => {
      const customEvent = e as CustomEvent;
      activatedRegionId = customEvent.detail.id;
    });

    // Dispatch pointermove to trigger handlePointerMoveForFocus
    postSelectionRenderer.dispatchEvent(new PointerEvent('pointermove', {
      clientX: 100,
      clientY: 100,
      bubbles: true,
      composed: true,
    }));

    assertEquals('region-b', activatedRegionId);
  });

  test('CloseButton_DeletesActiveRegion', async () => {
    const regions: SelectedRegion[] = [
      {
        id: 'region-a',
        region: {x: 0.2, y: 0.3, width: 0.1, height: 0.1},
      },
    ];

    fakeSelectionHandler.triggerMultiRegionSelectionUpdated(regions);
    postSelectionRenderer.setActiveRegionIdForTesting('region-a');
    await waitAfterNextRender(postSelectionRenderer);

    // There should be a close button in the active region corners.
    const closeButton =
        postSelectionRenderer.shadowRoot!.querySelector<HTMLElement>(
            '#selectionCorners .close-button');
    assertTrue(!!closeButton, 'Close button should exist');

    // Click close button
    closeButton.click();

    // Verify deleteRegion was called on the handler
    assertEquals(1, fakeSelectionHandler.deletedRegions.length);
    const deletedRegion = fakeSelectionHandler.deletedRegions[0]!;
    assertEquals('region-a', deletedRegion.id);
    assertEquals(RegionSource.KEYBOARD, deletedRegion.source);
  });
});
