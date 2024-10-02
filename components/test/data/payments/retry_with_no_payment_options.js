/*
 * Copyright 2018 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

var gShowPromise = null;
var gPaymentResponse = null;

/**
 * Launches the PaymentRequest UI
 *
 * Legacy entry-point for basic-card tests; to be removed.
 */
function buy() {
  var options = {};
  getPaymentResponse(options)
      .then(function(response) {
        gPaymentResponse = response;
      });
}

/**
 * Launches the PaymentRequest UI. The promise from show() is saved into
 * gShowPromise; call processShowResponse after you are done interacting with
 * the UI.
 *
 * @param {sequence<PaymentMethodData>} methodData An array of payment method
 *        objects.
 */
function buyWithMethods(methodData) {
  var options = {};
  gShowPromise = getPaymentResponseWithMethod(options, methodData);
}

/**
 * Waits for the outstanding gShowPromise to resolve, and then saves the
 * response as gPaymentResponse and sets the response as the HTML body text for
 * test consumption.
 */
async function processShowResponse() {
  gPaymentResponse = await gShowPromise;
  print(JSON.stringify(gPaymentResponse, undefined, 2));
}

/**
 * Retry PaymentRequest UI with indicating validation error messages.
 *
 * @param {PaymentValidationErrors} validationErrors Represent validation errors
 */
function retry(validationErrors) {
  if (gPaymentResponse == null) {
    return;
  }

  gPaymentResponse.retry(validationErrors);
}
