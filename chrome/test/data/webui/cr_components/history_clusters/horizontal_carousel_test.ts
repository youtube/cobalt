// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/history_clusters/horizontal_carousel.js';

import {HorizontalCarouselElement} from 'chrome://resources/cr_components/history_clusters/horizontal_carousel.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

let carouselElement : HorizontalCarouselElement;

suite('HorizontalCarouselTest', () => {
  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    carouselElement = document.createElement('horizontal-carousel');
    document.body.appendChild(carouselElement);
  });

  test('CarouselResizeUpdatesButtons', async () => {
    document.documentElement.setAttribute('chrome-refresh-2023', 'true');

    const carousel = carouselElement!.$.carouselContainer;
    assertTrue(!!carousel);
    carousel.style.width = '600px';

    const forwardButton = carouselElement!.$.carouselForwardButton;
    const backButton = carouselElement!.$.carouselBackButton;

    // Assert forward/back button does not show initially.
    assertTrue(forwardButton.hidden);
    assertTrue(backButton.hidden);

    const smallDiv = document.createElement('div');
    smallDiv.style.width = '80px';
    smallDiv.style.height = '50px';
    smallDiv.style.flexShrink = '0';
    carousel.querySelector('slot')!.appendChild(smallDiv);

    await new Promise<void>((resolve) => {
      const observer = new ResizeObserver(() => {
        /* Includes 2px padding on either side */
        if (carousel.offsetWidth === 64) {
          resolve();
          observer.unobserve(carousel);
        }
      });
      observer.observe(carousel);
      carousel.style.width = '60px';
    });

    // Assert forward buttons shows when carousel is resized with larger
    // element.
    assertFalse(forwardButton.hidden);
    assertTrue(backButton.hidden);

    // On forward button click the back button should show up and the
    // forward button should hide.
    forwardButton.click();
    setTimeout(() => {
      assertFalse(backButton.hidden);
      assertTrue(forwardButton.hidden);
    }, 1000);

  });
});
