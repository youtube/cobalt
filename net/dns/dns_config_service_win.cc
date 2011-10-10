// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service_win.h"

#include <iphlpapi.h>
#include <windows.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "base/win/object_watcher.h"
#include "base/win/registry.h"
#include "googleurl/src/url_canon.h"
#include "net/base/net_util.h"
#include "net/dns/serial_worker.h"
#include "net/dns/watching_file_reader.h"

namespace net {

// Converts a string16 domain name to ASCII, possibly using punycode.
// Returns true if the conversion succeeds.
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
  if (!url_canon::IDNToASCII(widestr.data(),
                             widestr.length(),
                             &punycode)) {
    return false;
  }

  // |punycode_output| should now be ASCII; convert it to a std::string.
  // (We could use UTF16ToASCII() instead, but that requires an extra string
  // copy. Since ASCII is a subset of UTF8 the following is equivalent).
  bool success = UTF16ToUTF8(punycode.data(),
                             punycode.length(),
                             domain);
  DCHECK(success);
  DCHECK(IsStringASCII(*domain));
  return success;
}

bool ParseSearchList(const string16& value, std::vector<std::string>* out) {
  DCHECK(out);
  if (value.empty())
    return false;

  // If the list includes an empty hostname (",," or ", ,"), it is terminated.
  // Although nslookup and network connection property tab ignore such
  // fragments ("a,b,,c" becomes ["a", "b", "c"]), our reference is getaddrinfo
  // (which sees ["a", "b"]). WMI queries also return a matching search list.
  std::vector<string16> woutput;
  base::SplitString(value, ',', &woutput);
  std::vector<std::string> output;
  for (size_t i = 0; i < woutput.size(); ++i) {
    // Convert non-ascii to punycode, although getaddrinfo does not properly
    // handle such suffixes.
    const string16& t = woutput[i];
    std::string parsed;
    if (t.empty() || !ParseDomainASCII(t, &parsed))
      break;
    output.push_back(parsed);
  }
  if (output.empty())
    return false;
  out->swap(output);
  return true;
}

// Watches registry for changes and reads config from registry and IP helper.
// Reading and opening of reg keys is always performed on WorkerPool. Setting
// up watches requires IO loop.
class DnsConfigServiceWin::ConfigReader : public SerialWorker {
 public:
  // Watches a single registry key for changes.
  class RegistryWatcher : public base::win::ObjectWatcher::Delegate {
   public:
    explicit RegistryWatcher(ConfigReader* reader) : reader_(reader) {}

    bool StartWatch(const wchar_t* key) {
      DCHECK(!key_.IsWatching());
      if (key_.Open(HKEY_LOCAL_MACHINE, key,
                    KEY_NOTIFY | KEY_QUERY_VALUE) != ERROR_SUCCESS)
        return false;
      if (key_.StartWatching() != ERROR_SUCCESS)
        return false;
      if (!watcher_.StartWatching(key_.watch_event(), this))
        return false;
      return true;
    }

    void Cancel() {
      reader_ = NULL;
      key_.StopWatching();
      watcher_.StopWatching();
    }

    void OnObjectSignaled(HANDLE object) OVERRIDE {
      if (reader_)
        reader_->WorkNow();
      // TODO(szym): handle errors
      key_.StartWatching();
      watcher_.StartWatching(key_.watch_event(), this);
    }

    bool Read(const wchar_t* name, string16* value) const {
      return key_.ReadValue(name, value) == ERROR_SUCCESS;
    }

   private:
    ConfigReader* reader_;
    base::win::RegKey key_;
    base::win::ObjectWatcher watcher_;
  };

  explicit ConfigReader(DnsConfigServiceWin* service)
    : policy_watcher_(this),
      tcpip_watcher_(this),
      service_(service) {}

  bool StartWatch() {
    DCHECK(loop()->BelongsToCurrentThread());

    // This is done only once per lifetime so open the keys on this thread.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    if (!tcpip_watcher_.StartWatch(
        L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters"))
      return false;

    // DNS suffix search list can be installed via group policy which sets
    // this registry key. If the key is missing, the policy does not apply,
    // and the DNS client uses DHCP and manual settings.
    // If a policy is installed, DnsConfigService will need to be restarted.
    // BUG=99509
    policy_watcher_.StartWatch(
        L"Software\\Policies\\Microsoft\\Windows NT\\DNSClient");
    WorkNow();
    return true;
  }

  void Cancel() {
    DCHECK(loop()->BelongsToCurrentThread());
    SerialWorker::Cancel();
    policy_watcher_.Cancel();
    tcpip_watcher_.Cancel();
  }

  // Delay between calls to WorkerPool::PostTask
  static const int kWorkerPoolRetryDelayMs = 100;

 private:
  friend class RegistryWatcher;
  virtual ~ConfigReader() {}

  // Uses GetAdapterAddresses to get effective DNS server order and default
  // DNS suffix.
  bool ReadIpHelper(DnsConfig* config) {
    base::ThreadRestrictions::AssertIOAllowed();

    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_UNICAST |
                  GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_FRIENDLY_NAME;
    IP_ADAPTER_ADDRESSES buf;
    IP_ADAPTER_ADDRESSES* adapters = &buf;
    ULONG len = sizeof(buf);
    scoped_array<char> extra_buf;

    UINT rv = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, adapters, &len);
    if (rv == ERROR_BUFFER_OVERFLOW) {
      extra_buf.reset(new char[len]);
      adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(extra_buf.get());
      rv = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, adapters, &len);
    }
    if (rv != NO_ERROR)
      return false;

    config->search.clear();
    config->nameservers.clear();

    IP_ADAPTER_ADDRESSES* adapter;
    for (adapter = adapters; adapter != NULL; adapter = adapter->Next) {
      if (adapter->OperStatus != IfOperStatusUp)
        continue;
      if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
        continue;

      // IP_ADAPTER_ADDRESSES in Vista+ has a search list at FirstDnsSuffix,
      // but it's only the per-interface search list. First initialize with
      // DnsSuffix, then override from registry.
      std::string dns_suffix;
      if (ParseDomainASCII(adapter->DnsSuffix, &dns_suffix)) {
        config->search.push_back(dns_suffix);
      }
      IP_ADAPTER_DNS_SERVER_ADDRESS* address;
      for (address = adapter->FirstDnsServerAddress; address != NULL;
           address = address->Next) {
        IPEndPoint ipe;
        if (ipe.FromSockAddr(address->Address.lpSockaddr,
                             address->Address.iSockaddrLength)) {
          config->nameservers.push_back(ipe);
        } else {
          return false;
        }
      }
    }
    return true;
  }

  // Read and parse SearchList from registry. Only accept the value if all
  // elements of the search list are non-empty and can be converted to UTF-8.
  bool ReadSearchList(const RegistryWatcher& watcher, DnsConfig* config) {
    string16 value;
    if (!watcher.Read(L"SearchList", &value))
      return false;
    return ParseSearchList(value, &(config->search));
  }

  void DoWork() OVERRIDE {
    // Should be called on WorkerPool.
    dns_config_ = DnsConfig();
    if (!ReadIpHelper(&dns_config_))
      return;  // no point reading the rest

    // check global registry overrides
    if (!ReadSearchList(policy_watcher_, &dns_config_))
      ReadSearchList(tcpip_watcher_, &dns_config_);

    // TODO(szym): add support for DNS suffix devolution BUG=99510
  }

  void OnWorkFinished() OVERRIDE {
    DCHECK(loop()->BelongsToCurrentThread());
    DCHECK(!IsCancelled());
    if (dns_config_.IsValid()) {
      service_->OnConfigRead(dns_config_);
    } else {
      LOG(WARNING) << "Failed to read name servers from registry";
    }
  }

  // Written in DoRead(), read in OnReadFinished(). No locking required.
  DnsConfig dns_config_;

  RegistryWatcher policy_watcher_;
  RegistryWatcher tcpip_watcher_;

  DnsConfigServiceWin* service_;
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

