// Given a piece of 'code', runs it in the worklet, then once loaded finish the
// test (content_shell will ensure a full frame update before exit).
//
// Usage:
//   importPaintWorkletThenEndTest('/* worklet code goes here. */');

function importPaintWorkletThenEndTest(code) {
    if (window.testRunner) {
      testRunner.waitUntilDone();
    }

    var blob = new Blob([code], {type: 'text/javascript'});
    CSS.paintWorklet.addModule(URL.createObjectURL(blob)).then(function() {
        if (window.testRunner) {
            testRunner.notifyDone();
        }
    });
}
