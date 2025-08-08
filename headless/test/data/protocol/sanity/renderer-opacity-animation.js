// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
    let {page, session, dp} = await testRunner.startWithFrameControl(
        'Tests that animating layer transparency produces correct pixels');

    await dp.Runtime.enable();
    await dp.HeadlessExperimental.enable();

    let RendererTestHelper =
        await testRunner.loadScript('../helpers/renderer-test-helper.js');
    let {httpInterceptor, frameNavigationHelper, virtualTimeController} =
        await (new RendererTestHelper(testRunner, dp, page)).init();

    // Animate the opacity of the div from 0 to 1 after a 1s delay.
    httpInterceptor.addResponse(
        `http://example.com/`,
        `<style>
           body { margin: 0 }
           div {
             animation-delay: 1s;
             opacity: 0;
             animation-name: animation;
             animation-fill-mode: forwards;
             animation-duration: 1s;
             background-color: green;
             width: 100vw;
             height: 100vh;
           }
           @keyframes animation {
             0% { opacity: 1; }
             100% { opacity: 1; }
           }
         </style>
         <div></div>`);

    await virtualTimeController.initialize(100);
    await frameNavigationHelper.navigate('http://example.com/');
    // Give page 500ms before capturing first screenshot. The screenshot
    // should fall into animamtion-delay interval, so no animation effects
    // should be present and the point should be white.
    await virtualTimeController.grantTime(500);
    const ctx = await virtualTimeController.captureScreenshot();
    let rgba = ctx.getImageData(25, 25, 1, 1).data;
    testRunner.log(`rgba @(25,25) before animaion started: ${rgba}`);
    // After additional 550ms, the animation should have started and the test
    // point should be green.
    await virtualTimeController.grantTime(550);
    const ctx2 = await virtualTimeController.captureScreenshot();
    rgba = ctx2.getImageData(25, 25, 1, 1).data;
    testRunner.log(`rgba @(25,25) after animation started: ${rgba}`);
    testRunner.completeTest();
  })
