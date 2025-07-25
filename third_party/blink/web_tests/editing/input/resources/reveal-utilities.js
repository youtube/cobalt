function offsetFromViewportTop(element)
{
    return element.getClientRects()[0].top;
}

function offsetFromViewportBottom(element)
{
    var rect = element.getClientRects()[0];
    return window.innerHeight - rect.top - rect.height;
}

function offsetOfMiddleFromViewportTop(element)
{
    return element.getClientRects()[0].top + Math.round(element.getClientRects()[0].height / 2);
}

function copyText()
{
    var copy = document.getElementById("copy");
    copy.focus();
    document.execCommand("selectall");
    document.execCommand("copy");
}

function generateNumbers(from, to, zeroPaddedWidth, delimiter)
{
    var result = "";
    for (var i = from; i <= to; ++i) {
        var number = i.toString();
        while (number.length < zeroPaddedWidth)
            number = "0" + number;
        result += number + delimiter;
    }
    return result;
}

function assertInputIsInTheBottomEdgeOfViewport()
{
    var offsetOfInput = offsetFromViewportBottom(document.getElementById("input"));
    document.getElementById("results").innerHTML += "ScrollVertically: " +
        (Math.abs(offsetOfInput) <= 3 ?
         "PASS" :
         "FAIL<br>offsetOfInput: " + offsetOfInput);
}

function assertInputIsInTheMiddleOfViewport()
{
    var viewportMiddle = Math.round(window.innerHeight / 2);
    var offsetOfInput = offsetOfMiddleFromViewportTop(document.getElementById("input"));
    document.getElementById("results").innerHTML += "ScrollVertically: " +
        (Math.abs(offsetOfInput - viewportMiddle) <= 3 ?
         "PASS" :
         "FAIL<br>viewportMiddle: " + viewportMiddle + ", offsetOfInput: " + offsetOfInput);
}

function performVerticalScrollingTest()
{
    var initialOffset = offsetFromViewportTop(document.body.children[0]);
    document.getElementById("input").focus();
    if (window.eventSender) {
        while (offsetFromViewportTop(document.body.children[0]) < initialOffset)
            eventSender.keyDown("PageUp");
        return true;
    }
    return false;
}

function performVerticalScrollingInputTest()
{
    if (performVerticalScrollingTest())
        eventSender.keyDown("a");
}

function performVerticalScrollingPasteTest()
{
    copyText();
    if (performVerticalScrollingTest()) {
        document.execCommand("selectall");
        document.execCommand("paste");
    }
}

function performJumpAtTheEdgeTest()
{
    var textArea = document.getElementById("input");
    textArea.focus();
    if (window.eventSender) {
        var previousScrollTop = 0, currentScrollTop = 0;
        var jumpDetected = false;
        for (var i = 0; i < 120; ++i) {
            previousScrollTop = document.documentElement.scrollTop;
            eventSender.keyDown("\r", []);
            currentScrollTop = document.documentElement.scrollTop;
            // Smooth scrolls are allowed.
            if (Math.abs(previousScrollTop - currentScrollTop) > 24) {
                jumpDetected = true;
                break;
            }
        }
        if (!jumpDetected)
            document.write("PASS");
        else
            document.write("FAIL<br>Jump scroll from " + previousScrollTop + " to " + currentScrollTop);
    }
}
