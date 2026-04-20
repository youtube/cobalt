(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank('Tests that DOM.resolveNode can resolve into isolated worlds.');

  await dp.Runtime.enable();
  await dp.Page.enable();
  await session.evaluate(() => document.body.dontFindThis = true);

  // Get reference to document.body in default context.
  const mainWorldNode = (await dp.Runtime.evaluate({expression: 'document.body'})).result.result.objectId;
  const {backendNodeId} = (await dp.DOM.describeNode({objectId: mainWorldNode})).result.node;

  // Resolve reference in a new isolated world.
  const frameId = (await dp.Page.getResourceTree()).result.frameTree.frame.id;
  const isolatedContext = (await dp.Page.createIsolatedWorld({frameId, worldName: 'Test world'})).result.executionContextId;
  const isolatedObjectId = (await dp.DOM.resolveNode({backendNodeId, executionContextId: isolatedContext})).result.object.objectId;

  testRunner.log(await dp.Runtime.callFunctionOn({
    functionDeclaration: `function(){
      if ('dontFindThis' in this)
        return 'FAILED.';
      return this === document.body ? 'SUCCESS' : 'FAILURE';
    }`,
    returnByValue: true,
    objectId: isolatedObjectId,
  }));
  testRunner.completeTest();
})
