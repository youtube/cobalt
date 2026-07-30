(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Tests the new Overlay.setShowDisplayCutout method.');

  let response = await dp.Overlay.setShowDisplayCutout({
    displayCutoutConfig: {
      rect: {x: 100, y: 10, width: 120, height: 40},
      shape: 'pill',
      borderRadius: 20,
      contentColor: {r: 0, g: 0, b: 0, a: 1},
    },
  });
  testRunner.log(response);

  response = await dp.Overlay.setShowDisplayCutout({
    displayCutoutConfig: {
      rect: {x: 100, y: 0, width: 210, height: 32},
      shape: 'notch',
      upperRadius: 6,
      lowerRadius: 23,
    },
  });
  testRunner.log(response);

  response = await dp.Overlay.setShowDisplayCutout({
    displayCutoutConfig: {
      rect: {x: 180, y: 0, width: 50, height: 50},
      shape: 'circle',
      cx: 205,
      cy: 25,
      radius: 14,
    },
  });
  testRunner.log(response);

  response = await dp.Overlay.setShowDisplayCutout({
    displayCutoutConfig: {
      rect: {x: 10, y: 0, width: 50, height: 20},
      shape: 'rectangle',
    },
  });
  testRunner.log(response);

  response = await dp.Overlay.setShowDisplayCutout({
    displayCutoutConfig: {
      rect: {x: 10, y: 0, width: 50, height: 20},
      shape: 'triangle',
    },
  });
  testRunner.log(response);

  response = await dp.Overlay.setShowDisplayCutout({
    displayCutoutConfig: {
      rect: {x: -10, y: 0, width: 50, height: 20},
      shape: 'rectangle',
    },
  });
  testRunner.log(response);

  response = await dp.Overlay.setShowDisplayCutout({
    displayCutoutConfig: {
      rect: {x: 10, y: 0, width: 50, height: 20},
      shape: 'pill',
    },
  });
  testRunner.log(response);

  response = await dp.Overlay.setShowDisplayCutout({});
  testRunner.log(response);

  testRunner.completeTest();
})
