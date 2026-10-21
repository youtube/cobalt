function createSquareCompositedHighlight(node)
{
  return _createHighlight(node, "squaredHighlight highlightDiv composited");
}

function createCompositedHighlight(node)
{
  return _createHighlight(node, "highlightDiv composited");
}

function createHighlight(node)
{
  return _createHighlight(node, "highlightDiv");
}

function createHighlightRect(rect)
{
  return _createHighlightRect(rect, "highlightDiv");
}

function _createHighlight(node, classes) {
  return _createHighlightRect(node.getBoundingClientRect(), classes);
}

function _createHighlightRect(rect, classes) {
  var div = document.createElement('div');
  div.setAttribute('id', 'highlight');
  div.setAttribute('class', classes);
  document.body.appendChild(div);

  div.style.top = rect.top + "px";
  div.style.left = rect.left + "px";
  div.style.width = rect.width + "px";
  div.style.height = rect.height + "px";

  return div;
}

function useMockHighlight() {
  if (window.internals)
    internals.settings.setMockGestureTapHighlightsEnabled(true);
}

function description(s) {
  var div = document.createElement('div');
  div.textContent = s;
  document.body.appendChild(div);
}

function debug(s) {
  description(s);
}

function testHighlightTarget() {
    useMockHighlight();

    var clientRect = document.getElementById('highlightTarget').getBoundingClientRect();
    x = (clientRect.left + clientRect.right) / 2;
    y = (clientRect.top + clientRect.bottom) / 2;
    if (window.testRunner)
        testRunner.waitUntilDone();

    if (window.eventSender) {
        eventSender.gestureTapDown(x, y, 30, 30);
        eventSender.gestureShowPress(x, y, 30, 30);
        window.setTimeout(function() { testRunner.notifyDone(); }, 0);
    } else {
        debug("This test requires eventSender");
    }
}
