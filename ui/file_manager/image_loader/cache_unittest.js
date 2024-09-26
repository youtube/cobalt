// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {LoadImageRequest} from './load_image_request.js';


export function testCreateCacheKey() {
  const key = LoadImageRequest.cacheKey({url: 'http://example.com/image.jpg'});
  assertTrue(!!key);
}

export function testNotCreateCacheKey() {
  let key = LoadImageRequest.cacheKey({url: 'data:xxx'});
  assertFalse(!!key);

  key = LoadImageRequest.cacheKey({url: 'DaTa:xxx'});
  assertFalse(!!key);
}
