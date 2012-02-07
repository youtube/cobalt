// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service_win.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "base/win/object_watcher.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "googleurl/src/url_canon.h"
#include "net/base/net_util.h"
#include "net/dns/serial_worker.h"
#include "net/dns/watching_file_reader.h"

#pragma comment(lib, "iphlpapi.lib")

namespace net {

namespace {

// Watches a single registry key for changes. This class is thread-safe with
// the exception of StartWatch which must be called once and only once.
class RegistryWatcher : public base::win::ObjectWatcher::Delegate {
 public:
  RegistryWatcher() {}

  bool StartWatch(const wchar_t* key, const base::Closure& callback) {
    DCHECK(!callback.is_null());
    DCHECK(callback_.is_null());
    // TODO(szym): This will need to change to address http://crbug.com/99509
    DCHECK(!key_.Valid());
    if (key_.Open(HKEY_LOCAL_MACHINE, key,
                  KEY_NOTIFY | KEY_QUERY_VALUE) != ERROR_SUCCESS)
      return false;
    if (key_.StartWatching() != ERROR_SUCCESS)
      return false;
    if (!watcher_.StartWatching(key_.watch_event(), this))
      return false;
    callback_ = callback;
    return true;
  }

  void Cancel() {
    callback_.Reset();
    key_.StopWatching();
    watcher_.StopWatching();
  }

  virtual void OnObjectSignaled(HANDLE object) OVERRIDE {
    if (!callback_.is_null())
      callback_.Run();
    // TODO(szym): handle errors
    bool success = key_.StartWatching() &&
                   watcher_.StartWatching(key_.watch_event(), this);
    DCHECK(success);
  }

  bool ReadString(const wchar_t* name,
                  DnsSystemSettings::RegString* out) const {
    out->set = false;
    if (!key_.Valid()) {
      // Assume that if the |key_| is invalid then the key is missing.
      return true;
    }
    LONG result = key_.ReadValue(name, &out->value);
    if (result == ERROR_SUCCESS) {
      out->set = true;
      return true;
    }
    return (result == ERROR_FILE_NOT_FOUND);
  }

  bool ReadDword(const wchar_t* name,
                 DnsSystemSettings::RegDword* out) const {
    out->set = false;
    if (!key_.Valid()) {
      // Assume that if the |key_| is invalid then the key is missing.
      return true;
    }
    LONG result = key_.ReadValueDW(name, &out->value);
    if (result == ERROR_SUCCESS) {
      out->set = true;
      return true;
    }
    return (result == ERROR_FILE_NOT_FOUND);
  }

 private:
  base::Closure callback_;
  base::win::RegKey key_;
  base::win::ObjectWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(RegistryWatcher);
};

// Converts a string16 domain name to ASCII, possibly using punycode.
// Returns true if the conversion succeeds. In case of failure, |domain| might
// become dirty.
bool ParseDomainASCII(const string16& widestr, std::string* domain) {
  DCHECK(domain);
  // Check if already ASCII.
  if (IsStringASCII(widestr)) {
    *domain = UTF16ToASCII(widestr);
    return true;
  }

  // Otherwise try to convert it from IDN to punycode.
  const int kInitialBufferSize = 256;
  url_canon::RawCanonOutputT<char16, kInitialBufferSize> punycode;
  if (!url_canon::IDNToASCII(widestr.data(), widestr.length(), &punycode)) {
    return false;
  }

  // |punycode_output| should now be ASCII; convert it to a std::string.
  // (We could use UTF16ToASCII() instead, but that requires an extra string
  // copy. Since ASCII is a subset of UTF8 the following is equivalent).
  bool success = UTF16ToUTF8(punycode.data(), punycode.length(), domain);
  DCHECK(success);
  DCHECK(IsStringASCII(*domain));
  return success;
}

}  // namespace

bool ParseSearchList(const string16& value, std::vector<std::string>* output) {
  DCHECK(output);
  if (value.empty())
    return false;

  output->clear();

  // If the list includes an empty hostname (",," or ", ,"), it is terminated.
  // Although nslookup and network connection property tab ignore such
  // fragments ("a,b,,c" becomes ["a", "b", "c"]), our reference is getaddrinfo
  // (which sees ["a", "b"]). WMI queries also return a matching search list.
  std::vector<string16> woutput;
  base::SplitString(value, ',', &woutput);
  for (size_t i = 0; i < woutput.size(); ++i) {
    // Convert non-ASCII to punycode, although getaddrinfo does not properly
    // handle such suffixes.
    const string16& t = woutput[i];
    std::string parsed;
    if (t.empty() || !ParseDomainASCII(t, &parsed))
      break;
    output->push_back(parsed);
  }
  return !output->empty();
}

bool ConvertSettingsToDnsConfig(const DnsSystemSettings& settings,
                                DnsConfig* config) {
  *config = DnsConfig();

  // Use GetAdapterAddresses to get effective DNS server order and
  // connection-specific DNS suffix. Ignore disconnected and loopback adapters.
  // TODO(szym): Also ignore VM adapters. http://crbug.com/112856
  for (const IP_ADAPTER_ADDRESSES* adapter = settings.addresses.get();
       adapter != NULL;
       adapter = adapter->Next) {
    if (adapter->OperStatus != IfOperStatusUp)
      continue;
    if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
      continue;

    for (const IP_ADAPTER_DNS_SERVER_ADDRESS* address =
             adapter->FirstDnsServerAddress;
         address != NULL;
         address = address->Next) {
      IPEndPoint ipe;
      if (ipe.FromSockAddr(address->Address.lpSockaddr,
                           address->Address.iSockaddrLength)) {
        config->nameservers.push_back(ipe);
      } else {
        return false;
      }
    }

    // IP_ADAPTER_ADDRESSES in Vista+ has a search list at |FirstDnsSuffix|,
    // but it came up empty in all trials.
    // |DnsSuffix| stores the effective connection-specific suffix, which is
    // obtained via DHCP (regkey: Tcpip\Parameters\Interfaces\{XXX}\DhcpDomain)
    // or specified by the user (regkey: Tcpip\Parameters\Domain).
    std::string dns_suffix;
    if (ParseDomainASCII(adapter->DnsSuffix, &dns_suffix)) {
      config->search.push_back(dns_suffix);
    }
  }

  if (config->nameservers.empty())
    return false;  // No point continuing.

  // Windows always tries a multi-label name "as is" before using suffixes.
  config->ndots = 1;

  if (!settings.append_to_multi_label_name.set) {
    // The default setting is true for XP, false for Vista+.
    if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
      config->append_to_multi_label_name = false;
    } else {
      config->append_to_multi_label_name = true;
    }
  } else {
    config->append_to_multi_label_name =
        (settings.append_to_multi_label_name.value != 0);
  }

  // SearchList takes precedence, so check it first.
  if (settings.policy_search_list.set) {
    std::vector<std::string> search;
    if (ParseSearchList(settings.policy_search_list.value, &search)) {
      config->search.swap(search);
      return true;
    }
    // Even if invalid, the policy disables the user-specified setting below.
  } else if (settings.tcpip_search_list.set) {
    std::vector<std::string> search;
    if (ParseSearchList(settings.tcpip_search_list.value, &search)) {
      config->search.swap(search);
      return true;
    }
  }

  if (!settings.tcpip_domain.set)
    return true;

  std::string primary_suffix;
  if (!ParseDomainASCII(settings.tcpip_domain.value, &primary_suffix))
    return true;  // No primary suffix, hence no devolution.

  // Primary suffix goes in front.
  config->search.insert(config->search.begin(), primary_suffix);

  // Devolution is determined by precedence: policy > dnscache > tcpip.
  // |enabled|: UseDomainNameDevolution and |level|: DomainNameDevolutionLevel
  // are overridden independently.
  DnsSystemSettings::DevolutionSetting devolution = settings.policy_devolution;

  if (!devolution.enabled.set)
    devolution.enabled = settings.dnscache_devolution.enabled;
  if (!devolution.enabled.set)
    devolution.enabled = settings.tcpip_devolution.enabled;
  if (devolution.enabled.set && (devolution.enabled.value == 0))
    return true;  // Devolution disabled.

  // By default devolution is enabled.

  if (!devolution.level.set)
    devolution.level = settings.dnscache_devolution.level;
  if (!devolution.level.set)
    devolution.level = settings.tcpip_devolution.level;

  // After the recent update, Windows will try to determine a safe default
  // value by comparing the forest root domain (FRD) to the primary suffix.
  // See http://support.microsoft.com/kb/957579 for details.
  // For now, if the level is not set, we disable devolution, assuming that
  // we will fallback to the system getaddrinfo anyway. This might cause
  // performance loss for resolutions which depend on the system default
  // devolution setting.
  //
  // If the level is explicitly set below 2, devolution is disabled.
  if (!devolution.level.set || devolution.level.value < 2)
    return true;  // Devolution disabled.

  // Devolve the primary suffix. This naive logic matches the observed
  // behavior (see also ParseSearchList). If a suffix is not valid, it will be
  // discarded when the fully-qualified name is converted to DNS format.

  unsigned num_dots = std::count(primary_suffix.begin(),
                                 primary_suffix.end(), '.');

  for (size_t offset = 0; num_dots >= devolution.level.value; --num_dots) {
    offset = primary_suffix.find('.', offset + 1);
    config->search.push_back(primary_suffix.substr(offset + 1));
  }

  return true;
}

// Watches registry for changes and reads config from registry and IP helper.
// Reading and opening of reg keys is always performed on WorkerPool. Setting
// up watches requires IO loop.
class DnsConfigServiceWin::ConfigReader : public SerialWorker {
 public:
  explicit ConfigReader(DnsConfigServiceWin* service)
      : service_(service),
        success_(false) {}

  bool StartWatch() {
    DCHECK(loop()->BelongsToCurrentThread());

    // This is done only once per lifetime so open the keys on this thread.
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    base::Closure callback = base::Bind(&SerialWorker::WorkNow,
                                        base::Unretained(this));

    // DNS suffix search list and devolution can be configured via group
    // policy which sets this registry key. If the key is missing, the policy
    // does not apply, and the DNS client uses Tcpip and Dnscache settings.
    // If a policy is installed, DnsConfigService will need to be restarted.
    // BUG=99509
    policy_watcher_.StartWatch(
        L"SOFTWARE\\Policies\\Microsoft\\Windows NT\\DNSClient",
        callback);

    // The Dnscache key might also be missing.
    dnscache_watcher_.StartWatch(
        L"SYSTEM\\CurrentControlSet\\Services\\Dnscache\\Parameters",
        callback);

    // The Tcpip key must be present.
    if (!tcpip_watcher_.StartWatch(
        L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
        callback))
    return false;

    WorkNow();
    return true;
  }

  void Cancel() {
    DCHECK(loop()->BelongsToCurrentThread());
    SerialWorker::Cancel();
    policy_watcher_.Cancel();
    dnscache_watcher_.Cancel();
    tcpip_watcher_.Cancel();
  }

  // Delay between calls to WorkerPool::PostTask
  static const int kWorkerPoolRetryDelayMs = 100;

 private:
  friend class RegistryWatcher;
  virtual ~ConfigReader() {
    DCHECK(IsCancelled());
  }

  bool ReadIpHelper(DnsSystemSettings* settings) {
    base::ThreadRestrictions::AssertIOAllowed();

    ULONG flags = GAA_FLAG_SKIP_ANYCAST |
                  GAA_FLAG_SKIP_UNICAST |
                  GAA_FLAG_SKIP_MULTICAST |
                  GAA_FLAG_SKIP_FRIENDLY_NAME;
    ULONG len = 15000;  // As recommended by MSDN for GetAdaptersAddresses.
    UINT rv = ERROR_BUFFER_OVERFLOW;
    // Try up to three times.
    for (unsigned tries = 0;
         (tries < 3) && (rv == ERROR_BUFFER_OVERFLOW);
         tries++) {
      settings->addresses.reset(
          reinterpret_cast<PIP_ADAPTER_ADDRESSES>(malloc(len)));
      rv = GetAdaptersAddresses(AF_UNSPEC, flags, NULL,
                                settings->addresses.get(), &len);
    }
    return (rv == NO_ERROR);
  }

  bool ReadDevolutionSetting(const RegistryWatcher& watcher,
                             DnsSystemSettings::DevolutionSetting& setting) {
    return watcher.ReadDword(L"UseDomainNameDevolution", &setting.enabled) &&
           watcher.ReadDword(L"DomainNameDevolutionLevel", &setting.level);
  }

  virtual void DoWork() OVERRIDE {
    // Should be called on WorkerPool.
    success_ = false;

    DnsSystemSettings settings;
    memset(&settings, 0, sizeof(settings));
    if (!ReadIpHelper(&settings))
      return;  // no point reading the rest

    if (!policy_watcher_.ReadString(L"SearchList",
                                    &settings.policy_search_list))
      return;

    if (!tcpip_watcher_.ReadString(L"SearchList", &settings.tcpip_search_list))
      return;

    if (!tcpip_watcher_.ReadString(L"Domain", &settings.tcpip_domain))
      return;

    if (!ReadDevolutionSetting(policy_watcher_, settings.policy_devolution))
      return;

    if (!ReadDevolutionSetting(dnscache_watcher_,
                               settings.dnscache_devolution))
      return;

    if (!ReadDevolutionSetting(tcpip_watcher_, settings.tcpip_devolution))
      return;

    if (!policy_watcher_.ReadDword(L"AppendToMultiLabelName",
                                   &settings.append_to_multi_label_name))
      return;

    success_ = ConvertSettingsToDnsConfig(settings, &dns_config_);
  }

  virtual void OnWorkFinished() OVERRIDE {
    DCHECK(loop()->BelongsToCurrentThread());
    DCHECK(!IsCancelled());
    if (success_) {
      service_->OnConfigRead(dns_config_);
    } else {
      LOG(WARNING) << "Failed to read config.";
    }
  }

  DnsConfigServiceWin* service_;
  // Written in DoRead(), read in OnReadFinished(). No locking required.
  DnsConfig dns_config_;
  bool success_;

  RegistryWatcher policy_watcher_;
  RegistryWatcher dnscache_watcher_;
  RegistryWatcher tcpip_watcher_;
};

DnsConfigServiceWin::DnsConfigServiceWin()
    : config_reader_(new ConfigReader(this)),
      hosts_watcher_(new WatchingFileReader()) {}

DnsConfigServiceWin::~DnsConfigServiceWin() {
  DCHECK(CalledOnValidThread());
  config_reader_->Cancel();
}

void DnsConfigServiceWin::Watch() {
  DCHECK(CalledOnValidThread());
  bool started = config_reader_->StartWatch();
  // TODO(szym): handle possible failure
  DCHECK(started);

  TCHAR buffer[MAX_PATH];
  UINT rc = GetSystemDirectory(buffer, MAX_PATH);
  DCHECK(0 < rc && rc < MAX_PATH);
  FilePath hosts_path = FilePath(buffer).
      Append(FILE_PATH_LITERAL("drivers\\etc\\hosts"));
  hosts_watcher_->StartWatch(hosts_path, new DnsHostsReader(hosts_path, this));
}

// static
DnsConfigService* DnsConfigService::CreateSystemService() {
  return new DnsConfigServiceWin();
}

}  // namespace net

