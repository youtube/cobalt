package com.google.android.apps.tv.tcas.awareness;

import com.google.android.apps.tv.tcas.awareness.IContextEventListener;

interface ITvContextAwarenessService {
    void registerContextEventListener(in IContextEventListener listener, int eventType, int eventSubtype);
    void unregisterContextEventListener(in IContextEventListener listener, int eventType, int eventSubtype);
}
