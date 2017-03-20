var window = { 'tests': new Map() };

loadRelativeToScript('pairCyclicWeakMap.js');

var test = window.tests.get('pairCyclicWeakMap');
test.load(1);
for (var i = 0; i < 100; i++) {
  test.makeGarbage(2000);
  gc();
}
test.unload();
gc();
