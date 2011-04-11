// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_CONFIG_SERVICE_LINUX_H_
#define NET_PROXY_PROXY_CONFIG_SERVICE_LINUX_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/environment.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/observer_list.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_server.h"

namespace net {

// Implementation of ProxyConfigService that retrieves the system proxy
// settings from environment variables or gconf.
class ProxyConfigServiceLinux : public ProxyConfigService {
 public:

  // Forward declaration of Delegate.
  class Delegate;

  class GConfSettingGetter {
   public:
    // Buffer size used in some implementations of this class when reading
    // files. Defined here so unit tests can construct worst-case inputs.
    static const size_t BUFFER_SIZE = 512;

    GConfSettingGetter() {}
    virtual ~GConfSettingGetter() {}

    // Initializes the class: obtains a gconf client, or simulates one, in
    // the concrete implementations. Returns true on success. Must be called
    // before using other methods, and should be called on the thread running
    // the glib main loop.
    // One of |glib_default_loop| and |file_loop| will be used for gconf calls
    // or reading necessary files, depending on the implementation.
    virtual bool Init(MessageLoop* glib_default_loop,
                      MessageLoopForIO* file_loop) = 0;

    // Releases the gconf client, which clears cached directories and
    // stops notifications.
    virtual void Shutdown() = 0;

    // Requests notification of gconf setting changes for proxy
    // settings. Returns true on success.
    virtual bool SetupNotification(Delegate* delegate) = 0;

    // Returns the message loop for the thread on which this object
    // handles notifications, and also on which it must be destroyed.
    // Returns NULL if it does not matter.
    virtual MessageLoop* GetNotificationLoop() = 0;

    // Returns the data source's name (e.g. "gconf", "KDE", "test").
    // Used only for diagnostic purposes (e.g. VLOG(1) etc.).
    virtual const char* GetDataSource() = 0;

    // Gets a string type value from gconf and stores it in
    // result. Returns false if the key is unset or on error. Must
    // only be called after a successful call to Init(), and not
    // after a failed call to SetupNotification() or after calling
    // Release().
    virtual bool GetString(const char* key, std::string* result) = 0;
    // Same thing for a bool typed value.
    virtual bool GetBoolean(const char* key, bool* result) = 0;
    // Same for an int typed value.
    virtual bool GetInt(const char* key, int* result) = 0;
    // And for a string list.
    virtual bool GetStringList(const char* key,
                               std::vector<std::string>* result) = 0;

    // Returns true if the bypass list should be interpreted as a proxy
    // whitelist rather than blacklist. (This is KDE-specific.)
    virtual bool BypassListIsReversed() = 0;

    // Returns true if the bypass rules should be interpreted as
    // suffix-matching rules.
    virtual bool MatchHostsUsingSuffixMatching() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(GConfSettingGetter);
  };

  // ProxyConfigServiceLinux is created on the UI thread, and
  // SetupAndFetchInitialConfig() is immediately called to
  // synchronously fetch the original configuration and set up gconf
  // notifications on the UI thread.
  //
  // Past that point, it is accessed periodically through the
  // ProxyConfigService interface (GetLatestProxyConfig, AddObserver,
  // RemoveObserver) from the IO thread.
  //
  // gconf change notification callbacks can occur at any time and are
  // run on the UI thread. The new gconf settings are fetched on the
  // UI thread, and the new resulting proxy config is posted to the IO
  // thread through Delegate::SetNewProxyConfig(). We then notify
  // observers on the IO thread of the configuration change.
  //
  // ProxyConfigServiceLinux is deleted from the IO thread.
  //
  // The substance of the ProxyConfigServiceLinux implementation is
  // wrapped in the Delegate ref counted class. On deleting the
  // ProxyConfigServiceLinux, Delegate::OnDestroy() is posted to the
  // UI thread where gconf notifications will be safely stopped before
  // releasing Delegate.

  class Delegate : public base::RefCountedThreadSafe<Delegate> {
   public:
    // Constructor receives env var getter implementation to use, and
    // takes ownership of it. This is the normal constructor.
    explicit Delegate(base::Environment* env_var_getter);
    // Constructor receives gconf and env var getter implementations
    // to use, and takes ownership of them. Used for testing.
    Delegate(base::Environment* env_var_getter,
             GConfSettingGetter* gconf_getter);
    // Synchronously obtains the proxy configuration. If gconf is
    // used, also enables gconf notification for setting
    // changes. gconf must only be accessed from the thread running
    // the default glib main loop, and so this method must be called
    // from the UI thread. The message loop for the IO thread is
    // specified so that notifications can post tasks to it (and for
    // assertions). The message loop for the file thread is used to
    // read any files needed to determine proxy settings.
    void SetupAndFetchInitialConfig(MessageLoop* glib_default_loop,
                                    MessageLoop* io_loop,
                                    MessageLoopForIO* file_loop);

    // Handler for gconf change notifications: fetches a new proxy
    // configuration from gconf settings, and if this config is
    // different than what we had before, posts a task to have it
    // stored in cached_config_.
    // Left public for simplicity.
    void OnCheckProxyConfigSettings();

    // Called from IO thread.
    void AddObserver(Observer* observer);
    void RemoveObserver(Observer* observer);
    ProxyConfigService::ConfigAvailability GetLatestProxyConfig(
        ProxyConfig* config);

    // Posts a call to OnDestroy() to the UI thread. Called from
    // ProxyConfigServiceLinux's destructor.
    void PostDestroyTask();
    // Safely stops gconf notifications. Posted to the UI thread.
    void OnDestroy();

   private:
    friend class base::RefCountedThreadSafe<Delegate>;

    ~Delegate();

    // Obtains an environment variable's value. Parses a proxy server
    // specification from it and puts it in result. Returns true if the
    // requested variable is defined and the value valid.
    bool GetProxyFromEnvVarForScheme(const char* variable,
                                     ProxyServer::Scheme scheme,
                                     ProxyServer* result_server);
    // As above but with scheme set to HTTP, for convenience.
    bool GetProxyFromEnvVar(const char* variable, ProxyServer* result_server);
    // Fills proxy config from environment variables. Returns true if
    // variables were found and the configuration is valid.
    bool GetConfigFromEnv(ProxyConfig* config);

    // Obtains host and port gconf settings and parses a proxy server
    // specification from it and puts it in result. Returns true if the
    // requested variable is defined and the value valid.
    bool GetProxyFromGConf(const char* key_prefix, bool is_socks,
                           ProxyServer* result_server);
    // Fills proxy config from gconf. Returns true if settings were found
    // and the configuration is valid.
    bool GetConfigFromGConf(ProxyConfig* config);

    // This method is posted from the UI thread to the IO thread to
    // carry the new config information.
    void SetNewProxyConfig(const ProxyConfig& new_config);

    scoped_ptr<base::Environment> env_var_getter_;
    scoped_ptr<GConfSettingGetter> gconf_getter_;

    // Cached proxy configuration, to be returned by
    // GetLatestProxyConfig. Initially populated from the UI thread, but
    // afterwards only accessed from the IO thread.
    ProxyConfig cached_config_;

    // A copy kept on the UI thread of the last seen proxy config, so as
    // to avoid posting a call to SetNewProxyConfig when we get a
    // notification but the config has not actually changed.
    ProxyConfig reference_config_;

    // The MessageLoop for the UI thread, aka main browser thread. This
    // thread is where we run the glib main loop (see
    // base/message_pump_glib.h). It is the glib default loop in the
    // sense that it runs the glib default context: as in the context
    // where sources are added by g_timeout_add and g_idle_add, and
    // returned by g_main_context_default. gconf uses glib timeouts and
    // idles and possibly other callbacks that will all be dispatched on
    // this thread. Since gconf is not thread safe, any use of gconf
    // must be done on the thread running this loop.
    MessageLoop* glib_default_loop_;
    // MessageLoop for the IO thread. GetLatestProxyConfig() is called from
    // the thread running this loop.
    MessageLoop* io_loop_;

    ObserverList<Observer> observers_;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Thin wrapper shell around Delegate.

  // Usual constructor
  ProxyConfigServiceLinux();
  // For testing: take alternate gconf and env var getter implementations.
  explicit ProxyConfigServiceLinux(base::Environment* env_var_getter);
  ProxyConfigServiceLinux(base::Environment* env_var_getter,
                          GConfSettingGetter* gconf_getter);

  virtual ~ProxyConfigServiceLinux();

  void SetupAndFetchInitialConfig(MessageLoop* glib_default_loop,
                                  MessageLoop* io_loop,
                                  MessageLoopForIO* file_loop) {
    delegate_->SetupAndFetchInitialConfig(glib_default_loop, io_loop,
                                          file_loop);
  }
  void OnCheckProxyConfigSettings() {
    delegate_->OnCheckProxyConfigSettings();
  }

  // ProxyConfigService methods:
  // Called from IO thread.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);
  virtual ProxyConfigService::ConfigAvailability GetLatestProxyConfig(
      ProxyConfig* config);

 private:
  scoped_refptr<Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigServiceLinux);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_CONFIG_SERVICE_LINUX_H_
