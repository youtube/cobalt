if (this.importScripts) {
    importScripts('fs-worker-common.js');
    importScripts('../../../resources/js-test.js');
    importScripts('fs-test-util.js');
}

description("Obtaining URL from DirectoryEntry.");

var fileSystem = null;
var testDirectoryName = 'testDirectory';
var testDirectoryURI = null;

function errorCallback(error) {
    testFailed("Error occurred:" + error.name);
    finishJSTest();
}

function entryToURL(entry) {
    testDirectoryURL = entry.toURL();
    shouldBe("testDirectoryURL", "'filesystem:file:///temporary/testDirectory'");
    finishJSTest();
}

function createTestDirectory() {
    fileSystem.root.getDirectory(testDirectoryName, {create:true}, entryToURL, errorCallback);
}

function fileSystemCallback(fs) {
    fileSystem = fs;
    removeAllInDirectory(fileSystem.root, createTestDirectory, errorCallback);
}

var jsTestIsAsync = true;
webkitRequestFileSystem(TEMPORARY, 100, fileSystemCallback, errorCallback);
