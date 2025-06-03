// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals} from 'chrome://webui-test/chai_assert.js';

import {getRequiredElement} from 'chrome://resources/js/util_ts.js';

declare global {
  interface Window {
    // Exported by the prod code.
    whenPageIsPopulatedForTest(): Promise<void>;
  }
}

suite('Stats', function() {
  suiteSetup(function() {
    return window.whenPageIsPopulatedForTest();
  });

  test('check table is loaded', () => {
    const statsRows =
        Array.from(getRequiredElement('stats-table-body').children);

    assertDeepEquals(
        [
          ['mediaImage', '0'],
          ['meta', '3'],
          ['origin', '0'],
          ['playback', '0'],
          ['playbackSession', '0'],
          ['sessionImage', '0'],
        ],
        statsRows.map(
            x => [x.children[0]!.textContent, x.children[1]!.textContent]));
  });
});

suite('Origins', function() {
  suiteSetup(function() {
    return window.whenPageIsPopulatedForTest();
  });

  test('check table is loaded', () => {
    const dataHeaderRows = Array.from(
        document.body.querySelector('#origins-table thead tr')!.children);

    assertDeepEquals(
        [
          'Origin',
          'Last Updated',
          'Audio + Video Watchtime (secs, cached)',
          'Audio + Video Watchtime (secs, actual)',
        ],
        dataHeaderRows.map(x => x.textContent!.trim()));
  });
});

suite('Playbacks', function() {
  suiteSetup(function() {
    return window.whenPageIsPopulatedForTest();
  });

  test('check table is loaded', () => {
    const dataHeaderRows = Array.from(
        document.body.querySelector('#playbacks-table thead tr')!.children);

    assertDeepEquals(
        ['URL', 'Last Updated', 'Has Audio', 'Has Video', 'Watchtime (secs)'],
        dataHeaderRows.map(x => x.textContent!.trim()));
  });
});

suite('Sessions', function() {
  suiteSetup(function() {
    return window.whenPageIsPopulatedForTest();
  });

  test('check table is loaded', () => {
    const dataHeaderRows = Array.from(
        document.body.querySelector('#sessions-table thead tr')!.children);

    assertDeepEquals(
        [
          'URL',
          'Last Updated',
          'Position (secs)',
          'Duration (secs)',
          'Title',
          'Artist',
          'Album',
          'Source Title',
          'Artwork',
        ],
        dataHeaderRows.map(x => x.textContent!.trim()));
  });
});
