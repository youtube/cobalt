// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests multiple screens details origin and size.');

  const HttpInterceptor =
      await testRunner.loadScript('../helpers/http-interceptor.js');
  const httpInterceptor = await (new HttpInterceptor(testRunner, dp)).init();

  httpInterceptor.setDisableRequestedUrlsLogging(true);
  httpInterceptor.addResponse(
      'https://example.com/index.html', `<html></html>`);

  await dp.Browser.grantPermissions({permissions: ['windowManagement']});

  await session.navigate('https://example.com/index.html');

  const result = await session.evaluateAsync(async () => {
    const screenDetails = await getScreenDetails();
    const screenInfos = screenDetails.screens.map(
        s => `${s.label}: ${s.left},${s.top} ${s.width}x${s.height}` +
            ` isPrimary=${s.isPrimary} isExtended=${s.isExtended}`);
    return screenInfos.join('\n');
  });

  testRunner.log(result);

  testRunner.completeTest();
})
