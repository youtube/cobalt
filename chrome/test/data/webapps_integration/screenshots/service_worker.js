// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

self.addEventListener('fetch', event => {
  event.respondWith(fetch(event.request).catch(_ => {
    return new Response('Offline test.');
  }));
});
