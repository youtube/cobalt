// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_config_service_android.h"

#include <sys/system_properties.h>

#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "googleurl/src/url_parse.h"
#include "jni/proxy_change_listener_jni.h"
#include "net/base/host_port_pair.h"
#include "net/proxy/proxy_config.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertJavaStringToUTF8;
using base::android::CheckException;
using base::android::ClearException;
using base::android::GetMethodID;
using base::android::ScopedJavaGlobalRef;

namespace net {

namespace {

typedef ProxyConfigServiceAndroid::GetPropertyCallback GetPropertyCallback;

// Returns whether the provided string was successfully converted to a port.
bool ConvertStringToPort(const std::string& port, int* output) {
  url_parse::Component component(0, port.size());
  int result = url_parse::ParsePort(port.c_str(), component);
  if (result == url_parse::PORT_INVALID ||
      result == url_parse::PORT_UNSPECIFIED)
    return false;
  *output = result;
  return true;
}

ProxyServer ConstructProxyServer(ProxyServer::Scheme scheme,
                                 const std::string& proxy_host,
                                 const std::string& proxy_port) {
  DCHECK(!proxy_host.empty());
  int port_as_int = 0;
  if (proxy_port.empty())
    port_as_int = ProxyServer::GetDefaultPortForScheme(scheme);
  else if (!ConvertStringToPort(proxy_port, &port_as_int))
    return ProxyServer();
  DCHECK(port_as_int > 0);
  return ProxyServer(
      scheme,
      HostPortPair(proxy_host, static_cast<uint16>(port_as_int)));
}

ProxyServer LookupProxy(const std::string& prefix,
                        const GetPropertyCallback& get_property,
                        ProxyServer::Scheme scheme) {
  DCHECK(!prefix.empty());
  std::string proxy_host = get_property.Run(prefix + ".proxyHost");
  if (!proxy_host.empty()) {
    std::string proxy_port = get_property.Run(prefix + ".proxyPort");
    return ConstructProxyServer(scheme, proxy_host, proxy_port);
  }
  // Fall back to default proxy, if any.
  proxy_host = get_property.Run("proxyHost");
  if (!proxy_host.empty()) {
    std::string proxy_port = get_property.Run("proxyPort");
    return ConstructProxyServer(scheme, proxy_host, proxy_port);
  }
  return ProxyServer();
}

ProxyServer LookupSocksProxy(const GetPropertyCallback& get_property) {
  std::string proxy_host = get_property.Run("socksProxyHost");
  if (!proxy_host.empty()) {
    std::string proxy_port = get_property.Run("socksProxyPort");
    return ConstructProxyServer(ProxyServer::SCHEME_SOCKS5, proxy_host,
                                proxy_port);
  }
  return ProxyServer();
}

void AddBypassRules(const std::string& scheme,
                    const GetPropertyCallback& get_property,
                    ProxyBypassRules* bypass_rules) {
  // The format of a hostname pattern is a list of hostnames that are separated
  // by | and that use * as a wildcard. For example, setting the
  // http.nonProxyHosts property to *.android.com|*.kernel.org will cause
  // requests to http://developer.android.com to be made without a proxy.
  std::string non_proxy_hosts =
      get_property.Run(scheme + ".nonProxyHosts");
  if (non_proxy_hosts.empty())
    return;
  StringTokenizer tokenizer(non_proxy_hosts, "|");
  while (tokenizer.GetNext()) {
    std::string token = tokenizer.token();
    std::string pattern;
    TrimWhitespaceASCII(token, TRIM_ALL, &pattern);
    if (pattern.empty())
      continue;
    // '?' is not one of the specified pattern characters above.
    DCHECK_EQ(std::string::npos, pattern.find('?'));
    bypass_rules->AddRuleForHostname(scheme, pattern, -1);
  }
}

// Returns true if a valid proxy was found.
bool GetProxyRules(const GetPropertyCallback& get_property,
                   ProxyConfig::ProxyRules* rules) {
  // See libcore/luni/src/main/java/java/net/ProxySelectorImpl.java for the
  // equivalent Android implementation.
  rules->type = ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME;
  rules->proxy_for_http = LookupProxy("http", get_property,
                                      ProxyServer::SCHEME_HTTP);
  rules->proxy_for_https = LookupProxy("https", get_property,
                                       ProxyServer::SCHEME_HTTPS);
  rules->proxy_for_ftp = LookupProxy("ftp", get_property,
                                     ProxyServer::SCHEME_HTTP);
  rules->fallback_proxy = LookupSocksProxy(get_property);
  rules->bypass_rules.Clear();
  AddBypassRules("ftp", get_property, &rules->bypass_rules);
  AddBypassRules("http", get_property, &rules->bypass_rules);
  AddBypassRules("https", get_property, &rules->bypass_rules);
  return rules->proxy_for_http.is_valid() ||
      rules->proxy_for_https.is_valid() ||
      rules->proxy_for_ftp.is_valid() ||
      rules->fallback_proxy.is_valid();
};

void GetLatestProxyConfigInternal(const GetPropertyCallback& get_property,
                                  ProxyConfig* config) {
  if (!GetProxyRules(get_property, &config->proxy_rules()))
    *config = ProxyConfig::CreateDirect();
}

std::string GetJavaProperty(const std::string& property) {
  // Use Java System.getProperty to get configuration information.
  // TODO(pliard): Conversion to/from UTF8 ok here?
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> str = ConvertUTF8ToJavaString(env, property);
  ScopedJavaLocalRef<jstring> result =
      Java_ProxyChangeListener_getProperty(env, str.obj());
  return result.is_null() ?
      std::string() : ConvertJavaStringToUTF8(env, result.obj());
}

}  // namespace

class ProxyConfigServiceAndroid::Delegate
    : public base::RefCountedThreadSafe<Delegate> {
 public:
  Delegate(base::SequencedTaskRunner* network_task_runner,
           base::SequencedTaskRunner* jni_task_runner,
           const GetPropertyCallback& get_property_callback)
      : ALLOW_THIS_IN_INITIALIZER_LIST(jni_delegate_(this)),
        network_task_runner_(network_task_runner),
        jni_task_runner_(jni_task_runner),
        get_property_callback_(get_property_callback) {
  }

  void SetupJNI() {
    DCHECK(OnJNIThread());
    JNIEnv* env = AttachCurrentThread();
    if (java_proxy_change_listener_.is_null()) {
      java_proxy_change_listener_.Reset(
          Java_ProxyChangeListener_create(
              env, base::android::GetApplicationContext()));
      CHECK(!java_proxy_change_listener_.is_null());
    }
    Java_ProxyChangeListener_start(
        env,
        java_proxy_change_listener_.obj(),
        reinterpret_cast<jint>(&jni_delegate_));
  }

  void FetchInitialConfig() {
    DCHECK(OnJNIThread());
    ProxyConfig proxy_config;
    GetLatestProxyConfigInternal(get_property_callback_, &proxy_config);
    network_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&Delegate::SetNewConfigOnNetworkThread, this, proxy_config));
  }

  void Shutdown() {
    if (OnJNIThread()) {
      ShutdownOnJNIThread();
    } else {
      jni_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&Delegate::ShutdownOnJNIThread, this));
    }
  }

  // Called only on the network thread.
  void AddObserver(Observer* observer) {
    DCHECK(OnNetworkThread());
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) {
    DCHECK(OnNetworkThread());
    observers_.RemoveObserver(observer);
  }

  ConfigAvailability GetLatestProxyConfig(ProxyConfig* config) {
    DCHECK(OnNetworkThread());
    if (!config)
      return ProxyConfigService::CONFIG_UNSET;
    *config = proxy_config_;
    return ProxyConfigService::CONFIG_VALID;
  }

  // Called on the JNI thread.
  void ProxySettingsChanged() {
    DCHECK(OnJNIThread());
    ProxyConfig proxy_config;
    GetLatestProxyConfigInternal(get_property_callback_, &proxy_config);
    network_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &Delegate::SetNewConfigOnNetworkThread, this, proxy_config));
  }

 private:
  friend class base::RefCountedThreadSafe<Delegate>;

  class JNIDelegateImpl : public ProxyConfigServiceAndroid::JNIDelegate {
   public:
    explicit JNIDelegateImpl(Delegate* delegate) : delegate_(delegate) {}

    // ProxyConfigServiceAndroid::JNIDelegate overrides.
    virtual void ProxySettingsChanged(JNIEnv*, jobject) OVERRIDE {
      delegate_->ProxySettingsChanged();
    }

   private:
    Delegate* const delegate_;
  };

  virtual ~Delegate() {}

  void ShutdownOnJNIThread() {
    if (java_proxy_change_listener_.is_null())
      return;
    JNIEnv* env = AttachCurrentThread();
    Java_ProxyChangeListener_stop(env, java_proxy_change_listener_.obj());
  }

  // Called on the network thread.
  void SetNewConfigOnNetworkThread(const ProxyConfig& proxy_config) {
    DCHECK(OnNetworkThread());
    proxy_config_ = proxy_config;
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnProxyConfigChanged(proxy_config,
                                           ProxyConfigService::CONFIG_VALID));
  }

  bool OnJNIThread() const {
    return jni_task_runner_->RunsTasksOnCurrentThread();
  }

  bool OnNetworkThread() const {
    return network_task_runner_->RunsTasksOnCurrentThread();
  }

  ScopedJavaGlobalRef<jobject> java_proxy_change_listener_;

  JNIDelegateImpl jni_delegate_;
  ObserverList<Observer> observers_;
  scoped_refptr<base::SequencedTaskRunner> network_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> jni_task_runner_;
  GetPropertyCallback get_property_callback_;
  ProxyConfig proxy_config_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

ProxyConfigServiceAndroid::ProxyConfigServiceAndroid(
    base::SequencedTaskRunner* network_task_runner,
    base::SequencedTaskRunner* jni_task_runner)
    : delegate_(new Delegate(
        network_task_runner, jni_task_runner, base::Bind(&GetJavaProperty))) {
  delegate_->SetupJNI();
  delegate_->FetchInitialConfig();
}

ProxyConfigServiceAndroid::~ProxyConfigServiceAndroid() {
  delegate_->Shutdown();
}

// static
bool ProxyConfigServiceAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ProxyConfigServiceAndroid::AddObserver(Observer* observer) {
  delegate_->AddObserver(observer);
}

void ProxyConfigServiceAndroid::RemoveObserver(Observer* observer) {
  delegate_->RemoveObserver(observer);
}

ProxyConfigService::ConfigAvailability
ProxyConfigServiceAndroid::GetLatestProxyConfig(ProxyConfig* config) {
  return delegate_->GetLatestProxyConfig(config);
}

ProxyConfigServiceAndroid::ProxyConfigServiceAndroid(
    base::SequencedTaskRunner* network_task_runner,
    base::SequencedTaskRunner* jni_task_runner,
    GetPropertyCallback get_property_callback)
    : delegate_(new Delegate(
        network_task_runner, jni_task_runner, get_property_callback)) {
  delegate_->FetchInitialConfig();
}

void ProxyConfigServiceAndroid::ProxySettingsChanged() {
  delegate_->ProxySettingsChanged();
}

} // namespace net
