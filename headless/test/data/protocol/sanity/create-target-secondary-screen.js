// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests Target.createTarget() on a secodnary screen.');

  await dp.Browser.grantPermissions({permissions: ['windowManagement']});

  const {sessionId} =
      (await testRunner.browserP().Target.attachToBrowserTarget({})).result;
  const bp = (new TestRunner.Session(testRunner, sessionId)).protocol;

  const HttpInterceptor =
      await testRunner.loadScript('../helpers/http-interceptor.js');
  const httpInterceptor = await (new HttpInterceptor(testRunner, bp)).init();
  httpInterceptor.setDisableRequestedUrlsLogging(true);

  httpInterceptor.addResponse(
      'https://example.com/index.html', `<html></html>`);

  const {targetId} = (await session.protocol.Target.createTarget({
                       'url': 'about:blank',
                       'left': 800,
                       'top': 100,
                       'width': 400,
                       'height': 300
                     })).result;

  const createdTargetSession = await session.attachChild(targetId);

  await createdTargetSession.navigate('https://example.com/index.html');

  const screen = await createdTargetSession.evaluateAsync(async () => {
    console.log(`Window: ${screenX},${screenY} ${outerWidth}x${outerHeight}`);
    const cs = (await getScreenDetails()).currentScreen;
    return cs.label;
  });

  // Expect screen #2.
  testRunner.log(`Screen: ${screen}`);

  testRunner.completeTest();
})
