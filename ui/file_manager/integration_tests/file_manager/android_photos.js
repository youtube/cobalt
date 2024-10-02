// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addEntries, ENTRIES, RootPath} from '../test_util.js';
import {testcase} from '../testcase.js';

import {openNewWindow, remoteCall} from './background.js';

testcase.androidPhotosBanner = async () => {
  // Add test files.
  // Photos provider currently does not have subdirectories, but we need one
  // there to tell that it's mounted and clickable (has-children="true"
  // selector).
  await addEntries(
      ['photos_documents_provider'], [ENTRIES.photos, ENTRIES.image2]);
  await addEntries(['local'], [ENTRIES.hello]);

  // Open Files app.
  const appId = await openNewWindow(RootPath.DOWNLOADS);

  const click = async (query) => {
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('fakeMouseClick', appId, [query]),
        'fakeMouseClick failed');
  };
  const waitForElement = async (query) => {
    await remoteCall.waitForElement(appId, query);
  };
  const waitForElementLost = async (query) => {
    await remoteCall.waitForElementLost(appId, query);
  };
  const waitForFile = async (name) => {
    await remoteCall.waitForElement(appId, `#file-list [file-name="${name}"]`);
  };

  await remoteCall.isolateBannerForTesting(appId, 'photos-welcome-banner');
  const photosBannerHiddenQuery = '#banners > photos-welcome-banner[hidden]';
  const photosBannerShownQuery =
      '#banners > photos-welcome-banner:not([hidden])';
  const photosBannerTextQuery = [
    '#banners > photos-welcome-banner',
    'educational-banner',
    '#educational-text-group',
  ];
  const photosBannerDismissButton = [
    '#banners > photos-welcome-banner',
    'educational-banner',
    '#dismiss-button',
  ];

  // Initial state: In the new framework banner is lazily loaded so will not be
  // attached to the DOM, without the banners framework the root element should
  // exist but the text should not be attached yet.
  await waitForElementLost(photosBannerTextQuery);

  // Wait for the DocumentsProvider volume to mount and navigate to Photos.
  const photosVolumeQuery =
      '[has-children="true"] [volume-type-icon="documents_provider"]';
  await waitForElement(photosVolumeQuery);
  await click(photosVolumeQuery);
  // Banner should be created and made visible.
  await waitForElement(photosBannerShownQuery);
  await waitForElement(photosBannerTextQuery);

  // Banner should disappear when navigating away (child elements are still in
  // DOM).
  await click('[volume-type-icon="downloads"]');
  await waitForElement(photosBannerHiddenQuery);
  await waitForElement(photosBannerTextQuery);

  // Banner should re-appear when navigating to Photos again.
  await click(photosVolumeQuery);
  await waitForElement(photosBannerShownQuery);

  // Dismiss the banner (created banner still in DOM).
  await waitForElement(photosBannerDismissButton);
  await click(photosBannerDismissButton);
  await waitForElement(photosBannerHiddenQuery);

  // Navigate away and then back, it should not re-appear.
  await click('[volume-type-icon="downloads"]');
  await waitForFile('hello.txt');
  await click(photosVolumeQuery);
  await waitForFile('image2.png');
  await waitForElement(photosBannerHiddenQuery);
};
