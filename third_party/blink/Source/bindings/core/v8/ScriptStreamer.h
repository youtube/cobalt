// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptStreamer_h
#define ScriptStreamer_h

#include "bindings/core/v8/ScriptStreamingMode.h"
#include "core/dom/PendingScript.h"
#include "wtf/RefCounted.h"

#include <v8.h>

namespace blink {

class PendingScript;
class Resource;
class ScriptResource;
class ScriptResourceClient;
class ScriptState;
class Settings;
class SourceStream;

// ScriptStreamer streams incomplete script data to V8 so that it can be parsed
// while it's loaded. PendingScript holds a reference to ScriptStreamer. At the
// moment, ScriptStreamer is only used for parser blocking scripts; this means
// that the Document stays stable and no other scripts are executing while we're
// streaming. It is possible, though, that Document and the PendingScript are
// destroyed while the streaming is in progress, and ScriptStreamer handles it
// gracefully.
class ScriptStreamer : public RefCounted<ScriptStreamer> {
    WTF_MAKE_NONCOPYABLE(ScriptStreamer);
public:
    // Launches a task (on a background thread) which will stream the given
    // PendingScript into V8 as it loads. It's also possible that V8 cannot
    // stream the given script; in that case this function returns
    // false. Internally, this constructs a ScriptStreamer and attaches it to
    // the PendingScript. Use ScriptStreamer::addClient to get notified when the
    // streaming finishes.
    static void startStreaming(PendingScript&, Settings*, ScriptState*, PendingScript::Type);

    bool isFinished() const
    {
        return m_loadingFinished && (m_parsingFinished || m_streamingSuppressed);
    }

    v8::ScriptCompiler::StreamedSource* source() { return &m_source; }
    ScriptResource* resource() const { return m_resource; }

    // Called when the script is not needed any more (e.g., loading was
    // cancelled). After calling cancel, PendingScript can drop its reference to
    // ScriptStreamer, and ScriptStreamer takes care of eventually deleting
    // itself (after the V8 side has finished too).
    void cancel();

    // When the streaming is suppressed, the data is not given to V8, but
    // ScriptStreamer still watches the resource load and notifies the upper
    // layers when loading is finished. It is used in situations when we have
    // started streaming but then we detect we don't want to stream (e.g., when
    // we have the code cache for the script) and we still want to parse and
    // execute it when it has finished loading.
    void suppressStreaming();
    bool streamingSuppressed() const { return m_streamingSuppressed; }

    unsigned cachedDataType() const { return m_cachedDataType; }

    void addClient(ScriptResourceClient* client)
    {
        ASSERT(!m_client);
        m_client = client;
        notifyFinishedToClient();
    }

    void removeClient(ScriptResourceClient* client)
    {
        ASSERT(m_client == client);
        m_client = 0;
    }

    // Called by PendingScript when data arrives from the network.
    void notifyAppendData(ScriptResource*);
    void notifyFinished(Resource*);

    // Called by ScriptStreamingTask when it has streamed all data to V8 and V8
    // has processed it.
    void streamingCompleteOnBackgroundThread();

    static void setSmallScriptThresholdForTesting(size_t threshold)
    {
        kSmallScriptThreshold = threshold;
    }

    static size_t smallScriptThreshold() { return kSmallScriptThreshold; }

private:
    // Scripts whose first data chunk is smaller than this constant won't be
    // streamed. Non-const for testing.
    static size_t kSmallScriptThreshold;

    ScriptStreamer(ScriptResource*, v8::ScriptCompiler::StreamedSource::Encoding, PendingScript::Type, ScriptStreamingMode);

    void streamingComplete();
    void notifyFinishedToClient();

    bool shouldBlockMainThread() const
    {
        return m_scriptStreamingMode == ScriptStreamingModeAllPlusBlockParsingBlocking && m_scriptType == PendingScript::ParsingBlocking;
    }

    static const char* startedStreamingHistogramName(PendingScript::Type);

    static bool startStreamingInternal(PendingScript&, Settings*, ScriptState*, PendingScript::Type);

    // This pointer is weak. If PendingScript and its Resource are deleted
    // before ScriptStreamer, PendingScript will notify ScriptStreamer of its
    // deletion by calling cancel().
    ScriptResource* m_resource;
    // Whether ScriptStreamer is detached from the Resource. In those cases, the
    // script data is not needed any more, and the client won't get notified
    // when the loading and streaming are done.
    bool m_detached;

    SourceStream* m_stream;
    v8::ScriptCompiler::StreamedSource m_source;
    ScriptResourceClient* m_client;
    WTF::OwnPtr<v8::ScriptCompiler::ScriptStreamingTask> m_task;
    bool m_loadingFinished; // Whether loading from the network is done.
    // Whether the V8 side processing is done. Will be used by the main thread
    // and the streamer thread; guarded by m_mutex.
    bool m_parsingFinished;
    // Whether we have received enough data to start the streaming.
    bool m_haveEnoughDataForStreaming;

    // Whether the script source code should be retrieved from the Resource
    // instead of the ScriptStreamer; guarded by m_mutex.
    bool m_streamingSuppressed;

    // What kind of cached data V8 produces during streaming.
    unsigned m_cachedDataType;

    // For recording metrics for different types of scripts separately.
    PendingScript::Type m_scriptType;

    // Streaming mode defines whether the main thread should block and wait for
    // the parsing to complete after the load has finished. See
    // ScriptStreamer::notifyFinished for more information.
    ScriptStreamingMode m_scriptStreamingMode;
    Mutex m_mutex;
    ThreadCondition m_parsingFinishedCondition;
    // Whether the main thread is currently waiting on the parser thread in
    // notifyFinished(). This also defines which thread should do the cleanup of
    // the parsing task: if the main thread is waiting, the main thread should
    // do it, otherwise the parser thread should do it. Guarded by m_mutex.
    bool m_mainThreadWaitingForParserThread;
};

} // namespace blink

#endif // ScriptStreamer_h
