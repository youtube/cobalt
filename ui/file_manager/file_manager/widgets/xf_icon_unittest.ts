// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {waitForElementUpdate} from '../common/js/unittest_util.js';
import {constants} from '../foreground/js/constants.js';

import {XfIcon} from './xf_icon.js';

export function setUp() {
  document.body.innerHTML = '<xf-icon></xf-icon>';
}

async function getIcon(): Promise<XfIcon> {
  const element = document.querySelector('xf-icon');
  assertNotEquals(null, element);
  await waitForElementUpdate(element!);
  return element!;
}

function getSpanFromIcon(icon: XfIcon): HTMLSpanElement {
  return icon.shadowRoot!.querySelector<HTMLSpanElement>('span')!;
}

export async function testIconType(done: () => void) {
  const icon = await getIcon();
  const span = getSpanFromIcon(icon);

  // Check for all office icons, there should be a keep-color class.
  icon.type = constants.ICON_TYPES.WORD;
  await waitForElementUpdate(icon);
  assertTrue(span.classList.contains('keep-color'));

  icon.type = constants.ICON_TYPES.EXCEL;
  await waitForElementUpdate(icon);
  assertTrue(span.classList.contains('keep-color'));

  icon.type = constants.ICON_TYPES.POWERPOINT;
  await waitForElementUpdate(icon);
  assertTrue(span.classList.contains('keep-color'));

  // Check no keep-color class for other icon types.
  icon.type = constants.ICON_TYPES.ANDROID_FILES;
  await waitForElementUpdate(icon);
  assertFalse(span.classList.contains('keep-color'));

  done();
}

export async function testIconSize(done: () => void) {
  const icon = await getIcon();
  const span = getSpanFromIcon(icon);

  // By default the size should be small.
  assertEquals(XfIcon.sizes.SMALL, icon.size);
  assertEquals('20px', window.getComputedStyle(span).width);
  assertEquals('20px', window.getComputedStyle(span).height);

  // Check large size should change the width/height.
  icon.size = XfIcon.sizes.LARGE;
  await waitForElementUpdate(icon);
  assertEquals('48px', window.getComputedStyle(span).width);
  assertEquals('48px', window.getComputedStyle(span).height);

  done();
}

export async function testIconSetWithLowDPI(done: () => void) {
  const icon = await getIcon();
  icon.iconSet = {
    icon16x16Url: 'fake-base64-data',
    icon32x32Url: undefined,
  };
  await waitForElementUpdate(icon);

  const span = getSpanFromIcon(icon);
  assertTrue(span.classList.contains('keep-color'));
  assertTrue(
      window.getComputedStyle(span).backgroundImage.includes('image-set'));
  assertTrue(window.getComputedStyle(span).backgroundImage.includes('1dppx'));
  assertFalse(window.getComputedStyle(span).backgroundImage.includes('2dppx'));

  done();
}


export async function testIconSetWithHighDPI(done: () => void) {
  const icon = await getIcon();
  icon.iconSet = {
    icon16x16Url: undefined,
    icon32x32Url: 'fake-base64-data',
  };
  await waitForElementUpdate(icon);

  const span = getSpanFromIcon(icon);
  assertTrue(span.classList.contains('keep-color'));
  assertTrue(
      window.getComputedStyle(span).backgroundImage.includes('image-set'));
  assertFalse(window.getComputedStyle(span).backgroundImage.includes('1dppx'));
  assertTrue(window.getComputedStyle(span).backgroundImage.includes('2dppx'));

  done();
}


export async function testIconSetWithBothDPI(done: () => void) {
  const icon = await getIcon();
  icon.iconSet = {
    icon16x16Url: 'fake-base64-data',
    icon32x32Url: 'fake-base64-data',
  };
  await waitForElementUpdate(icon);

  const span = getSpanFromIcon(icon);
  assertTrue(span.classList.contains('keep-color'));
  assertTrue(
      window.getComputedStyle(span).backgroundImage.includes('image-set'));
  assertTrue(window.getComputedStyle(span).backgroundImage.includes('1dppx'));
  assertTrue(window.getComputedStyle(span).backgroundImage.includes('2dppx'));

  done();
}
