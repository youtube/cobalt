description("This test checks whether orphaned workers exit under various conditions");

if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
    waitUntilWorkerThreadsExit(runTests);
} else {
    debug("NOTE: This test relies on functionality in DumpRenderTree to detect when workers have exited - test results will be incorrect when run in a browser.");
    runTests();
}

// Contains tests for dedicated-worker-specific lifecycle functionality.
function runTests()
{
    // Start a worker, drop/GC our reference to it, make sure it exits.
    var worker = createWorker();
    worker.postMessage("ping");
    worker.onmessage = function(event) {
        if (window.testRunner) {
            if (internals.workerThreadCount == 1)
                testPassed("Orphaned worker thread created.");
            else
                testFailed("After thread creation: internals.workerThreadCount = " + internals.workerThreadCount);
        }

        // Orphan our worker (no more references to it) and wait for it to exit.
        worker.onmessage = 0;
        worker.terminate();
        worker = 0;
        waitUntilWorkerThreadsExit(orphanedWorkerExited);
    }
}

function orphanedWorkerExited()
{
    testPassed("Orphaned worker thread exited.");
    // Start a worker, drop/GC our reference to it, make sure it exits.
    var worker = createWorker();
    worker.postMessage("ping");
    worker.onmessage = function(event) {
        if (window.testRunner) {
            if (internals.workerThreadCount == 1)
                testPassed("Orphaned timeout worker thread created.");
            else
                testFailed("After thread creation: internals.workerThreadCount = " + internals.workerThreadCount);
        }
        // Send a message that starts up an async operation, to make sure the thread exits when it completes.
        // FIXME: Disabled for now - re-enable when bug 28702 is fixed.
        //worker.postMessage("eval setTimeout('', 10)");

        // Orphan our worker (no more references to it) and wait for it to exit.
        worker.onmessage = 0;
        worker.terminate();
        worker = 0;
        waitUntilWorkerThreadsExit(orphanedTimeoutWorkerExited);
    }
}

function orphanedTimeoutWorkerExited()
{
    testPassed("Orphaned timeout worker thread exited.");
    done();
}
