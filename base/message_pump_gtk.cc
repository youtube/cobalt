// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_gtk.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

namespace base {

MessagePumpGtk::MessagePumpGtk() : MessagePumpGlib() {
  gdk_event_handler_set(&EventDispatcher, this, NULL);
}

MessagePumpGtk::~MessagePumpGtk() {
  gdk_event_handler_set(reinterpret_cast<GdkEventFunc>(gtk_main_do_event),
                        this, NULL);
}

void MessagePumpGtk::DispatchEvents(GdkEvent* event) {
  WillProcessEvent(event);

  MessagePumpDispatcher* dispatcher = GetDispatcher();
  if (!dispatcher)
    gtk_main_do_event(event);
  else if (!dispatcher->Dispatch(event))
    Quit();

  DidProcessEvent(event);
}

// static
Display* MessagePumpGtk::GetDefaultXDisplay() {
  static GdkDisplay* display = gdk_display_get_default();
  return display ? GDK_DISPLAY_XDISPLAY(display) : NULL;
}

bool MessagePumpGtk::RunOnce(GMainContext* context, bool block) {
  // g_main_context_iteration returns true if events have been dispatched.
  return g_main_context_iteration(context, block);
}

void MessagePumpGtk::WillProcessEvent(GdkEvent* event) {
  FOR_EACH_OBSERVER(MessagePumpObserver, observers(), WillProcessEvent(event));
}

void MessagePumpGtk::DidProcessEvent(GdkEvent* event) {
  FOR_EACH_OBSERVER(MessagePumpObserver, observers(), DidProcessEvent(event));
}

// static
void MessagePumpGtk::EventDispatcher(GdkEvent* event, gpointer data) {
  MessagePumpGtk* message_pump = reinterpret_cast<MessagePumpGtk*>(data);
  message_pump->DispatchEvents(event);
}

}  // namespace base
