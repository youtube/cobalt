// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_EXTENSIONS_ARC_SUPPORT_MESSAGE_HOST_H_
#define CHROME_BROWSER_ASH_ARC_EXTENSIONS_ARC_SUPPORT_MESSAGE_HOST_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/messaging/native_message_host.h"

namespace arc {

// Implements message routing to communicate with arc_support Chrome App.
// Provides JSON (base::Value) APIs for the communication.
class ArcSupportMessageHost : public extensions::NativeMessageHost {
 public:
  class Observer {
   public:
    // Called when an message is sent from arc_support Chrome App.
    virtual void OnMessage(const base::Value::Dict& message) = 0;

   protected:
    virtual ~Observer() = default;
  };

  ArcSupportMessageHost(const ArcSupportMessageHost&) = delete;
  ArcSupportMessageHost& operator=(const ArcSupportMessageHost&) = delete;

  static const char kHostName[];
  static const char* const kHostOrigin[];

  // Called when the arc_support connects the "port". Returns the
  // instance of ArcSupportMessageHost.
  static std::unique_ptr<NativeMessageHost> Create(
      content::BrowserContext* browser_context);

  ~ArcSupportMessageHost() override;

  // Sends a message to arc_support. If the client is not yet ready, the
  // message will be just ignored.
  void SendMessage(const base::ValueView& message);

  // Registers (or unregisters if nullptr) the observer. Currently this class
  // assumes that it has only one observer.
  void SetObserver(Observer* observer);

 private:
  // Keep private. The instance will be created via Create().
  ArcSupportMessageHost();

  // extensions::NativeMessageHost overrides:
  void Start(Client* client) override;
  void OnMessage(const std::string& request_string) override;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override;

  raw_ptr<Observer, ExperimentalAsh> observer_ = nullptr;
  raw_ptr<Client, ExperimentalAsh> client_ = nullptr;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_EXTENSIONS_ARC_SUPPORT_MESSAGE_HOST_H_
