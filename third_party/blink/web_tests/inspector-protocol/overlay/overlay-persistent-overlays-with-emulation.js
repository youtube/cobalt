(async function(testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
    <style>
      #grid {
        display: grid;
      }
    </style>
    <div id="grid"><div>A</div><div>B</div></div>
  `, 'Verifies that Overlay.setShowGridOverlays works together with Emulation.setDeviceMetricsOverride.');

  await dp.DOM.enable();
  await dp.CSS.enable();
  await dp.Emulation.enable();
  await dp.Overlay.enable();

  await dp.Emulation.setDeviceMetricsOverride({
    width: 0,
    height: 0,
    deviceScaleFactor: 2,
    mobile: true,
    scale: 1,
    screenWidth: 800,
    screenHeight: 600,
    positionX: 0,
    positionY: 0,
    dontSetVisibleSize: true,
    screenOrientation: {
      type: 'landscapePrimary',
      angle: 90
    }
  });

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);
  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#grid');
  await dp.Overlay.setShowGridOverlays({
    gridNodeHighlightConfigs: [{
      nodeId,
      gridHighlightConfig: {
        showTrackSizes: true,
      },
    }]
  });

  // Wait for overlay rendering to finish by requesting an animation frame.
  await session.evaluate(() => {
    return new Promise(resolve => requestAnimationFrame(resolve));
  });

  async function getGridLabelPositions() {
    return await session.evaluate(() => {
      return internals.evaluateInInspectorOverlay(`(function () {
        const labels = document.querySelectorAll('.grid-label-content');
        const positions = [];
        for (const label of labels) {
          const rect = label.getBoundingClientRect();
          positions.push({
            left: rect.left,
            right: rect.right,
            bottom: rect.bottom,
            top: rect.top,
            width: rect.width,
            height: rect.height,
          });
        }
        return JSON.stringify(positions);
      })()`);
    });
  }

  const labelPositions = JSON.parse(await getGridLabelPositions());

  testRunner.log('Expected 3 track size labels; actual: ' + labelPositions.length);
  testRunner.log('Positions: ');

  for (const position of labelPositions) {
    testRunner.log(position);
  }

  testRunner.completeTest();
});
