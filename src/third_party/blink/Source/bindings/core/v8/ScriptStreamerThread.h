// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptStreamerThread_h
#define ScriptStreamerThread_h

#include "platform/TaskSynchronizer.h"
#include "public/platform/WebThread.h"
#include "wtf/OwnPtr.h"

#include <v8.h>

namespace blink {

class ScriptStreamer;

// A singleton thread for running background tasks for script streaming.
class ScriptStreamerThread {
    WTF_MAKE_NONCOPYABLE(ScriptStreamerThread);
public:
    static void init();
    static void shutdown();
    static ScriptStreamerThread* shared();

    void postTask(WebThread::Task*);

    bool isRunningTask() const
    {
        MutexLocker locker(m_mutex);
        return m_runningTask;
    }

    void taskDone();

private:
    ScriptStreamerThread()
        : m_runningTask(false) { }

    bool isRunning() const
    {
        return !!m_thread;
    }

    blink::WebThread& platformThread();

    // At the moment, we only use one thread, so we can only stream one script
    // at a time. FIXME: Use a thread pool and stream multiple scripts.
    WTF::OwnPtr<blink::WebThread> m_thread;
    bool m_runningTask;
    mutable Mutex m_mutex; // Guards m_runningTask.
};

class ScriptStreamingTask : public WebThread::Task {
    WTF_MAKE_NONCOPYABLE(ScriptStreamingTask);
public:
    ScriptStreamingTask(WTF::PassOwnPtr<v8::ScriptCompiler::ScriptStreamingTask>, ScriptStreamer*);
    virtual void run() override;

private:
    WTF::OwnPtr<v8::ScriptCompiler::ScriptStreamingTask> m_v8Task;
    ScriptStreamer* m_streamer;
};


} // namespace blink

#endif // ScriptStreamerThread_h
