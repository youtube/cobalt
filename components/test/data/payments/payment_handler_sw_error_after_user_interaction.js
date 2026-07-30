/*
 * Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

let rejectPromise = null;

self.addEventListener('canmakepayment', (evt) => {
  evt.respondWith(true);
});

self.addEventListener('message', (evt) => {
  if (evt.data === 'reject') {
    rejectPromise(new Error('Rejected'));
  }
});

self.addEventListener('paymentrequest', (evt) => {
  evt.respondWith(new Promise((resolve, reject) => {
    rejectPromise = reject;
    evt.openWindow('payment_handler_window_reject.html');
  }));
});
