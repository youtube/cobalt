// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/ScriptStreamer.h"

#include "bindings/core/v8/ScriptStreamerThread.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/PendingScript.h"
#include "core/fetch/ScriptResource.h"
#include "core/frame/Settings.h"
#include "platform/SharedBuffer.h"
#include "platform/TraceEvent.h"
#include "public/platform/Platform.h"
#include "wtf/MainThread.h"
#include "wtf/text/TextEncodingRegistry.h"

namespace blink {

// For passing data between the main thread (producer) and the streamer thread
// (consumer). The main thread prepares the data (copies it from Resource) and
// the streamer thread feeds it to V8.
class SourceStreamDataQueue {
    WTF_MAKE_NONCOPYABLE(SourceStreamDataQueue);
public:
    SourceStreamDataQueue()
        : m_finished(false) { }

    ~SourceStreamDataQueue()
    {
        while (!m_data.isEmpty()) {
            std::pair<const uint8_t*, size_t> next_data = m_data.takeFirst();
            delete[] next_data.first;
        }
    }

    void produce(const uint8_t* data, size_t length)
    {
        MutexLocker locker(m_mutex);
        m_data.append(std::make_pair(data, length));
        m_haveData.signal();
    }

    void finish()
    {
        MutexLocker locker(m_mutex);
        m_finished = true;
        m_haveData.signal();
    }

    void consume(const uint8_t** data, size_t* length)
    {
        MutexLocker locker(m_mutex);
        while (!tryGetData(data, length))
            m_haveData.wait(m_mutex);
    }

private:
    bool tryGetData(const uint8_t** data, size_t* length)
    {
        if (!m_data.isEmpty()) {
            std::pair<const uint8_t*, size_t> next_data = m_data.takeFirst();
            *data = next_data.first;
            *length = next_data.second;
            return true;
        }
        if (m_finished) {
            *length = 0;
            return true;
        }
        return false;
    }

    WTF::Deque<std::pair<const uint8_t*, size_t> > m_data;
    bool m_finished;
    Mutex m_mutex;
    ThreadCondition m_haveData;
};


// SourceStream implements the streaming interface towards V8. The main
// functionality is preparing the data to give to V8 on main thread, and
// actually giving the data (via GetMoreData which is called on a background
// thread).
class SourceStream : public v8::ScriptCompiler::ExternalSourceStream {
    WTF_MAKE_NONCOPYABLE(SourceStream);
public:
    SourceStream(ScriptStreamer* streamer)
        : v8::ScriptCompiler::ExternalSourceStream()
        , m_streamer(streamer)
        , m_cancelled(false)
        , m_dataPosition(0) { }

    virtual ~SourceStream() { }

    // Called by V8 on a background thread. Should block until we can return
    // some data.
    virtual size_t GetMoreData(const uint8_t** src) override
    {
        ASSERT(!isMainThread());
        {
            MutexLocker locker(m_mutex);
            if (m_cancelled)
                return 0;
        }
        size_t length = 0;
        // This will wait until there is data.
        m_dataQueue.consume(src, &length);
        {
            MutexLocker locker(m_mutex);
            if (m_cancelled)
                return 0;
        }
        return length;
    }

    void didFinishLoading()
    {
        ASSERT(isMainThread());
        m_dataQueue.finish();
    }

    void didReceiveData()
    {
        ASSERT(isMainThread());
        prepareDataOnMainThread();
    }

    void cancel()
    {
        ASSERT(isMainThread());
        // The script is no longer needed by the upper layers. Stop streaming
        // it. The next time GetMoreData is called (or woken up), it will return
        // 0, which will be interpreted as EOS by V8 and the parsing will
        // fail. ScriptStreamer::streamingComplete will be called, and at that
        // point we will release the references to SourceStream.
        {
            MutexLocker locker(m_mutex);
            m_cancelled = true;
        }
        m_dataQueue.finish();
    }

private:
    void prepareDataOnMainThread()
    {
        ASSERT(isMainThread());
        // The Resource must still be alive; otherwise we should've cancelled
        // the streaming (if we have cancelled, the background thread is not
        // waiting).
        ASSERT(m_streamer->resource());

        if (m_streamer->resource()->cachedMetadata(V8ScriptRunner::tagForCodeCache())) {
            // The resource has a code cache, so it's unnecessary to stream and
            // parse the code. Cancel the streaming and resume the non-streaming
            // code path.
            m_streamer->suppressStreaming();
            {
                MutexLocker locker(m_mutex);
                m_cancelled = true;
            }
            m_dataQueue.finish();
            return;
        }

        if (!m_resourceBuffer) {
            // We don't have a buffer yet. Try to get it from the resource.
            SharedBuffer* buffer = m_streamer->resource()->resourceBuffer();
            if (!buffer)
                return;
            m_resourceBuffer = RefPtr<SharedBuffer>(buffer);
        }

        // Get as much data from the ResourceBuffer as we can.
        const char* data = 0;
        Vector<const char*> chunks;
        Vector<unsigned> chunkLengths;
        size_t dataLength = 0;
        while (unsigned length = m_resourceBuffer->getSomeData(data, m_dataPosition)) {
            // FIXME: Here we can limit based on the total length, if it turns
            // out that we don't want to give all the data we have (memory
            // vs. speed).
            chunks.append(data);
            chunkLengths.append(length);
            dataLength += length;
            m_dataPosition += length;
        }
        // Copy the data chunks into a new buffer, since we're going to give the
        // data to a background thread.
        if (dataLength > 0) {
            uint8_t* copiedData = new uint8_t[dataLength];
            unsigned offset = 0;
            for (size_t i = 0; i < chunks.size(); ++i) {
                memcpy(copiedData + offset, chunks[i], chunkLengths[i]);
                offset += chunkLengths[i];
            }
            m_dataQueue.produce(copiedData, dataLength);
        }
    }

    ScriptStreamer* m_streamer;

    // For coordinating between the main thread and background thread tasks.
    // Guarded by m_mutex.
    bool m_cancelled;
    Mutex m_mutex;

    unsigned m_dataPosition; // Only used by the main thread.
    RefPtr<SharedBuffer> m_resourceBuffer; // Only used by the main thread.
    SourceStreamDataQueue m_dataQueue; // Thread safe.
};

size_t ScriptStreamer::kSmallScriptThreshold = 30 * 1024;

void ScriptStreamer::startStreaming(PendingScript& script, Settings* settings, ScriptState* scriptState, PendingScript::Type scriptType)
{
    // We don't yet know whether the script will really be streamed. E.g.,
    // suppressing streaming for short scripts is done later. Record only the
    // sure negative cases here.
    bool startedStreaming = startStreamingInternal(script, settings, scriptState, scriptType);
    if (!startedStreaming)
        blink::Platform::current()->histogramEnumeration(startedStreamingHistogramName(scriptType), 0, 2);
}

void ScriptStreamer::streamingCompleteOnBackgroundThread()
{
    ASSERT(!isMainThread());
    MutexLocker locker(m_mutex);
    m_parsingFinished = true;
    // In the blocking case, the main thread is normally waiting at this
    // point, but it can also happen that the load is not yet finished
    // (e.g., a parse error). In that case, notifyFinished will be called
    // eventually and it will not wait on m_parsingFinishedCondition.

    // In the non-blocking case, notifyFinished might already be called, or it
    // might be called in the future. In any case, do the cleanup here.
    if (m_mainThreadWaitingForParserThread) {
        m_parsingFinishedCondition.signal();
    } else {
        callOnMainThread(WTF::bind(&ScriptStreamer::streamingComplete, this));
    }
}

void ScriptStreamer::cancel()
{
    ASSERT(isMainThread());
    // The upper layer doesn't need the script any more, but streaming might
    // still be ongoing. Tell SourceStream to try to cancel it whenever it gets
    // the control the next time. It can also be that V8 has already completed
    // its operations and streamingComplete will be called soon.
    m_detached = true;
    m_resource = 0;
    m_stream->cancel();
}

void ScriptStreamer::suppressStreaming()
{
    MutexLocker locker(m_mutex);
    ASSERT(!m_loadingFinished);
    // It can be that the parsing task has already finished (e.g., if there was
    // a parse error).
    m_streamingSuppressed = true;
}

void ScriptStreamer::notifyAppendData(ScriptResource* resource)
{
    ASSERT(isMainThread());
    ASSERT(m_resource == resource);
    {
        MutexLocker locker(m_mutex);
        if (m_streamingSuppressed)
            return;
    }
    if (!m_haveEnoughDataForStreaming) {
        // Even if the first data chunk is small, the script can still be big
        // enough - wait until the next data chunk comes before deciding whether
        // to start the streaming.
        if (resource->resourceBuffer()->size() < kSmallScriptThreshold) {
            return;
        }
        m_haveEnoughDataForStreaming = true;
        const char* histogramName = startedStreamingHistogramName(m_scriptType);
        if (ScriptStreamerThread::shared()->isRunningTask()) {
            // At the moment we only have one thread for running the tasks. A
            // new task shouldn't be queued before the running task completes,
            // because the running task can block and wait for data from the
            // network.
            suppressStreaming();
            blink::Platform::current()->histogramEnumeration(histogramName, 0, 2);
            return;
        }
        ASSERT(m_task);
        // ScriptStreamer needs to stay alive as long as the background task is
        // running. This is taken care of with a manual ref() & deref() pair;
        // the corresponding deref() is in streamingComplete or in
        // notifyFinished.
        ref();
        ScriptStreamingTask* task = new ScriptStreamingTask(m_task.release(), this);
        ScriptStreamerThread::shared()->postTask(task);
        blink::Platform::current()->histogramEnumeration(histogramName, 1, 2);
    }
    m_stream->didReceiveData();
}

void ScriptStreamer::notifyFinished(Resource* resource)
{
    ASSERT(isMainThread());
    ASSERT(m_resource == resource);
    // A special case: empty and small scripts. We didn't receive enough data to
    // start the streaming before this notification. In that case, there won't
    // be a "parsing complete" notification either, and we should not wait for
    // it.
    if (!m_haveEnoughDataForStreaming) {
        const char* histogramName = startedStreamingHistogramName(m_scriptType);
        blink::Platform::current()->histogramEnumeration(histogramName, 0, 2);
        suppressStreaming();
    }
    m_stream->didFinishLoading();
    m_loadingFinished = true;

    if (shouldBlockMainThread()) {
        // Make the main thead wait until the streaming is complete, to make
        // sure that the script gets the main thread's attention as early as
        // possible (for possible compiling, if the client wants to do it
        // right away). Note that blocking here is not any worse than the
        // non-streaming code path where the main thread eventually blocks
        // to parse the script.
        TRACE_EVENT0("v8", "v8.mainThreadWaitingForParserThread");
        MutexLocker locker(m_mutex);
        while (!isFinished()) {
            ASSERT(!m_parsingFinished);
            ASSERT(!m_streamingSuppressed);
            m_mainThreadWaitingForParserThread = true;
            m_parsingFinishedCondition.wait(m_mutex);
        }
    }

    // Calling notifyFinishedToClient can result into the upper layers dropping
    // references to ScriptStreamer. Keep it alive until this function ends.
    RefPtr<ScriptStreamer> protect(this);

    notifyFinishedToClient();

    if (m_mainThreadWaitingForParserThread) {
        ASSERT(m_parsingFinished);
        ASSERT(!m_streamingSuppressed);
        // streamingComplete won't be called, so do the ramp-down work
        // here.
        deref();
    }
}

ScriptStreamer::ScriptStreamer(ScriptResource* resource, v8::ScriptCompiler::StreamedSource::Encoding encoding, PendingScript::Type scriptType, ScriptStreamingMode mode)
    : m_resource(resource)
    , m_detached(false)
    , m_stream(new SourceStream(this))
    , m_source(m_stream, encoding) // m_source takes ownership of m_stream.
    , m_client(0)
    , m_loadingFinished(false)
    , m_parsingFinished(false)
    , m_haveEnoughDataForStreaming(false)
    , m_streamingSuppressed(false)
    , m_scriptType(scriptType)
    , m_scriptStreamingMode(mode)
    , m_mainThreadWaitingForParserThread(false)
{
}

void ScriptStreamer::streamingComplete()
{
    // The background task is completed; do the necessary ramp-down in the main
    // thread.
    ASSERT(isMainThread());

    // It's possible that the corresponding Resource was deleted before V8
    // finished streaming. In that case, the data or the notification is not
    // needed. In addition, if the streaming is suppressed, the non-streaming
    // code path will resume after the resource has loaded, before the
    // background task finishes.
    if (m_detached || m_streamingSuppressed) {
        deref();
        return;
    }

    // We have now streamed the whole script to V8 and it has parsed the
    // script. We're ready for the next step: compiling and executing the
    // script.
    notifyFinishedToClient();

    // The background thread no longer holds an implicit reference.
    deref();
}

void ScriptStreamer::notifyFinishedToClient()
{
    ASSERT(isMainThread());
    // Usually, the loading will be finished first, and V8 will still need some
    // time to catch up. But the other way is possible too: if V8 detects a
    // parse error, the V8 side can complete before loading has finished. Send
    // the notification after both loading and V8 side operations have
    // completed. Here we also check that we have a client: it can happen that a
    // function calling notifyFinishedToClient was already scheduled in the task
    // queue and the upper layer decided that it's not interested in the script
    // and called removeClient.
    {
        MutexLocker locker(m_mutex);
        if (!isFinished())
            return;
    }
    if (m_client)
        m_client->notifyFinished(m_resource);
}

const char* ScriptStreamer::startedStreamingHistogramName(PendingScript::Type scriptType)
{
    switch (scriptType) {
    case PendingScript::ParsingBlocking:
        return "WebCore.Scripts.ParsingBlocking.StartedStreaming";
        break;
    case PendingScript::Deferred:
        return "WebCore.Scripts.Deferred.StartedStreaming";
        break;
    case PendingScript::Async:
        return "WebCore.Scripts.Async.StartedStreaming";
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return 0;
}

bool ScriptStreamer::startStreamingInternal(PendingScript& script, Settings* settings, ScriptState* scriptState, PendingScript::Type scriptType)
{
    ASSERT(isMainThread());
    if (!settings || !settings->v8ScriptStreamingEnabled())
        return false;
    if (settings->v8ScriptStreamingMode() == ScriptStreamingModeOnlyAsyncAndDefer
        && scriptType == PendingScript::ParsingBlocking)
        return false;

    ScriptResource* resource = script.resource();
    ASSERT(!resource->isLoaded());
    if (!resource->url().protocolIsInHTTPFamily())
        return false;
    if (resource->resourceToRevalidate()) {
        // This happens e.g., during reloads. We're actually not going to load
        // the current Resource of the PendingScript but switch to another
        // Resource -> don't stream.
        return false;
    }
    // We cannot filter out short scripts, even if we wait for the HTTP headers
    // to arrive. In general, the web servers don't seem to send the
    // Content-Length HTTP header for scripts.

    WTF::TextEncoding textEncoding(resource->encoding());
    const char* encodingName = textEncoding.name();

    // Here's a list of encodings we can use for streaming. These are
    // the canonical names.
    v8::ScriptCompiler::StreamedSource::Encoding encoding;
    if (strcmp(encodingName, "windows-1252") == 0
        || strcmp(encodingName, "ISO-8859-1") == 0
        || strcmp(encodingName, "US-ASCII") == 0) {
        encoding = v8::ScriptCompiler::StreamedSource::ONE_BYTE;
    } else if (strcmp(encodingName, "UTF-8") == 0) {
        encoding = v8::ScriptCompiler::StreamedSource::UTF8;
    } else {
        // We don't stream other encodings; especially we don't stream two byte
        // scripts to avoid the handling of byte order marks. Most scripts are
        // Latin1 or UTF-8 anyway, so this should be enough for most real world
        // purposes.
        return false;
    }

    if (!scriptState->contextIsValid())
        return false;
    ScriptState::Scope scope(scriptState);

    // The Resource might go out of scope if the script is no longer needed. We
    // will soon call PendingScript::setStreamer, which makes the PendingScript
    // notify the ScriptStreamer when it is destroyed.
    RefPtr<ScriptStreamer> streamer = adoptRef(new ScriptStreamer(resource, encoding, scriptType, settings->v8ScriptStreamingMode()));

    // Decide what kind of cached data we should produce while streaming. By
    // default, we generate the parser cache for streamed scripts, to emulate
    // the non-streaming behavior (see V8ScriptRunner::compileScript).
    v8::ScriptCompiler::CompileOptions compileOption = v8::ScriptCompiler::kProduceParserCache;
    if (settings->v8CacheOptions() == V8CacheOptionsCode)
        compileOption = v8::ScriptCompiler::kProduceCodeCache;
    v8::ScriptCompiler::ScriptStreamingTask* scriptStreamingTask = v8::ScriptCompiler::StartStreamingScript(scriptState->isolate(), &(streamer->m_source), compileOption);
    if (scriptStreamingTask) {
        streamer->m_task = adoptPtr(scriptStreamingTask);
        script.setStreamer(streamer.release());
        return true;
    }
    // Otherwise, V8 cannot stream the script.
    return false;
}

} // namespace blink
