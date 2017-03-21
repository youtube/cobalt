// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "config.h"
#include "bindings/core/v8/ScriptStreamer.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/ScriptStreamerThread.h"
#include "bindings/core/v8/ScriptStreamingMode.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/dom/PendingScript.h"
#include "core/frame/Settings.h"
#include "platform/Task.h"
#include "platform/heap/Handle.h"
#include "public/platform/Platform.h"

#include <gtest/gtest.h>
#include <v8.h>

namespace blink {

namespace {

// For the benefit of Oilpan, put the part object PendingScript inside
// a wrapper that's on the Oilpan heap and hold a reference to that wrapper
// from ScriptStreamingTest.
class PendingScriptWrapper : public NoBaseWillBeGarbageCollectedFinalized<PendingScriptWrapper> {
public:
    static PassOwnPtrWillBeRawPtr<PendingScriptWrapper> create()
    {
        return adoptPtrWillBeNoop(new PendingScriptWrapper());
    }

    static PassOwnPtrWillBeRawPtr<PendingScriptWrapper> create(Element* element, ScriptResource* resource)
    {
        return adoptPtrWillBeNoop(new PendingScriptWrapper(element, resource));
    }

    PendingScript& get() { return m_pendingScript; }

    void trace(Visitor* visitor)
    {
        visitor->trace(m_pendingScript);
    }

private:
    PendingScriptWrapper()
    {
    }

    PendingScriptWrapper(Element* element, ScriptResource* resource)
        : m_pendingScript(PendingScript(element, resource))
    {
    }

    PendingScript m_pendingScript;
};

// The bool param for ScriptStreamingTest controls whether to make the main
// thread block and wait for parsing.
class ScriptStreamingTest : public testing::TestWithParam<bool> {
public:
    ScriptStreamingTest()
        : m_scope(v8::Isolate::GetCurrent())
        , m_settings(Settings::create())
        , m_resourceRequest("http://www.streaming-test.com/")
        , m_resource(new ScriptResource(m_resourceRequest, "text/utf-8"))
        , m_pendingScript(PendingScriptWrapper::create(0, m_resource)) // Takes ownership of m_resource.
    {
        m_settings->setV8ScriptStreamingEnabled(true);
        if (GetParam())
            m_settings->setV8ScriptStreamingMode(ScriptStreamingModeAllPlusBlockParsingBlocking);
        m_resource->setLoading(true);
        ScriptStreamer::setSmallScriptThresholdForTesting(0);
    }

    ScriptState* scriptState() const { return m_scope.scriptState(); }
    v8::Isolate* isolate() const { return m_scope.isolate(); }

    PendingScript& pendingScript() const { return m_pendingScript->get(); }

protected:
    void appendData(const char* data)
    {
        m_resource->appendData(data, strlen(data));
        // Yield control to the background thread, so that V8 gets a change to
        // process the data before the main thread adds more. Note that we
        // cannot fully control in what kind of chunks the data is passed to V8
        // (if the V8 is not requesting more data between two appendData calls,
        // V8 will get both chunks together).
        Platform::current()->yieldCurrentThread();
    }

    void appendPadding()
    {
        for (int i = 0; i < 10; ++i) {
            appendData(" /* this is padding to make the script long enough, so "
                "that V8's buffer gets filled and it starts processing "
                "the data */ ");
        }
    }

    void finish()
    {
        m_resource->finish();
        m_resource->setLoading(false);
    }

    void processTasksUntilStreamingComplete()
    {
        WebThread* currentThread = blink::Platform::current()->currentThread();
        while (ScriptStreamerThread::shared()->isRunningTask()) {
            currentThread->postTask(new Task(WTF::bind(&WebThread::exitRunLoop, currentThread)));
            currentThread->enterRunLoop();
        }
        // Once more, because the "streaming complete" notification might only
        // now be in the task queue.
        currentThread->postTask(new Task(WTF::bind(&WebThread::exitRunLoop, currentThread)));
        currentThread->enterRunLoop();
    }

    V8TestingScope m_scope;
    OwnPtr<Settings> m_settings;
    // The Resource and PendingScript where we stream from. These don't really
    // fetch any data outside the test; the test controls the data by calling
    // ScriptResource::appendData.
    ResourceRequest m_resourceRequest;
    ScriptResource* m_resource;
    OwnPtrWillBePersistent<PendingScriptWrapper> m_pendingScript;
};

class TestScriptResourceClient : public ScriptResourceClient {
public:
    TestScriptResourceClient()
        : m_finished(false) { }

    virtual void notifyFinished(Resource*) override { m_finished = true; }

    bool finished() const { return m_finished; }

private:
    bool m_finished;
};

TEST_P(ScriptStreamingTest, CompilingStreamedScript)
{
    // Test that we can successfully compile a streamed script.
    ScriptStreamer::startStreaming(pendingScript(), m_settings.get(), m_scope.scriptState(), PendingScript::ParsingBlocking);
    TestScriptResourceClient client;
    pendingScript().watchForLoad(&client);

    appendData("function foo() {");
    appendPadding();
    appendData("return 5; }");
    appendPadding();
    appendData("foo();");
    EXPECT_FALSE(client.finished());
    finish();

    // Process tasks on the main thread until the streaming background thread
    // has completed its tasks.
    processTasksUntilStreamingComplete();
    EXPECT_TRUE(client.finished());
    bool errorOccurred = false;
    ScriptSourceCode sourceCode = pendingScript().getSource(KURL(), errorOccurred);
    EXPECT_FALSE(errorOccurred);
    EXPECT_TRUE(sourceCode.streamer());
    v8::TryCatch tryCatch;
    v8::Handle<v8::Script> script = V8ScriptRunner::compileScript(sourceCode, isolate());
    EXPECT_FALSE(script.IsEmpty());
    EXPECT_FALSE(tryCatch.HasCaught());
}

TEST_P(ScriptStreamingTest, CompilingStreamedScriptWithParseError)
{
    // Test that scripts with parse errors are handled properly. In those cases,
    // the V8 side typically finished before loading finishes: make sure we
    // handle it gracefully.
    ScriptStreamer::startStreaming(pendingScript(), m_settings.get(), m_scope.scriptState(), PendingScript::ParsingBlocking);
    TestScriptResourceClient client;
    pendingScript().watchForLoad(&client);
    appendData("function foo() {");
    appendData("this is the part which will be a parse error");
    // V8 won't realize the parse error until it actually starts parsing the
    // script, and this happens only when its buffer is filled.
    appendPadding();

    EXPECT_FALSE(client.finished());

    // Force the V8 side to finish before the loading.
    processTasksUntilStreamingComplete();
    EXPECT_FALSE(client.finished());

    finish();
    EXPECT_TRUE(client.finished());

    bool errorOccurred = false;
    ScriptSourceCode sourceCode = pendingScript().getSource(KURL(), errorOccurred);
    EXPECT_FALSE(errorOccurred);
    EXPECT_TRUE(sourceCode.streamer());
    v8::TryCatch tryCatch;
    v8::Handle<v8::Script> script = V8ScriptRunner::compileScript(sourceCode, isolate());
    EXPECT_TRUE(script.IsEmpty());
    EXPECT_TRUE(tryCatch.HasCaught());
}

TEST_P(ScriptStreamingTest, CancellingStreaming)
{
    // Test that the upper layers (PendingScript and up) can be ramped down
    // while streaming is ongoing, and ScriptStreamer handles it gracefully.
    ScriptStreamer::startStreaming(pendingScript(), m_settings.get(), m_scope.scriptState(), PendingScript::ParsingBlocking);
    TestScriptResourceClient client;
    pendingScript().watchForLoad(&client);
    appendData("function foo() {");

    // In general, we cannot control what the background thread is doing
    // (whether it's parsing or waiting for more data). In this test, we have
    // given it so little data that it's surely waiting for more.

    // Simulate cancelling the network load (e.g., because the user navigated
    // away).
    EXPECT_FALSE(client.finished());
    pendingScript().stopWatchingForLoad(&client);
    pendingScript().releaseElementAndClear();
    m_pendingScript = PendingScriptWrapper::create(); // This will destroy m_resource.
    m_resource = 0;

    // The V8 side will complete too. This should not crash. We don't receive
    // any results from the streaming and the client doesn't get notified.
    processTasksUntilStreamingComplete();
    EXPECT_FALSE(client.finished());
}

TEST_P(ScriptStreamingTest, SuppressingStreaming)
{
    // If we notice during streaming that there is a code cache, streaming
    // is suppressed (V8 doesn't parse while the script is loading), and the
    // upper layer (ScriptResourceClient) should get a notification when the
    // script is loaded.
    ScriptStreamer::startStreaming(pendingScript(), m_settings.get(), m_scope.scriptState(), PendingScript::ParsingBlocking);
    TestScriptResourceClient client;
    pendingScript().watchForLoad(&client);
    appendData("function foo() {");
    appendPadding();

    m_resource->setCachedMetadata(V8ScriptRunner::tagForCodeCache(), "X", 1, Resource::CacheLocally);

    appendPadding();
    finish();
    processTasksUntilStreamingComplete();
    EXPECT_TRUE(client.finished());

    bool errorOccurred = false;
    ScriptSourceCode sourceCode = pendingScript().getSource(KURL(), errorOccurred);
    EXPECT_FALSE(errorOccurred);
    // ScriptSourceCode doesn't refer to the streamer, since we have suppressed
    // the streaming and resumed the non-streaming code path for script
    // compilation.
    EXPECT_FALSE(sourceCode.streamer());
}

TEST_P(ScriptStreamingTest, EmptyScripts)
{
    // Empty scripts should also be streamed properly, that is, the upper layer
    // (ScriptResourceClient) should be notified when an empty script has been
    // loaded.
    ScriptStreamer::startStreaming(pendingScript(), m_settings.get(), m_scope.scriptState(), PendingScript::ParsingBlocking);
    TestScriptResourceClient client;
    pendingScript().watchForLoad(&client);

    // Finish the script without sending any data.
    finish();
    // The finished notification should arrive immediately and not be cycled
    // through a background thread.
    EXPECT_TRUE(client.finished());

    bool errorOccurred = false;
    ScriptSourceCode sourceCode = pendingScript().getSource(KURL(), errorOccurred);
    EXPECT_FALSE(errorOccurred);
    EXPECT_FALSE(sourceCode.streamer());
}

TEST_P(ScriptStreamingTest, SmallScripts)
{
    // Small scripts shouldn't be streamed.
    ScriptStreamer::setSmallScriptThresholdForTesting(100);

    ScriptStreamer::startStreaming(pendingScript(), m_settings.get(), m_scope.scriptState(), PendingScript::ParsingBlocking);
    TestScriptResourceClient client;
    pendingScript().watchForLoad(&client);

    appendData("function foo() { }");

    finish();

    // The finished notification should arrive immediately and not be cycled
    // through a background thread.
    EXPECT_TRUE(client.finished());

    bool errorOccurred = false;
    ScriptSourceCode sourceCode = pendingScript().getSource(KURL(), errorOccurred);
    EXPECT_FALSE(errorOccurred);
    EXPECT_FALSE(sourceCode.streamer());
}

TEST_P(ScriptStreamingTest, ScriptsWithSmallFirstChunk)
{
    // If a script is long enough, if should be streamed, even if the first data
    // chunk is small.
    ScriptStreamer::setSmallScriptThresholdForTesting(100);

    ScriptStreamer::startStreaming(pendingScript(), m_settings.get(), m_scope.scriptState(), PendingScript::ParsingBlocking);
    TestScriptResourceClient client;
    pendingScript().watchForLoad(&client);

    // This is the first data chunk which is small.
    appendData("function foo() { }");
    appendPadding();
    appendPadding();
    appendPadding();

    finish();

    processTasksUntilStreamingComplete();
    EXPECT_TRUE(client.finished());
    bool errorOccurred = false;
    ScriptSourceCode sourceCode = pendingScript().getSource(KURL(), errorOccurred);
    EXPECT_FALSE(errorOccurred);
    EXPECT_TRUE(sourceCode.streamer());
    v8::TryCatch tryCatch;
    v8::Handle<v8::Script> script = V8ScriptRunner::compileScript(sourceCode, isolate());
    EXPECT_FALSE(script.IsEmpty());
    EXPECT_FALSE(tryCatch.HasCaught());
}

INSTANTIATE_TEST_CASE_P(ScriptStreamingInstantiation, ScriptStreamingTest, ::testing::Values(false, true));

} // namespace

} // namespace blink
