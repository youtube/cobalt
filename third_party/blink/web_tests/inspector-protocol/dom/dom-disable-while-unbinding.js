(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
    '<style>#v{color:red}</style><div id=v>target</div><script>window.removeIt=()=>v.remove()</script>',
    'Tests that disabling domains while paused on node removal DOM breakpoint works.');

  await dp.Runtime.enable();
  await dp.Debugger.enable();
  await dp.DOM.enable();
  await dp.CSS.enable();

  const { result: doc } = await dp.DOM.getDocument({ depth: -1, pierce: true });
  const { result: node } = await dp.DOM.querySelector({ nodeId: doc.root.nodeId, selector: '#v' });
  await dp.DOMDebugger.setDOMBreakpoint({ nodeId: node.nodeId, type: 'node-removed' });

  testRunner.log('removing the node');
  const evaluated = dp.Runtime.evaluate({ expression: 'window.removeIt()' });
  await dp.Debugger.oncePaused();
  testRunner.log('paused in debugger, disabling domains');
  await dp.CSS.disable();
  await dp.DOM.disable();
  testRunner.log('resuming');
  await dp.Debugger.resume();
  await evaluated;
  testRunner.log('success');

  testRunner.completeTest();
})
