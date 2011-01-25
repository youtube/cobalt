// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_config_service_linux.h"

#include <errno.h>
#include <fcntl.h>
#if defined(USE_GCONF)
#include <gconf/gconf-client.h>
#endif
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <map>

#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer.h"
#include "base/nix/xdg_util.h"
#include "googleurl/src/url_canon.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_server.h"

namespace net {

namespace {

// Given a proxy hostname from a setting, returns that hostname with
// an appropriate proxy server scheme prefix.
// scheme indicates the desired proxy scheme: usually http, with
// socks 4 or 5 as special cases.
// TODO(arindam): Remove URI string manipulation by using MapUrlSchemeToProxy.
std::string FixupProxyHostScheme(ProxyServer::Scheme scheme,
                                 std::string host) {
  if (scheme == ProxyServer::SCHEME_SOCKS5 &&
      StartsWithASCII(host, "socks4://", false)) {
    // We default to socks 5, but if the user specifically set it to
    // socks4://, then use that.
    scheme = ProxyServer::SCHEME_SOCKS4;
  }
  // Strip the scheme if any.
  std::string::size_type colon = host.find("://");
  if (colon != std::string::npos)
    host = host.substr(colon + 3);
  // If a username and perhaps password are specified, give a warning.
  std::string::size_type at_sign = host.find("@");
  // Should this be supported?
  if (at_sign != std::string::npos) {
    // ProxyConfig does not support authentication parameters, but Chrome
    // will prompt for the password later. Disregard the
    // authentication parameters and continue with this hostname.
    LOG(WARNING) << "Proxy authentication parameters ignored, see bug 16709";
    host = host.substr(at_sign + 1);
  }
  // If this is a socks proxy, prepend a scheme so as to tell
  // ProxyServer. This also allows ProxyServer to choose the right
  // default port.
  if (scheme == ProxyServer::SCHEME_SOCKS4)
    host = "socks4://" + host;
  else if (scheme == ProxyServer::SCHEME_SOCKS5)
    host = "socks5://" + host;
  // If there is a trailing slash, remove it so |host| will parse correctly
  // even if it includes a port number (since the slash is not numeric).
  if (host.length() && host[host.length() - 1] == '/')
    host.resize(host.length() - 1);
  return host;
}

}  // namespace

ProxyConfigServiceLinux::Delegate::~Delegate() {
}

bool ProxyConfigServiceLinux::Delegate::GetProxyFromEnvVarForScheme(
    const char* variable, ProxyServer::Scheme scheme,
    ProxyServer* result_server) {
  std::string env_value;
  if (env_var_getter_->GetVar(variable, &env_value)) {
    if (!env_value.empty()) {
      env_value = FixupProxyHostScheme(scheme, env_value);
      ProxyServer proxy_server =
          ProxyServer::FromURI(env_value, ProxyServer::SCHEME_HTTP);
      if (proxy_server.is_valid() && !proxy_server.is_direct()) {
        *result_server = proxy_server;
        return true;
      } else {
        LOG(ERROR) << "Failed to parse environment variable " << variable;
      }
    }
  }
  return false;
}

bool ProxyConfigServiceLinux::Delegate::GetProxyFromEnvVar(
    const char* variable, ProxyServer* result_server) {
  return GetProxyFromEnvVarForScheme(variable, ProxyServer::SCHEME_HTTP,
                                     result_server);
}

bool ProxyConfigServiceLinux::Delegate::GetConfigFromEnv(ProxyConfig* config) {
  // Check for automatic configuration first, in
  // "auto_proxy". Possibly only the "environment_proxy" firefox
  // extension has ever used this, but it still sounds like a good
  // idea.
  std::string auto_proxy;
  if (env_var_getter_->GetVar("auto_proxy", &auto_proxy)) {
    if (auto_proxy.empty()) {
      // Defined and empty => autodetect
      config->set_auto_detect(true);
    } else {
      // specified autoconfig URL
      config->set_pac_url(GURL(auto_proxy));
    }
    return true;
  }
  // "all_proxy" is a shortcut to avoid defining {http,https,ftp}_proxy.
  ProxyServer proxy_server;
  if (GetProxyFromEnvVar("all_proxy", &proxy_server)) {
    config->proxy_rules().type = ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY;
    config->proxy_rules().single_proxy = proxy_server;
  } else {
    bool have_http = GetProxyFromEnvVar("http_proxy", &proxy_server);
    if (have_http)
      config->proxy_rules().proxy_for_http = proxy_server;
    // It would be tempting to let http_proxy apply for all protocols
    // if https_proxy and ftp_proxy are not defined. Googling turns up
    // several documents that mention only http_proxy. But then the
    // user really might not want to proxy https. And it doesn't seem
    // like other apps do this. So we will refrain.
    bool have_https = GetProxyFromEnvVar("https_proxy", &proxy_server);
    if (have_https)
      config->proxy_rules().proxy_for_https = proxy_server;
    bool have_ftp = GetProxyFromEnvVar("ftp_proxy", &proxy_server);
    if (have_ftp)
      config->proxy_rules().proxy_for_ftp = proxy_server;
    if (have_http || have_https || have_ftp) {
      // mustn't change type unless some rules are actually set.
      config->proxy_rules().type =
          ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
    }
  }
  if (config->proxy_rules().empty()) {
    // If the above were not defined, try for socks.
    // For environment variables, we default to version 5, per the gnome
    // documentation: http://library.gnome.org/devel/gnet/stable/gnet-socks.html
    ProxyServer::Scheme scheme = ProxyServer::SCHEME_SOCKS5;
    std::string env_version;
    if (env_var_getter_->GetVar("SOCKS_VERSION", &env_version)
        && env_version == "4")
      scheme = ProxyServer::SCHEME_SOCKS4;
    if (GetProxyFromEnvVarForScheme("SOCKS_SERVER", scheme, &proxy_server)) {
      config->proxy_rules().type = ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY;
      config->proxy_rules().single_proxy = proxy_server;
    }
  }
  // Look for the proxy bypass list.
  std::string no_proxy;
  env_var_getter_->GetVar("no_proxy", &no_proxy);
  if (config->proxy_rules().empty()) {
    // Having only "no_proxy" set, presumably to "*", makes it
    // explicit that env vars do specify a configuration: having no
    // rules specified only means the user explicitly asks for direct
    // connections.
    return !no_proxy.empty();
  }
  // Note that this uses "suffix" matching. So a bypass of "google.com"
  // is understood to mean a bypass of "*google.com".
  config->proxy_rules().bypass_rules.ParseFromStringUsingSuffixMatching(
      no_proxy);
  return true;
}

namespace {

const int kDebounceTimeoutMilliseconds = 250;

#if defined(USE_GCONF)
// This is the "real" gconf version that actually uses gconf.
class GConfSettingGetterImplGConf
    : public ProxyConfigServiceLinux::GConfSettingGetter {
 public:
  GConfSettingGetterImplGConf()
      : client_(NULL), notify_delegate_(NULL), loop_(NULL) {}

  virtual ~GConfSettingGetterImplGConf() {
    // client_ should have been released before now, from
    // Delegate::OnDestroy(), while running on the UI thread. However
    // on exiting the process, it may happen that
    // Delegate::OnDestroy() task is left pending on the glib loop
    // after the loop was quit, and pending tasks may then be deleted
    // without being run.
    if (client_) {
      // gconf client was not cleaned up.
      if (MessageLoop::current() == loop_) {
        // We are on the UI thread so we can clean it safely. This is
        // the case at least for ui_tests running under Valgrind in
        // bug 16076.
        VLOG(1) << "~GConfSettingGetterImplGConf: releasing gconf client";
        Shutdown();
      } else {
        LOG(WARNING) << "~GConfSettingGetterImplGConf: leaking gconf client";
        client_ = NULL;
      }
    }
    DCHECK(!client_);
  }

  virtual bool Init(MessageLoop* glib_default_loop,
                    MessageLoopForIO* file_loop) {
    DCHECK(MessageLoop::current() == glib_default_loop);
    DCHECK(!client_);
    DCHECK(!loop_);
    loop_ = glib_default_loop;
    client_ = gconf_client_get_default();
    if (!client_) {
      // It's not clear whether/when this can return NULL.
      LOG(ERROR) << "Unable to create a gconf client";
      loop_ = NULL;
      return false;
    }
    GError* error = NULL;
    // We need to add the directories for which we'll be asking
    // notifications, and we might as well ask to preload them.
    gconf_client_add_dir(client_, "/system/proxy",
                         GCONF_CLIENT_PRELOAD_ONELEVEL, &error);
    if (error == NULL) {
      gconf_client_add_dir(client_, "/system/http_proxy",
                           GCONF_CLIENT_PRELOAD_ONELEVEL, &error);
    }
    if (error != NULL) {
      LOG(ERROR) << "Error requesting gconf directory: " << error->message;
      g_error_free(error);
      Shutdown();
      return false;
    }
    return true;
  }

  void Shutdown() {
    if (client_) {
      DCHECK(MessageLoop::current() == loop_);
      // This also disables gconf notifications.
      g_object_unref(client_);
      client_ = NULL;
      loop_ = NULL;
    }
  }

  bool SetupNotification(ProxyConfigServiceLinux::Delegate* delegate) {
    DCHECK(client_);
    DCHECK(MessageLoop::current() == loop_);
    GError* error = NULL;
    notify_delegate_ = delegate;
    gconf_client_notify_add(
        client_, "/system/proxy",
        OnGConfChangeNotification, this,
        NULL, &error);
    if (error == NULL) {
      gconf_client_notify_add(
          client_, "/system/http_proxy",
          OnGConfChangeNotification, this,
          NULL, &error);
    }
    if (error != NULL) {
      LOG(ERROR) << "Error requesting gconf notifications: " << error->message;
      g_error_free(error);
      Shutdown();
      return false;
    }
    return true;
  }

  virtual MessageLoop* GetNotificationLoop() {
    return loop_;
  }

  virtual const char* GetDataSource() {
    return "gconf";
  }

  virtual bool GetString(const char* key, std::string* result) {
    DCHECK(client_);
    DCHECK(MessageLoop::current() == loop_);
    GError* error = NULL;
    gchar* value = gconf_client_get_string(client_, key, &error);
    if (HandleGError(error, key))
      return false;
    if (!value)
      return false;
    *result = value;
    g_free(value);
    return true;
  }
  virtual bool GetBoolean(const char* key, bool* result) {
    DCHECK(client_);
    DCHECK(MessageLoop::current() == loop_);
    GError* error = NULL;
    // We want to distinguish unset values from values defaulting to
    // false. For that we need to use the type-generic
    // gconf_client_get() rather than gconf_client_get_bool().
    GConfValue* gconf_value = gconf_client_get(client_, key, &error);
    if (HandleGError(error, key))
      return false;
    if (!gconf_value) {
      // Unset.
      return false;
    }
    if (gconf_value->type != GCONF_VALUE_BOOL) {
      gconf_value_free(gconf_value);
      return false;
    }
    gboolean bool_value = gconf_value_get_bool(gconf_value);
    *result = static_cast<bool>(bool_value);
    gconf_value_free(gconf_value);
    return true;
  }
  virtual bool GetInt(const char* key, int* result) {
    DCHECK(client_);
    DCHECK(MessageLoop::current() == loop_);
    GError* error = NULL;
    int value = gconf_client_get_int(client_, key, &error);
    if (HandleGError(error, key))
      return false;
    // We don't bother to distinguish an unset value because callers
    // don't care. 0 is returned if unset.
    *result = value;
    return true;
  }
  virtual bool GetStringList(const char* key,
                             std::vector<std::string>* result) {
    DCHECK(client_);
    DCHECK(MessageLoop::current() == loop_);
    GError* error = NULL;
    GSList* list = gconf_client_get_list(client_, key,
                                         GCONF_VALUE_STRING, &error);
    if (HandleGError(error, key))
      return false;
    if (!list) {
      // unset
      return false;
    }
    for (GSList *it = list; it; it = it->next) {
      result->push_back(static_cast<char*>(it->data));
      g_free(it->data);
    }
    g_slist_free(list);
    return true;
  }

  virtual bool BypassListIsReversed() {
    // This is a KDE-specific setting.
    return false;
  }

  virtual bool MatchHostsUsingSuffixMatching() {
    return false;
  }

 private:
  // Logs and frees a glib error. Returns false if there was no error
  // (error is NULL).
  bool HandleGError(GError* error, const char* key) {
    if (error != NULL) {
      LOG(ERROR) << "Error getting gconf value for " << key
                 << ": " << error->message;
      g_error_free(error);
      return true;
    }
    return false;
  }

  // This is the callback from the debounce timer.
  void OnDebouncedNotification() {
    DCHECK(MessageLoop::current() == loop_);
    DCHECK(notify_delegate_);
    // Forward to a method on the proxy config service delegate object.
    notify_delegate_->OnCheckProxyConfigSettings();
  }

  void OnChangeNotification() {
    // We don't use Reset() because the timer may not yet be running.
    // (In that case Stop() is a no-op.)
    debounce_timer_.Stop();
    debounce_timer_.Start(base::TimeDelta::FromMilliseconds(
        kDebounceTimeoutMilliseconds), this,
        &GConfSettingGetterImplGConf::OnDebouncedNotification);
  }

  // gconf notification callback, dispatched from the default glib main loop.
  static void OnGConfChangeNotification(
      GConfClient* client, guint cnxn_id,
      GConfEntry* entry, gpointer user_data) {
    VLOG(1) << "gconf change notification for key "
            << gconf_entry_get_key(entry);
    // We don't track which key has changed, just that something did change.
    GConfSettingGetterImplGConf* setting_getter =
        reinterpret_cast<GConfSettingGetterImplGConf*>(user_data);
    setting_getter->OnChangeNotification();
  }

  GConfClient* client_;
  ProxyConfigServiceLinux::Delegate* notify_delegate_;
  base::OneShotTimer<GConfSettingGetterImplGConf> debounce_timer_;

  // Message loop of the thread that we make gconf calls on. It should
  // be the UI thread and all our methods should be called on this
  // thread. Only for assertions.
  MessageLoop* loop_;

  DISALLOW_COPY_AND_ASSIGN(GConfSettingGetterImplGConf);
};
#endif  // defined(USE_GCONF)

// This is the KDE version that reads kioslaverc and simulates gconf.
// Doing this allows the main Delegate code, as well as the unit tests
// for it, to stay the same - and the settings map fairly well besides.
class GConfSettingGetterImplKDE
    : public ProxyConfigServiceLinux::GConfSettingGetter,
      public base::MessagePumpLibevent::Watcher {
 public:
  explicit GConfSettingGetterImplKDE(base::Environment* env_var_getter)
      : inotify_fd_(-1), notify_delegate_(NULL), indirect_manual_(false),
        auto_no_pac_(false), reversed_bypass_list_(false),
        env_var_getter_(env_var_getter), file_loop_(NULL) {
    // This has to be called on the UI thread (http://crbug.com/69057).
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    // Derive the location of the kde config dir from the environment.
    std::string home;
    if (env_var_getter->GetVar("KDEHOME", &home) && !home.empty()) {
      // $KDEHOME is set. Use it unconditionally.
      kde_config_dir_ = KDEHomeToConfigPath(FilePath(home));
    } else {
      // $KDEHOME is unset. Try to figure out what to use. This seems to be
      // the common case on most distributions.
      if (!env_var_getter->GetVar(base::env_vars::kHome, &home))
        // User has no $HOME? Give up. Later we'll report the failure.
        return;
      if (base::nix::GetDesktopEnvironment(env_var_getter) ==
          base::nix::DESKTOP_ENVIRONMENT_KDE3) {
        // KDE3 always uses .kde for its configuration.
        FilePath kde_path = FilePath(home).Append(".kde");
        kde_config_dir_ = KDEHomeToConfigPath(kde_path);
      } else {
        // Some distributions patch KDE4 to use .kde4 instead of .kde, so that
        // both can be installed side-by-side. Sadly they don't all do this, and
        // they don't always do this: some distributions have started switching
        // back as well. So if there is a .kde4 directory, check the timestamps
        // of the config directories within and use the newest one.
        // Note that we should currently be running in the UI thread, because in
        // the gconf version, that is the only thread that can access the proxy
        // settings (a gconf restriction). As noted below, the initial read of
        // the proxy settings will be done in this thread anyway, so we check
        // for .kde4 here in this thread as well.
        FilePath kde3_path = FilePath(home).Append(".kde");
        FilePath kde3_config = KDEHomeToConfigPath(kde3_path);
        FilePath kde4_path = FilePath(home).Append(".kde4");
        FilePath kde4_config = KDEHomeToConfigPath(kde4_path);
        bool use_kde4 = false;
        if (file_util::DirectoryExists(kde4_path)) {
          base::PlatformFileInfo kde3_info;
          base::PlatformFileInfo kde4_info;
          if (file_util::GetFileInfo(kde4_config, &kde4_info)) {
            if (file_util::GetFileInfo(kde3_config, &kde3_info)) {
              use_kde4 = kde4_info.last_modified >= kde3_info.last_modified;
            } else {
              use_kde4 = true;
            }
          }
        }
        if (use_kde4) {
          kde_config_dir_ = KDEHomeToConfigPath(kde4_path);
        } else {
          kde_config_dir_ = KDEHomeToConfigPath(kde3_path);
        }
      }
    }
  }

  virtual ~GConfSettingGetterImplKDE() {
    // inotify_fd_ should have been closed before now, from
    // Delegate::OnDestroy(), while running on the file thread. However
    // on exiting the process, it may happen that Delegate::OnDestroy()
    // task is left pending on the file loop after the loop was quit,
    // and pending tasks may then be deleted without being run.
    // Here in the KDE version, we can safely close the file descriptor
    // anyway. (Not that it really matters; the process is exiting.)
    if (inotify_fd_ >= 0)
      Shutdown();
    DCHECK(inotify_fd_ < 0);
  }

  virtual bool Init(MessageLoop* glib_default_loop,
                    MessageLoopForIO* file_loop) {
    // This has to be called on the UI thread (http://crbug.com/69057).
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    DCHECK(inotify_fd_ < 0);
    inotify_fd_ = inotify_init();
    if (inotify_fd_ < 0) {
      PLOG(ERROR) << "inotify_init failed";
      return false;
    }
    int flags = fcntl(inotify_fd_, F_GETFL);
    if (fcntl(inotify_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
      PLOG(ERROR) << "fcntl failed";
      close(inotify_fd_);
      inotify_fd_ = -1;
      return false;
    }
    file_loop_ = file_loop;
    // The initial read is done on the current thread, not |file_loop_|,
    // since we will need to have it for SetupAndFetchInitialConfig().
    UpdateCachedSettings();
    return true;
  }

  void Shutdown() {
    if (inotify_fd_ >= 0) {
      ResetCachedSettings();
      inotify_watcher_.StopWatchingFileDescriptor();
      close(inotify_fd_);
      inotify_fd_ = -1;
    }
  }

  bool SetupNotification(ProxyConfigServiceLinux::Delegate* delegate) {
    DCHECK(inotify_fd_ >= 0);
    DCHECK(file_loop_);
    // We can't just watch the kioslaverc file directly, since KDE will write
    // a new copy of it and then rename it whenever settings are changed and
    // inotify watches inodes (so we'll be watching the old deleted file after
    // the first change, and it will never change again). So, we watch the
    // directory instead. We then act only on changes to the kioslaverc entry.
    if (inotify_add_watch(inotify_fd_, kde_config_dir_.value().c_str(),
                          IN_MODIFY | IN_MOVED_TO) < 0)
      return false;
    notify_delegate_ = delegate;
    return file_loop_->WatchFileDescriptor(inotify_fd_, true,
        MessageLoopForIO::WATCH_READ, &inotify_watcher_, this);
  }

  virtual MessageLoop* GetNotificationLoop() {
    return file_loop_;
  }

  // Implement base::MessagePumpLibevent::Delegate.
  void OnFileCanReadWithoutBlocking(int fd) {
    DCHECK(fd == inotify_fd_);
    DCHECK(MessageLoop::current() == file_loop_);
    OnChangeNotification();
  }
  void OnFileCanWriteWithoutBlocking(int fd) {
    NOTREACHED();
  }

  virtual const char* GetDataSource() {
    return "KDE";
  }

  virtual bool GetString(const char* key, std::string* result) {
    string_map_type::iterator it = string_table_.find(key);
    if (it == string_table_.end())
      return false;
    *result = it->second;
    return true;
  }
  virtual bool GetBoolean(const char* key, bool* result) {
    // We don't ever have any booleans.
    return false;
  }
  virtual bool GetInt(const char* key, int* result) {
    // We don't ever have any integers. (See AddProxy() below about ports.)
    return false;
  }
  virtual bool GetStringList(const char* key,
                             std::vector<std::string>* result) {
    strings_map_type::iterator it = strings_table_.find(key);
    if (it == strings_table_.end())
      return false;
    *result = it->second;
    return true;
  }

  virtual bool BypassListIsReversed() {
    return reversed_bypass_list_;
  }

  virtual bool MatchHostsUsingSuffixMatching() {
    return true;
  }

 private:
  void ResetCachedSettings() {
    string_table_.clear();
    strings_table_.clear();
    indirect_manual_ = false;
    auto_no_pac_ = false;
    reversed_bypass_list_ = false;
  }

  FilePath KDEHomeToConfigPath(const FilePath& kde_home) {
    return kde_home.Append("share").Append("config");
  }

  void AddProxy(const std::string& prefix, const std::string& value) {
    if (value.empty() || value.substr(0, 3) == "//:")
      // No proxy.
      return;
    // We don't need to parse the port number out; GetProxyFromGConf()
    // would only append it right back again. So we just leave the port
    // number right in the host string.
    string_table_[prefix + "host"] = value;
  }

  void AddHostList(const std::string& key, const std::string& value) {
    std::vector<std::string> tokens;
    StringTokenizer tk(value, ", ");
    while (tk.GetNext()) {
      std::string token = tk.token();
      if (!token.empty())
        tokens.push_back(token);
    }
    strings_table_[key] = tokens;
  }

  void AddKDESetting(const std::string& key, const std::string& value) {
    // The astute reader may notice that there is no mention of SOCKS
    // here. That's because KDE handles socks is a strange way, and we
    // don't support it. Rather than just a setting for the SOCKS server,
    // it has a setting for a library to LD_PRELOAD in all your programs
    // that will transparently SOCKSify them. Such libraries each have
    // their own configuration, and thus, we can't get it from KDE.
    if (key == "ProxyType") {
      const char* mode = "none";
      indirect_manual_ = false;
      auto_no_pac_ = false;
      int int_value;
      base::StringToInt(value, &int_value);
      switch (int_value) {
        case 0:  // No proxy, or maybe kioslaverc syntax error.
          break;
        case 1:  // Manual configuration.
          mode = "manual";
          break;
        case 2:  // PAC URL.
          mode = "auto";
          break;
        case 3:  // WPAD.
          mode = "auto";
          auto_no_pac_ = true;
          break;
        case 4:  // Indirect manual via environment variables.
          mode = "manual";
          indirect_manual_ = true;
          break;
      }
      string_table_["/system/proxy/mode"] = mode;
    } else if (key == "Proxy Config Script") {
      string_table_["/system/proxy/autoconfig_url"] = value;
    } else if (key == "httpProxy") {
      AddProxy("/system/http_proxy/", value);
    } else if (key == "httpsProxy") {
      AddProxy("/system/proxy/secure_", value);
    } else if (key == "ftpProxy") {
      AddProxy("/system/proxy/ftp_", value);
    } else if (key == "ReversedException") {
      // We count "true" or any nonzero number as true, otherwise false.
      // Note that if the value is not actually numeric StringToInt()
      // will return 0, which we count as false.
      int int_value;
      base::StringToInt(value, &int_value);
      reversed_bypass_list_ = (value == "true" || int_value);
    } else if (key == "NoProxyFor") {
      AddHostList("/system/http_proxy/ignore_hosts", value);
    } else if (key == "AuthMode") {
      // Check for authentication, just so we can warn.
      int mode;
      base::StringToInt(value, &mode);
      if (mode) {
        // ProxyConfig does not support authentication parameters, but
        // Chrome will prompt for the password later. So we ignore this.
        LOG(WARNING) <<
            "Proxy authentication parameters ignored, see bug 16709";
      }
    }
  }

  void ResolveIndirect(const std::string& key) {
    string_map_type::iterator it = string_table_.find(key);
    if (it != string_table_.end()) {
      std::string value;
      if (env_var_getter_->GetVar(it->second.c_str(), &value))
        it->second = value;
      else
        string_table_.erase(it);
    }
  }

  void ResolveIndirectList(const std::string& key) {
    strings_map_type::iterator it = strings_table_.find(key);
    if (it != strings_table_.end()) {
      std::string value;
      if (!it->second.empty() &&
          env_var_getter_->GetVar(it->second[0].c_str(), &value))
        AddHostList(key, value);
      else
        strings_table_.erase(it);
    }
  }

  // The settings in kioslaverc could occur in any order, but some affect
  // others. Rather than read the whole file in and then query them in an
  // order that allows us to handle that, we read the settings in whatever
  // order they occur and do any necessary tweaking after we finish.
  void ResolveModeEffects() {
    if (indirect_manual_) {
      ResolveIndirect("/system/http_proxy/host");
      ResolveIndirect("/system/proxy/secure_host");
      ResolveIndirect("/system/proxy/ftp_host");
      ResolveIndirectList("/system/http_proxy/ignore_hosts");
    }
    if (auto_no_pac_) {
      // Remove the PAC URL; we're not supposed to use it.
      string_table_.erase("/system/proxy/autoconfig_url");
    }
  }

  // Reads kioslaverc one line at a time and calls AddKDESetting() to add
  // each relevant name-value pair to the appropriate value table.
  void UpdateCachedSettings() {
    FilePath kioslaverc = kde_config_dir_.Append("kioslaverc");
    file_util::ScopedFILE input(file_util::OpenFile(kioslaverc, "r"));
    if (!input.get())
      return;
    ResetCachedSettings();
    bool in_proxy_settings = false;
    bool line_too_long = false;
    char line[BUFFER_SIZE];
    // fgets() will return NULL on EOF or error.
    while (fgets(line, sizeof(line), input.get())) {
      // fgets() guarantees the line will be properly terminated.
      size_t length = strlen(line);
      if (!length)
        continue;
      // This should be true even with CRLF endings.
      if (line[length - 1] != '\n') {
        line_too_long = true;
        continue;
      }
      if (line_too_long) {
        // The previous line had no line ending, but this done does. This is
        // the end of the line that was too long, so warn here and skip it.
        LOG(WARNING) << "skipped very long line in " << kioslaverc.value();
        line_too_long = false;
        continue;
      }
      // Remove the LF at the end, and the CR if there is one.
      line[--length] = '\0';
      if (length && line[length - 1] == '\r')
        line[--length] = '\0';
      // Now parse the line.
      if (line[0] == '[') {
        // Switching sections. All we care about is whether this is
        // the (a?) proxy settings section, for both KDE3 and KDE4.
        in_proxy_settings = !strncmp(line, "[Proxy Settings]", 16);
      } else if (in_proxy_settings) {
        // A regular line, in the (a?) proxy settings section.
        char* split = strchr(line, '=');
        // Skip this line if it does not contain an = sign.
        if (!split)
          continue;
        // Split the line on the = and advance |split|.
        *(split++) = 0;
        std::string key = line;
        std::string value = split;
        TrimWhitespaceASCII(key, TRIM_ALL, &key);
        TrimWhitespaceASCII(value, TRIM_ALL, &value);
        // Skip this line if the key name is empty.
        if (key.empty())
          continue;
        // Is the value name localized?
        if (key[key.length() - 1] == ']') {
          // Find the matching bracket.
          length = key.rfind('[');
          // Skip this line if the localization indicator is malformed.
          if (length == std::string::npos)
            continue;
          // Trim the localization indicator off.
          key.resize(length);
          // Remove any resulting trailing whitespace.
          TrimWhitespaceASCII(key, TRIM_TRAILING, &key);
          // Skip this line if the key name is now empty.
          if (key.empty())
            continue;
        }
        // Now fill in the tables.
        AddKDESetting(key, value);
      }
    }
    if (ferror(input.get()))
      LOG(ERROR) << "error reading " << kioslaverc.value();
    ResolveModeEffects();
  }

  // This is the callback from the debounce timer.
  void OnDebouncedNotification() {
    DCHECK(MessageLoop::current() == file_loop_);
    VLOG(1) << "inotify change notification for kioslaverc";
    UpdateCachedSettings();
    DCHECK(notify_delegate_);
    // Forward to a method on the proxy config service delegate object.
    notify_delegate_->OnCheckProxyConfigSettings();
  }

  // Called by OnFileCanReadWithoutBlocking() on the file thread. Reads
  // from the inotify file descriptor and starts up a debounce timer if
  // an event for kioslaverc is seen.
  void OnChangeNotification() {
    DCHECK(inotify_fd_ >= 0);
    DCHECK(MessageLoop::current() == file_loop_);
    char event_buf[(sizeof(inotify_event) + NAME_MAX + 1) * 4];
    bool kioslaverc_touched = false;
    ssize_t r;
    while ((r = read(inotify_fd_, event_buf, sizeof(event_buf))) > 0) {
      // inotify returns variable-length structures, which is why we have
      // this strange-looking loop instead of iterating through an array.
      char* event_ptr = event_buf;
      while (event_ptr < event_buf + r) {
        inotify_event* event = reinterpret_cast<inotify_event*>(event_ptr);
        // The kernel always feeds us whole events.
        CHECK_LE(event_ptr + sizeof(inotify_event), event_buf + r);
        CHECK_LE(event->name + event->len, event_buf + r);
        if (!strcmp(event->name, "kioslaverc"))
          kioslaverc_touched = true;
        // Advance the pointer just past the end of the filename.
        event_ptr = event->name + event->len;
      }
      // We keep reading even if |kioslaverc_touched| is true to drain the
      // inotify event queue.
    }
    if (!r)
      // Instead of returning -1 and setting errno to EINVAL if there is not
      // enough buffer space, older kernels (< 2.6.21) return 0. Simulate the
      // new behavior (EINVAL) so we can reuse the code below.
      errno = EINVAL;
    if (errno != EAGAIN) {
      PLOG(WARNING) << "error reading inotify file descriptor";
      if (errno == EINVAL) {
        // Our buffer is not large enough to read the next event. This should
        // not happen (because its size is calculated to always be sufficiently
        // large), but if it does we'd warn continuously since |inotify_fd_|
        // would be forever ready to read. Close it and stop watching instead.
        LOG(ERROR) << "inotify failure; no longer watching kioslaverc!";
        inotify_watcher_.StopWatchingFileDescriptor();
        close(inotify_fd_);
        inotify_fd_ = -1;
      }
    }
    if (kioslaverc_touched) {
      // We don't use Reset() because the timer may not yet be running.
      // (In that case Stop() is a no-op.)
      debounce_timer_.Stop();
      debounce_timer_.Start(base::TimeDelta::FromMilliseconds(
          kDebounceTimeoutMilliseconds), this,
          &GConfSettingGetterImplKDE::OnDebouncedNotification);
    }
  }

  typedef std::map<std::string, std::string> string_map_type;
  typedef std::map<std::string, std::vector<std::string> > strings_map_type;

  int inotify_fd_;
  base::MessagePumpLibevent::FileDescriptorWatcher inotify_watcher_;
  ProxyConfigServiceLinux::Delegate* notify_delegate_;
  base::OneShotTimer<GConfSettingGetterImplKDE> debounce_timer_;
  FilePath kde_config_dir_;
  bool indirect_manual_;
  bool auto_no_pac_;
  bool reversed_bypass_list_;
  // We don't own |env_var_getter_|.  It's safe to hold a pointer to it, since
  // both it and us are owned by ProxyConfigServiceLinux::Delegate, and have the
  // same lifetime.
  base::Environment* env_var_getter_;

  // We cache these settings whenever we re-read the kioslaverc file.
  string_map_type string_table_;
  strings_map_type strings_table_;

  // Message loop of the file thread, for reading kioslaverc. If NULL,
  // just read it directly (for testing). We also handle inotify events
  // on this thread.
  MessageLoopForIO* file_loop_;

  DISALLOW_COPY_AND_ASSIGN(GConfSettingGetterImplKDE);
};

}  // namespace

bool ProxyConfigServiceLinux::Delegate::GetProxyFromGConf(
    const char* key_prefix, bool is_socks, ProxyServer* result_server) {
  std::string key(key_prefix);
  std::string host;
  if (!gconf_getter_->GetString((key + "host").c_str(), &host)
      || host.empty()) {
    // Unset or empty.
    return false;
  }
  // Check for an optional port.
  int port = 0;
  gconf_getter_->GetInt((key + "port").c_str(), &port);
  if (port != 0) {
    // If a port is set and non-zero:
    host += ":" + base::IntToString(port);
  }
  host = FixupProxyHostScheme(
      is_socks ? ProxyServer::SCHEME_SOCKS5 : ProxyServer::SCHEME_HTTP,
      host);
  ProxyServer proxy_server = ProxyServer::FromURI(host,
                                                  ProxyServer::SCHEME_HTTP);
  if (proxy_server.is_valid()) {
    *result_server = proxy_server;
    return true;
  }
  return false;
}

bool ProxyConfigServiceLinux::Delegate::GetConfigFromGConf(
    ProxyConfig* config) {
  std::string mode;
  if (!gconf_getter_->GetString("/system/proxy/mode", &mode)) {
    // We expect this to always be set, so if we don't see it then we
    // probably have a gconf problem, and so we don't have a valid
    // proxy config.
    return false;
  }
  if (mode == "none") {
    // Specifically specifies no proxy.
    return true;
  }

  if (mode == "auto") {
    // automatic proxy config
    std::string pac_url_str;
    if (gconf_getter_->GetString("/system/proxy/autoconfig_url",
                                 &pac_url_str)) {
      if (!pac_url_str.empty()) {
        GURL pac_url(pac_url_str);
        if (!pac_url.is_valid())
          return false;
        config->set_pac_url(pac_url);
        return true;
      }
    }
    config->set_auto_detect(true);
    return true;
  }

  if (mode != "manual") {
    // Mode is unrecognized.
    return false;
  }
  bool use_http_proxy;
  if (gconf_getter_->GetBoolean("/system/http_proxy/use_http_proxy",
                                &use_http_proxy)
      && !use_http_proxy) {
    // Another master switch for some reason. If set to false, then no
    // proxy. But we don't panic if the key doesn't exist.
    return true;
  }

  bool same_proxy = false;
  // Indicates to use the http proxy for all protocols. This one may
  // not exist (presumably on older versions), assume false in that
  // case.
  gconf_getter_->GetBoolean("/system/http_proxy/use_same_proxy",
                            &same_proxy);

  ProxyServer proxy_server;
  if (!same_proxy) {
    // Try socks.
    if (GetProxyFromGConf("/system/proxy/socks_", true, &proxy_server)) {
      // gconf settings do not appear to distinguish between socks
      // version. We default to version 5. For more information on this policy
      // decisions, see:
      // http://code.google.com/p/chromium/issues/detail?id=55912#c2
      config->proxy_rules().type = ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY;
      config->proxy_rules().single_proxy = proxy_server;
    }
  }
  if (config->proxy_rules().empty()) {
    bool have_http = GetProxyFromGConf("/system/http_proxy/", false,
                                       &proxy_server);
    if (same_proxy) {
      if (have_http) {
        config->proxy_rules().type = ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY;
        config->proxy_rules().single_proxy = proxy_server;
      }
    } else {
      // Protocol specific settings.
      if (have_http)
        config->proxy_rules().proxy_for_http = proxy_server;
      bool have_secure = GetProxyFromGConf("/system/proxy/secure_", false,
                                           &proxy_server);
      if (have_secure)
        config->proxy_rules().proxy_for_https = proxy_server;
      bool have_ftp = GetProxyFromGConf("/system/proxy/ftp_", false,
                                        &proxy_server);
      if (have_ftp)
        config->proxy_rules().proxy_for_ftp = proxy_server;
      if (have_http || have_secure || have_ftp)
        config->proxy_rules().type =
            ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
    }
  }

  if (config->proxy_rules().empty()) {
    // Manual mode but we couldn't parse any rules.
    return false;
  }

  // Check for authentication, just so we can warn.
  bool use_auth = false;
  gconf_getter_->GetBoolean("/system/http_proxy/use_authentication",
                            &use_auth);
  if (use_auth) {
    // ProxyConfig does not support authentication parameters, but
    // Chrome will prompt for the password later. So we ignore
    // /system/http_proxy/*auth* settings.
    LOG(WARNING) << "Proxy authentication parameters ignored, see bug 16709";
  }

  // Now the bypass list.
  std::vector<std::string> ignore_hosts_list;
  config->proxy_rules().bypass_rules.Clear();
  if (gconf_getter_->GetStringList("/system/http_proxy/ignore_hosts",
                                   &ignore_hosts_list)) {
    std::vector<std::string>::const_iterator it(ignore_hosts_list.begin());
    for (; it != ignore_hosts_list.end(); ++it) {
      if (gconf_getter_->MatchHostsUsingSuffixMatching()) {
        config->proxy_rules().bypass_rules.
            AddRuleFromStringUsingSuffixMatching(*it);
      } else {
        config->proxy_rules().bypass_rules.AddRuleFromString(*it);
      }
    }
  }
  // Note that there are no settings with semantics corresponding to
  // bypass of local names in GNOME. In KDE, "<local>" is supported
  // as a hostname rule.

  // KDE allows one to reverse the bypass rules.
  config->proxy_rules().reverse_bypass = gconf_getter_->BypassListIsReversed();

  return true;
}

ProxyConfigServiceLinux::Delegate::Delegate(base::Environment* env_var_getter)
    : env_var_getter_(env_var_getter),
      glib_default_loop_(NULL), io_loop_(NULL) {
  // Figure out which GConfSettingGetterImpl to use, if any.
  switch (base::nix::GetDesktopEnvironment(env_var_getter)) {
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
#if defined(USE_GCONF)
      gconf_getter_.reset(new GConfSettingGetterImplGConf());
#endif
      break;
    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
      gconf_getter_.reset(new GConfSettingGetterImplKDE(env_var_getter));
      break;
    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      break;
  }
}

ProxyConfigServiceLinux::Delegate::Delegate(base::Environment* env_var_getter,
    GConfSettingGetter* gconf_getter)
    : env_var_getter_(env_var_getter), gconf_getter_(gconf_getter),
      glib_default_loop_(NULL), io_loop_(NULL) {
}

void ProxyConfigServiceLinux::Delegate::SetupAndFetchInitialConfig(
    MessageLoop* glib_default_loop, MessageLoop* io_loop,
    MessageLoopForIO* file_loop) {
  // We should be running on the default glib main loop thread right
  // now. gconf can only be accessed from this thread.
  DCHECK(MessageLoop::current() == glib_default_loop);
  glib_default_loop_ = glib_default_loop;
  io_loop_ = io_loop;

  // If we are passed a NULL io_loop or file_loop, then we don't set up
  // proxy setting change notifications. This should not be the usual
  // case but is intended to simplify test setups.
  if (!io_loop_ || !file_loop)
    VLOG(1) << "Monitoring of proxy setting changes is disabled";

  // Fetch and cache the current proxy config. The config is left in
  // cached_config_, where GetLatestProxyConfig() running on the IO thread
  // will expect to find it. This is safe to do because we return
  // before this ProxyConfigServiceLinux is passed on to
  // the ProxyService.

  // Note: It would be nice to prioritize environment variables
  // and only fall back to gconf if env vars were unset. But
  // gnome-terminal "helpfully" sets http_proxy and no_proxy, and it
  // does so even if the proxy mode is set to auto, which would
  // mislead us.

  bool got_config = false;
  if (gconf_getter_.get()) {
    if (gconf_getter_->Init(glib_default_loop, file_loop) &&
        (!io_loop || !file_loop || gconf_getter_->SetupNotification(this))) {
      if (GetConfigFromGConf(&cached_config_)) {
        cached_config_.set_id(1);  // mark it as valid
        got_config = true;
        VLOG(1) << "Obtained proxy settings from "
                << gconf_getter_->GetDataSource();
        // If gconf proxy mode is "none", meaning direct, then we take
        // that to be a valid config and will not check environment
        // variables. The alternative would have been to look for a proxy
        // whereever we can find one.
        //
        // Keep a copy of the config for use from this thread for
        // comparison with updated settings when we get notifications.
        reference_config_ = cached_config_;
        reference_config_.set_id(1);  // mark it as valid
      } else {
        gconf_getter_->Shutdown();  // Stop notifications
      }
    }
  }

  if (!got_config) {
    // We fall back on environment variables.
    //
    // Consulting environment variables doesn't need to be done from
    // the default glib main loop, but it's a tiny enough amount of
    // work.
    if (GetConfigFromEnv(&cached_config_)) {
      cached_config_.set_id(1);  // mark it as valid
      VLOG(1) << "Obtained proxy settings from environment variables";
    }
  }
}

void ProxyConfigServiceLinux::Delegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ProxyConfigServiceLinux::Delegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ProxyConfigServiceLinux::Delegate::GetLatestProxyConfig(
    ProxyConfig* config) {
  // This is called from the IO thread.
  DCHECK(!io_loop_ || MessageLoop::current() == io_loop_);

  // Simply return the last proxy configuration that glib_default_loop
  // notified us of.
  *config = cached_config_.is_valid() ?
      cached_config_ : ProxyConfig::CreateDirect();

  // We return true to indicate that *config was filled in. It is always
  // going to be available since we initialized eagerly on the UI thread.
  // TODO(eroman): do lazy initialization instead, so we no longer need
  //               to construct ProxyConfigServiceLinux on the UI thread.
  //               In which case, we may return false here.
  return true;
}

// Depending on the GConfSettingGetter in use, this method will be called
// on either the UI thread (GConf) or the file thread (KDE).
void ProxyConfigServiceLinux::Delegate::OnCheckProxyConfigSettings() {
  MessageLoop* required_loop = gconf_getter_->GetNotificationLoop();
  DCHECK(!required_loop || MessageLoop::current() == required_loop);
  ProxyConfig new_config;
  bool valid = GetConfigFromGConf(&new_config);
  if (valid)
    new_config.set_id(1);  // mark it as valid

  // See if it is different from what we had before.
  if (new_config.is_valid() != reference_config_.is_valid() ||
      !new_config.Equals(reference_config_)) {
    // Post a task to |io_loop| with the new configuration, so it can
    // update |cached_config_|.
    io_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            this,
            &ProxyConfigServiceLinux::Delegate::SetNewProxyConfig,
            new_config));
    // Update the thread-private copy in |reference_config_| as well.
    reference_config_ = new_config;
  }
}

void ProxyConfigServiceLinux::Delegate::SetNewProxyConfig(
    const ProxyConfig& new_config) {
  DCHECK(MessageLoop::current() == io_loop_);
  VLOG(1) << "Proxy configuration changed";
  cached_config_ = new_config;
  FOR_EACH_OBSERVER(Observer, observers_, OnProxyConfigChanged(new_config));
}

void ProxyConfigServiceLinux::Delegate::PostDestroyTask() {
  if (!gconf_getter_.get())
    return;
  MessageLoop* shutdown_loop = gconf_getter_->GetNotificationLoop();
  if (!shutdown_loop || MessageLoop::current() == shutdown_loop) {
    // Already on the right thread, call directly.
    // This is the case for the unittests.
    OnDestroy();
  } else {
    // Post to shutdown thread. Note that on browser shutdown, we may quit
    // this MessageLoop and exit the program before ever running this.
    shutdown_loop->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            this,
            &ProxyConfigServiceLinux::Delegate::OnDestroy));
  }
}
void ProxyConfigServiceLinux::Delegate::OnDestroy() {
  MessageLoop* shutdown_loop = gconf_getter_->GetNotificationLoop();
  DCHECK(!shutdown_loop || MessageLoop::current() == shutdown_loop);
  gconf_getter_->Shutdown();
}

ProxyConfigServiceLinux::ProxyConfigServiceLinux()
    : delegate_(new Delegate(base::Environment::Create())) {
}

ProxyConfigServiceLinux::~ProxyConfigServiceLinux() {
  delegate_->PostDestroyTask();
}

ProxyConfigServiceLinux::ProxyConfigServiceLinux(
    base::Environment* env_var_getter)
    : delegate_(new Delegate(env_var_getter)) {
}

ProxyConfigServiceLinux::ProxyConfigServiceLinux(
    base::Environment* env_var_getter,
    GConfSettingGetter* gconf_getter)
    : delegate_(new Delegate(env_var_getter, gconf_getter)) {
}

void ProxyConfigServiceLinux::AddObserver(Observer* observer) {
  delegate_->AddObserver(observer);
}

void ProxyConfigServiceLinux::RemoveObserver(Observer* observer) {
  delegate_->RemoveObserver(observer);
}

bool ProxyConfigServiceLinux::GetLatestProxyConfig(ProxyConfig* config) {
  return delegate_->GetLatestProxyConfig(config);
}

}  // namespace net
