description("Test file can't be readed or uploaded after the file on disk changes.");

var tempFileContent = "1234567890";
var tempFileName = "file-last-modified-after-delete.tmp";
var testStartTime = new Date();
var lastModifiedDate;

function onFileDrop(file)
{
    var xhr = new XMLHttpRequest();
    xhr.open("POST", "http://127.0.0.1:8000/xmlhttprequest/resources/post-echo.cgi", false);
    xhr.send(file);
    if (xhr.responseText == tempFileContent)
        testPassed("Expected response data received.");
    else
        testFailed("Unexpected response data received: " + xhr.responseText);

    var reader = new FileReader();
    reader.readAsText(file);
    reader.onerror = () => testFailed("Unexpected error reading blob");
    reader.onload = () => {
        if (reader.result == tempFileContent) {
            testPassed("Expected read result received.");
        } else {
            testFailed("Unexpected read result received: " + reader.result);
        }
    };
    reader.onloadend = () => {
        // Touch the temp file.
        touchTempFile(tempFileName);

        // Upload the touched file. We should receive an exception since the file has been changed.
        xhr.open("POST", "http://127.0.0.1:8000/xmlhttprequest/resources/post-echo.cgi", false);
        try {
          xhr.send(file);
          testFailed("Unexpected response data received: " + xhr.responseText);
        } catch (ex) {
          testPassed("Expected exception received.");
        }

        reader.readAsText(file);
        reader.onerror = () => testPassed("Expected error reading touched blob received.");
        reader.onload = () => testFailed("Unexpected read result received after touch.");
        reader.onloadend = cleanUp;
    }
}

function runTest()
{
    var tempFilePath = createTempFile(tempFileName, tempFileContent);
    if (tempFilePath.length == 0)
        return;

    setFileInputDropCallback(onFileDrop);
    eventSender.beginDragWithFiles([tempFilePath]);
    moveMouseToCenterOfElement(fileInput);
    eventSender.mouseUp();
}

function cleanUp() {
    // Clean up after ourselves
    removeFileInputElement();
    finishJSTest();
}

if (window.eventSender) {
    window.jsTestIsAsync = true;
    runTest();
} else {
    testFailed("This test is not interactive, please run using DumpRenderTree");
}

var successfullyParsed = true;
