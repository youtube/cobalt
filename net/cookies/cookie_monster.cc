// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Portions of this code based on Mozilla:
//   (netwerk/cookie/src/nsCookieService.cpp)
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@stanford.edu)
 *   Michiel van Leeuwen (mvl@exedo.nl)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "net/cookies/cookie_monster.h"

#include <algorithm>
#include <set>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "googleurl/src/gurl.h"
#include "net/cookies/canonical_cookie.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/parsed_cookie.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

// In steady state, most cookie requests can be satisfied by the in memory
// cookie monster store.  However, if a request comes in during the initial
// cookie load, it must be delayed until that load completes. That is done by
// queueing it on CookieMonster::queue_ and running it when notification of
// cookie load completion is received via CookieMonster::OnLoaded. This callback
// is passed to the persistent store from CookieMonster::InitStore(), which is
// called on the first operation invoked on the CookieMonster.
//
// On the browser critical paths (e.g. for loading initial web pages in a
// session restore) it may take too long to wait for the full load. If a cookie
// request is for a specific URL, DoCookieTaskForURL is called, which triggers a
// priority load if the key is not loaded yet by calling PersistentCookieStore
// :: LoadCookiesForKey. The request is queued in CookieMonster::tasks_queued
// and executed upon receiving notification of key load completion via
// CookieMonster::OnKeyLoaded(). If multiple requests for the same eTLD+1 are
// received before key load completion, only the first request calls
// PersistentCookieStore::LoadCookiesForKey, all subsequent requests are queued
// in CookieMonster::tasks_queued and executed upon receiving notification of
// key load completion triggered by the first request for the same eTLD+1.

static const int kMinutesInTenYears = 10 * 365 * 24 * 60;

namespace net {

// See comments at declaration of these variables in cookie_monster.h
// for details.
const size_t CookieMonster::kDomainMaxCookies           = 180;
const size_t CookieMonster::kDomainPurgeCookies         = 30;
const size_t CookieMonster::kMaxCookies                 = 3300;
const size_t CookieMonster::kPurgeCookies               = 300;
const int CookieMonster::kSafeFromGlobalPurgeDays       = 30;

namespace {

typedef std::vector<CanonicalCookie*> CanonicalCookieVector;

// Default minimum delay after updating a cookie's LastAccessDate before we
// will update it again.
const int kDefaultAccessUpdateThresholdSeconds = 60;

// Comparator to sort cookies from highest creation date to lowest
// creation date.
struct OrderByCreationTimeDesc {
  bool operator()(const CookieMonster::CookieMap::iterator& a,
                  const CookieMonster::CookieMap::iterator& b) const {
    return a->second->CreationDate() > b->second->CreationDate();
  }
};

// Constants for use in VLOG
const int kVlogPerCookieMonster = 1;
const int kVlogPeriodic = 3;
const int kVlogGarbageCollection = 5;
const int kVlogSetCookies = 7;
const int kVlogGetCookies = 9;

// Mozilla sorts on the path length (longest first), and then it
// sorts by creation time (oldest first).
// The RFC says the sort order for the domain attribute is undefined.
bool CookieSorter(CanonicalCookie* cc1, CanonicalCookie* cc2) {
  if (cc1->Path().length() == cc2->Path().length())
    return cc1->CreationDate() < cc2->CreationDate();
  return cc1->Path().length() > cc2->Path().length();
}

bool LRUCookieSorter(const CookieMonster::CookieMap::iterator& it1,
                     const CookieMonster::CookieMap::iterator& it2) {
  // Cookies accessed less recently should be deleted first.
  if (it1->second->LastAccessDate() != it2->second->LastAccessDate())
    return it1->second->LastAccessDate() < it2->second->LastAccessDate();

  // In rare cases we might have two cookies with identical last access times.
  // To preserve the stability of the sort, in these cases prefer to delete
  // older cookies over newer ones.  CreationDate() is guaranteed to be unique.
  return it1->second->CreationDate() < it2->second->CreationDate();
}

// Our strategy to find duplicates is:
// (1) Build a map from (cookiename, cookiepath) to
//     {list of cookies with this signature, sorted by creation time}.
// (2) For each list with more than 1 entry, keep the cookie having the
//     most recent creation time, and delete the others.
//
// Two cookies are considered equivalent if they have the same domain,
// name, and path.
struct CookieSignature {
 public:
  CookieSignature(const std::string& name, const std::string& domain,
                  const std::string& path)
      : name(name),
        domain(domain),
        path(path) {}

  // To be a key for a map this class needs to be assignable, copyable,
  // and have an operator<.  The default assignment operator
  // and copy constructor are exactly what we want.

  bool operator<(const CookieSignature& cs) const {
    // Name compare dominates, then domain, then path.
    int diff = name.compare(cs.name);
    if (diff != 0)
      return diff < 0;

    diff = domain.compare(cs.domain);
    if (diff != 0)
      return diff < 0;

    return path.compare(cs.path) < 0;
  }

  std::string name;
  std::string domain;
  std::string path;
};

// Determine the cookie domain to use for setting the specified cookie.
bool GetCookieDomain(const GURL& url,
                     const ParsedCookie& pc,
                     std::string* result) {
  std::string domain_string;
  if (pc.HasDomain())
    domain_string = pc.Domain();
  return cookie_util::GetCookieDomainWithString(url, domain_string, result);
}

// Helper for GarbageCollection.  If |cookie_its->size() > num_max|, remove the
// |num_max - num_purge| most recently accessed cookies from cookie_its.
// (In other words, leave the entries that are candidates for
// eviction in cookie_its.)  The cookies returned will be in order sorted by
// access time, least recently accessed first.  The access time of the least
// recently accessed entry not returned will be placed in
// |*lra_removed| if that pointer is set.  FindLeastRecentlyAccessed
// returns false if no manipulation is done (because the list size is less
// than num_max), true otherwise.
bool FindLeastRecentlyAccessed(
    size_t num_max,
    size_t num_purge,
    Time* lra_removed,
    std::vector<CookieMonster::CookieMap::iterator>* cookie_its) {
  DCHECK_LE(num_purge, num_max);
  if (cookie_its->size() > num_max) {
    VLOG(kVlogGarbageCollection)
        << "FindLeastRecentlyAccessed() Deep Garbage Collect.";
    num_purge += cookie_its->size() - num_max;
    DCHECK_GT(cookie_its->size(), num_purge);

    // Add 1 so that we can get the last time left in the store.
    std::partial_sort(cookie_its->begin(), cookie_its->begin() + num_purge + 1,
                      cookie_its->end(), LRUCookieSorter);
    *lra_removed =
        (*(cookie_its->begin() + num_purge))->second->LastAccessDate();
    cookie_its->erase(cookie_its->begin() + num_purge, cookie_its->end());
    return true;
  }
  return false;
}

// Mapping between DeletionCause and Delegate::ChangeCause; the mapping also
// provides a boolean that specifies whether or not an OnCookieChanged
// notification ought to be generated.
typedef struct ChangeCausePair_struct {
  CookieMonster::Delegate::ChangeCause cause;
  bool notify;
} ChangeCausePair;
ChangeCausePair ChangeCauseMapping[] = {
  // DELETE_COOKIE_EXPLICIT
  { CookieMonster::Delegate::CHANGE_COOKIE_EXPLICIT, true },
  // DELETE_COOKIE_OVERWRITE
  { CookieMonster::Delegate::CHANGE_COOKIE_OVERWRITE, true },
  // DELETE_COOKIE_EXPIRED
  { CookieMonster::Delegate::CHANGE_COOKIE_EXPIRED, true },
  // DELETE_COOKIE_EVICTED
  { CookieMonster::Delegate::CHANGE_COOKIE_EVICTED, true },
  // DELETE_COOKIE_DUPLICATE_IN_BACKING_STORE
  { CookieMonster::Delegate::CHANGE_COOKIE_EXPLICIT, false },
  // DELETE_COOKIE_DONT_RECORD
  { CookieMonster::Delegate::CHANGE_COOKIE_EXPLICIT, false },
  // DELETE_COOKIE_EVICTED_DOMAIN
  { CookieMonster::Delegate::CHANGE_COOKIE_EVICTED, true },
  // DELETE_COOKIE_EVICTED_GLOBAL
  { CookieMonster::Delegate::CHANGE_COOKIE_EVICTED, true },
  // DELETE_COOKIE_EVICTED_DOMAIN_PRE_SAFE
  { CookieMonster::Delegate::CHANGE_COOKIE_EVICTED, true },
  // DELETE_COOKIE_EVICTED_DOMAIN_POST_SAFE
  { CookieMonster::Delegate::CHANGE_COOKIE_EVICTED, true },
  // DELETE_COOKIE_EXPIRED_OVERWRITE
  { CookieMonster::Delegate::CHANGE_COOKIE_EXPIRED_OVERWRITE, true },
  // DELETE_COOKIE_LAST_ENTRY
  { CookieMonster::Delegate::CHANGE_COOKIE_EXPLICIT, false }
};

std::string BuildCookieLine(const CanonicalCookieVector& cookies) {
  std::string cookie_line;
  for (CanonicalCookieVector::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    if (it != cookies.begin())
      cookie_line += "; ";
    // In Mozilla if you set a cookie like AAAA, it will have an empty token
    // and a value of AAAA.  When it sends the cookie back, it will send AAAA,
    // so we need to avoid sending =AAAA for a blank token value.
    if (!(*it)->Name().empty())
      cookie_line += (*it)->Name() + "=";
    cookie_line += (*it)->Value();
  }
  return cookie_line;
}

void BuildCookieInfoList(const CanonicalCookieVector& cookies,
                         std::vector<CookieStore::CookieInfo>* cookie_infos) {
  for (CanonicalCookieVector::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    const CanonicalCookie* cookie = *it;
    CookieStore::CookieInfo cookie_info;

    cookie_info.name = cookie->Name();
    cookie_info.creation_date = cookie->CreationDate();
    cookie_info.mac_key = cookie->MACKey();
    cookie_info.mac_algorithm = cookie->MACAlgorithm();

    cookie_infos->push_back(cookie_info);
  }
}

}  // namespace

// static
bool CookieMonster::default_enable_file_scheme_ = false;

CookieMonster::CookieMonster(PersistentCookieStore* store, Delegate* delegate)
    : initialized_(false),
      loaded_(false),
      store_(store),
      last_access_threshold_(
          TimeDelta::FromSeconds(kDefaultAccessUpdateThresholdSeconds)),
      delegate_(delegate),
      last_statistic_record_time_(Time::Now()),
      keep_expired_cookies_(false),
      persist_session_cookies_(false) {
  InitializeHistograms();
  SetDefaultCookieableSchemes();
}

CookieMonster::CookieMonster(PersistentCookieStore* store,
                             Delegate* delegate,
                             int last_access_threshold_milliseconds)
    : initialized_(false),
      loaded_(false),
      store_(store),
      last_access_threshold_(base::TimeDelta::FromMilliseconds(
          last_access_threshold_milliseconds)),
      delegate_(delegate),
      last_statistic_record_time_(base::Time::Now()),
      keep_expired_cookies_(false),
      persist_session_cookies_(false) {
  InitializeHistograms();
  SetDefaultCookieableSchemes();
}


// Task classes for queueing the coming request.

class CookieMonster::CookieMonsterTask
    : public base::RefCountedThreadSafe<CookieMonsterTask> {
 public:
  // Runs the task and invokes the client callback on the thread that
  // originally constructed the task.
  virtual void Run() = 0;

 protected:
  explicit CookieMonsterTask(CookieMonster* cookie_monster);
  virtual ~CookieMonsterTask();

  // Invokes the callback immediately, if the current thread is the one
  // that originated the task, or queues the callback for execution on the
  // appropriate thread. Maintains a reference to this CookieMonsterTask
  // instance until the callback completes.
  void InvokeCallback(base::Closure callback);

  CookieMonster* cookie_monster() {
    return cookie_monster_;
  }

 private:
  friend class base::RefCountedThreadSafe<CookieMonsterTask>;

  CookieMonster* cookie_monster_;
  scoped_refptr<base::MessageLoopProxy> thread_;

  DISALLOW_COPY_AND_ASSIGN(CookieMonsterTask);
};

CookieMonster::CookieMonsterTask::CookieMonsterTask(
    CookieMonster* cookie_monster)
    : cookie_monster_(cookie_monster),
      thread_(base::MessageLoopProxy::current()) {
}

CookieMonster::CookieMonsterTask::~CookieMonsterTask() {}

// Unfortunately, one cannot re-bind a Callback with parameters into a closure.
// Therefore, the closure passed to InvokeCallback is a clumsy binding of
// Callback::Run on a wrapped Callback instance. Since Callback is not
// reference counted, we bind to an instance that is a member of the
// CookieMonsterTask subclass. Then, we cannot simply post the callback to a
// message loop because the underlying instance may be destroyed (along with the
// CookieMonsterTask instance) in the interim. Therefore, we post a callback
// bound to the CookieMonsterTask, which *is* reference counted (thus preventing
// destruction of the original callback), and which invokes the closure (which
// invokes the original callback with the returned data).
void CookieMonster::CookieMonsterTask::InvokeCallback(base::Closure callback) {
  if (thread_->BelongsToCurrentThread()) {
    callback.Run();
  } else {
    thread_->PostTask(FROM_HERE, base::Bind(
        &CookieMonster::CookieMonsterTask::InvokeCallback, this, callback));
  }
}

// Task class for SetCookieWithDetails call.
class CookieMonster::SetCookieWithDetailsTask
    : public CookieMonster::CookieMonsterTask {
 public:
  SetCookieWithDetailsTask(
      CookieMonster* cookie_monster,
      const GURL& url, const std::string& name, const std::string& value,
      const std::string& domain, const std::string& path,
      const base::Time& expiration_time, bool secure, bool http_only,
      const CookieMonster::SetCookiesCallback& callback)
      : CookieMonsterTask(cookie_monster),
        url_(url),
        name_(name),
        value_(value),
        domain_(domain),
        path_(path),
        expiration_time_(expiration_time),
        secure_(secure),
        http_only_(http_only),
        callback_(callback) {
  }

  // CookieMonster::CookieMonsterTask:
  virtual void Run() override;

 protected:
  virtual ~SetCookieWithDetailsTask() {}

 private:
  GURL url_;
  std::string name_;
  std::string value_;
  std::string domain_;
  std::string path_;
  base::Time expiration_time_;
  bool secure_;
  bool http_only_;
  CookieMonster::SetCookiesCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SetCookieWithDetailsTask);
};

void CookieMonster::SetCookieWithDetailsTask::Run() {
  bool success = this->cookie_monster()->
      SetCookieWithDetails(url_, name_, value_, domain_, path_,
                           expiration_time_, secure_, http_only_);
  if (!callback_.is_null()) {
    this->InvokeCallback(base::Bind(&CookieMonster::SetCookiesCallback::Run,
                                    base::Unretained(&callback_), success));
  }
}

// Task class for GetAllCookies call.
class CookieMonster::GetAllCookiesTask
    : public CookieMonster::CookieMonsterTask {
 public:
  GetAllCookiesTask(CookieMonster* cookie_monster,
                    const CookieMonster::GetCookieListCallback& callback)
      : CookieMonsterTask(cookie_monster),
        callback_(callback) {
  }

  // CookieMonster::CookieMonsterTask
  virtual void Run() override;

 protected:
  virtual ~GetAllCookiesTask() {}

 private:
  CookieMonster::GetCookieListCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetAllCookiesTask);
};

void CookieMonster::GetAllCookiesTask::Run() {
  if (!callback_.is_null()) {
    CookieList cookies = this->cookie_monster()->GetAllCookies();
    this->InvokeCallback(base::Bind(&CookieMonster::GetCookieListCallback::Run,
                                    base::Unretained(&callback_), cookies));
    }
}

// Task class for GetAllCookiesForURLWithOptions call.
class CookieMonster::GetAllCookiesForURLWithOptionsTask
    : public CookieMonster::CookieMonsterTask {
 public:
  GetAllCookiesForURLWithOptionsTask(
      CookieMonster* cookie_monster,
      const GURL& url,
      const CookieOptions& options,
      const CookieMonster::GetCookieListCallback& callback)
      : CookieMonsterTask(cookie_monster),
        url_(url),
        options_(options),
        callback_(callback) {
  }

  // CookieMonster::CookieMonsterTask:
  virtual void Run() override;

 protected:
  virtual ~GetAllCookiesForURLWithOptionsTask() {}

 private:
  GURL url_;
  CookieOptions options_;
  CookieMonster::GetCookieListCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetAllCookiesForURLWithOptionsTask);
};

void CookieMonster::GetAllCookiesForURLWithOptionsTask::Run() {
  if (!callback_.is_null()) {
    CookieList cookies = this->cookie_monster()->
        GetAllCookiesForURLWithOptions(url_, options_);
    this->InvokeCallback(base::Bind(&CookieMonster::GetCookieListCallback::Run,
                                    base::Unretained(&callback_), cookies));
  }
}

// Task class for DeleteAll call.
class CookieMonster::DeleteAllTask : public CookieMonster::CookieMonsterTask {
 public:
  DeleteAllTask(CookieMonster* cookie_monster,
                const CookieMonster::DeleteCallback& callback)
      : CookieMonsterTask(cookie_monster),
        callback_(callback) {
  }

  // CookieMonster::CookieMonsterTask:
  virtual void Run() override;

 protected:
  virtual ~DeleteAllTask() {}

 private:
  CookieMonster::DeleteCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DeleteAllTask);
};

void CookieMonster::DeleteAllTask::Run() {
  int num_deleted = this->cookie_monster()->DeleteAll(true);
  if (!callback_.is_null()) {
    this->InvokeCallback(base::Bind(&CookieMonster::DeleteCallback::Run,
                                    base::Unretained(&callback_), num_deleted));
  }
}

// Task class for DeleteAllCreatedBetween call.
class CookieMonster::DeleteAllCreatedBetweenTask
    : public CookieMonster::CookieMonsterTask {
 public:
  DeleteAllCreatedBetweenTask(
      CookieMonster* cookie_monster,
      const Time& delete_begin,
      const Time& delete_end,
      const CookieMonster::DeleteCallback& callback)
      : CookieMonsterTask(cookie_monster),
        delete_begin_(delete_begin),
        delete_end_(delete_end),
        callback_(callback) {
  }

  // CookieMonster::CookieMonsterTask:
  virtual void Run() override;

 protected:
  virtual ~DeleteAllCreatedBetweenTask() {}

 private:
  Time delete_begin_;
  Time delete_end_;
  CookieMonster::DeleteCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DeleteAllCreatedBetweenTask);
};

void CookieMonster::DeleteAllCreatedBetweenTask::Run() {
  int num_deleted = this->cookie_monster()->
      DeleteAllCreatedBetween(delete_begin_, delete_end_);
  if (!callback_.is_null()) {
    this->InvokeCallback(base::Bind(&CookieMonster::DeleteCallback::Run,
                                    base::Unretained(&callback_), num_deleted));
  }
}

// Task class for DeleteAllForHost call.
class CookieMonster::DeleteAllForHostTask
    : public CookieMonster::CookieMonsterTask {
 public:
  DeleteAllForHostTask(CookieMonster* cookie_monster,
                       const GURL& url,
                       const CookieMonster::DeleteCallback& callback)
      : CookieMonsterTask(cookie_monster),
        url_(url),
        callback_(callback) {
  }

  // CookieMonster::CookieMonsterTask:
  virtual void Run() override;

 protected:
  virtual ~DeleteAllForHostTask() {}

 private:
  GURL url_;
  CookieMonster::DeleteCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DeleteAllForHostTask);
};

void CookieMonster::DeleteAllForHostTask::Run() {
  int num_deleted = this->cookie_monster()->DeleteAllForHost(url_);
  if (!callback_.is_null()) {
    this->InvokeCallback(base::Bind(&CookieMonster::DeleteCallback::Run,
                                    base::Unretained(&callback_), num_deleted));
  }
}

// Task class for DeleteCanonicalCookie call.
class CookieMonster::DeleteCanonicalCookieTask
    : public CookieMonster::CookieMonsterTask {
 public:
  DeleteCanonicalCookieTask(
      CookieMonster* cookie_monster,
      const CanonicalCookie& cookie,
      const CookieMonster::DeleteCookieCallback& callback)
      : CookieMonsterTask(cookie_monster),
        cookie_(cookie),
        callback_(callback) {
  }

  // CookieMonster::CookieMonsterTask:
  virtual void Run() override;

 protected:
  virtual ~DeleteCanonicalCookieTask() {}

 private:
  CanonicalCookie cookie_;
  CookieMonster::DeleteCookieCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DeleteCanonicalCookieTask);
};

void CookieMonster::DeleteCanonicalCookieTask::Run() {
  bool result = this->cookie_monster()->DeleteCanonicalCookie(cookie_);
  if (!callback_.is_null()) {
    this->InvokeCallback(base::Bind(&CookieMonster::DeleteCookieCallback::Run,
                                    base::Unretained(&callback_), result));
  }
}

// Task class for SetCookieWithOptions call.
class CookieMonster::SetCookieWithOptionsTask
    : public CookieMonster::CookieMonsterTask {
 public:
  SetCookieWithOptionsTask(CookieMonster* cookie_monster,
                           const GURL& url,
                           const std::string& cookie_line,
                           const CookieOptions& options,
                           const CookieMonster::SetCookiesCallback& callback)
      : CookieMonsterTask(cookie_monster),
        url_(url),
        cookie_line_(cookie_line),
        options_(options),
        callback_(callback) {
  }

  // CookieMonster::CookieMonsterTask:
  virtual void Run() override;

 protected:
  virtual ~SetCookieWithOptionsTask() {}

 private:
  GURL url_;
  std::string cookie_line_;
  CookieOptions options_;
  CookieMonster::SetCookiesCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SetCookieWithOptionsTask);
};

void CookieMonster::SetCookieWithOptionsTask::Run() {
  bool result = this->cookie_monster()->
      SetCookieWithOptions(url_, cookie_line_, options_);
  if (!callback_.is_null()) {
    this->InvokeCallback(base::Bind(&CookieMonster::SetCookiesCallback::Run,
                                    base::Unretained(&callback_), result));
  }
}

// Task class for GetCookiesWithOptions call.
class CookieMonster::GetCookiesWithOptionsTask
    : public CookieMonster::CookieMonsterTask {
 public:
  GetCookiesWithOptionsTask(CookieMonster* cookie_monster,
                            const GURL& url,
                            const CookieOptions& options,
                            const CookieMonster::GetCookiesCallback& callback)
      : CookieMonsterTask(cookie_monster),
        url_(url),
        options_(options),
        callback_(callback) {
  }

  // CookieMonster::CookieMonsterTask:
  virtual void Run() override;

 protected:
  virtual ~GetCookiesWithOptionsTask() {}

 private:
  GURL url_;
  CookieOptions options_;
  CookieMonster::GetCookiesCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetCookiesWithOptionsTask);
};

void CookieMonster::GetCookiesWithOptionsTask::Run() {
  std::string cookie = this->cookie_monster()->
      GetCookiesWithOptions(url_, options_);
  if (!callback_.is_null()) {
    this->InvokeCallback(base::Bind(&CookieMonster::GetCookiesCallback::Run,
                                    base::Unretained(&callback_), cookie));
  }
}

// Task class for GetCookiesWithInfo call.
class CookieMonster::GetCookiesWithInfoTask
    : public CookieMonster::CookieMonsterTask {
 public:
  GetCookiesWithInfoTask(CookieMonster* cookie_monster,
                         const GURL& url,
                         const CookieOptions& options,
                         const CookieMonster::GetCookieInfoCallback& callback)
      : CookieMonsterTask(cookie_monster),
        url_(url),
        options_(options),
        callback_(callback) { }

  // CookieMonster::CookieMonsterTask:
  virtual void Run() override;

 protected:
  virtual ~GetCookiesWithInfoTask() {}

 private:
  GURL url_;
  CookieOptions options_;
  CookieMonster::GetCookieInfoCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetCookiesWithInfoTask);
};

void CookieMonster::GetCookiesWithInfoTask::Run() {
  if (!callback_.is_null()) {
    std::string cookie_line;
    std::vector<CookieMonster::CookieInfo> cookie_infos;
    this->cookie_monster()->
        GetCookiesWithInfo(url_, options_, &cookie_line, &cookie_infos);
    this->InvokeCallback(base::Bind(&CookieMonster::GetCookieInfoCallback::Run,
                                    base::Unretained(&callback_),
                                    cookie_line, cookie_infos));
  }
}

// Task class for DeleteCookie call.
class CookieMonster::DeleteCookieTask
    : public CookieMonster::CookieMonsterTask {
 public:
  DeleteCookieTask(CookieMonster* cookie_monster,
                   const GURL& url,
                   const std::string& cookie_name,
                   const base::Closure& callback)
      : CookieMonsterTask(cookie_monster),
        url_(url),
        cookie_name_(cookie_name),
        callback_(callback) { }

  // CookieMonster::CookieMonsterTask:
  virtual void Run() override;

 protected:
  virtual ~DeleteCookieTask() {}

 private:
  GURL url_;
  std::string cookie_name_;
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(DeleteCookieTask);
};

void CookieMonster::DeleteCookieTask::Run() {
  this->cookie_monster()->DeleteCookie(url_, cookie_name_);
  if (!callback_.is_null()) {
    this->InvokeCallback(callback_);
  }
}

// Task class for DeleteSessionCookies call.
class CookieMonster::DeleteSessionCookiesTask
    : public CookieMonster::CookieMonsterTask {
 public:
  DeleteSessionCookiesTask(
      CookieMonster* cookie_monster,
      const CookieMonster::DeleteCallback& callback)
      : CookieMonsterTask(cookie_monster),
        callback_(callback) {
  }

  // CookieMonster::CookieMonsterTask:
  virtual void Run() override;

 protected:
  virtual ~DeleteSessionCookiesTask() {}

 private:
  CookieMonster::DeleteCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DeleteSessionCookiesTask);
};

void CookieMonster::DeleteSessionCookiesTask::Run() {
  int num_deleted = this->cookie_monster()->DeleteSessionCookies();
  if (!callback_.is_null()) {
    this->InvokeCallback(base::Bind(&CookieMonster::DeleteCallback::Run,
                                    base::Unretained(&callback_), num_deleted));
  }
}

// Asynchronous CookieMonster API

void CookieMonster::SetCookieWithDetailsAsync(
    const GURL& url, const std::string& name, const std::string& value,
    const std::string& domain, const std::string& path,
    const base::Time& expiration_time, bool secure, bool http_only,
    const SetCookiesCallback& callback) {
  scoped_refptr<SetCookieWithDetailsTask> task =
      new SetCookieWithDetailsTask(this, url, name, value, domain, path,
                                   expiration_time, secure, http_only,
                                   callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::GetAllCookiesAsync(const GetCookieListCallback& callback) {
  scoped_refptr<GetAllCookiesTask> task =
      new GetAllCookiesTask(this, callback);

  DoCookieTask(task);
}


void CookieMonster::GetAllCookiesForURLWithOptionsAsync(
    const GURL& url,
    const CookieOptions& options,
    const GetCookieListCallback& callback) {
  scoped_refptr<GetAllCookiesForURLWithOptionsTask> task =
      new GetAllCookiesForURLWithOptionsTask(this, url, options, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::GetAllCookiesForURLAsync(
    const GURL& url, const GetCookieListCallback& callback) {
  CookieOptions options;
  options.set_include_httponly();
  scoped_refptr<GetAllCookiesForURLWithOptionsTask> task =
      new GetAllCookiesForURLWithOptionsTask(this, url, options, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::DeleteAllAsync(const DeleteCallback& callback) {
  scoped_refptr<DeleteAllTask> task =
      new DeleteAllTask(this, callback);

  DoCookieTask(task);
}

void CookieMonster::DeleteAllCreatedBetweenAsync(
    const Time& delete_begin, const Time& delete_end,
    const DeleteCallback& callback) {
  scoped_refptr<DeleteAllCreatedBetweenTask> task =
      new DeleteAllCreatedBetweenTask(this, delete_begin, delete_end,
                                      callback);

  DoCookieTask(task);
}

void CookieMonster::DeleteAllForHostAsync(
    const GURL& url, const DeleteCallback& callback) {
  scoped_refptr<DeleteAllForHostTask> task =
      new DeleteAllForHostTask(this, url, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::DeleteCanonicalCookieAsync(
    const CanonicalCookie& cookie,
    const DeleteCookieCallback& callback) {
  scoped_refptr<DeleteCanonicalCookieTask> task =
      new DeleteCanonicalCookieTask(this, cookie, callback);

  DoCookieTask(task);
}

void CookieMonster::SetCookieWithOptionsAsync(
    const GURL& url,
    const std::string& cookie_line,
    const CookieOptions& options,
    const SetCookiesCallback& callback) {
  scoped_refptr<SetCookieWithOptionsTask> task =
      new SetCookieWithOptionsTask(this, url, cookie_line, options, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::GetCookiesWithOptionsAsync(
    const GURL& url,
    const CookieOptions& options,
    const GetCookiesCallback& callback) {
  scoped_refptr<GetCookiesWithOptionsTask> task =
      new GetCookiesWithOptionsTask(this, url, options, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::GetCookiesWithInfoAsync(
    const GURL& url,
    const CookieOptions& options,
    const GetCookieInfoCallback& callback) {
  scoped_refptr<GetCookiesWithInfoTask> task =
      new GetCookiesWithInfoTask(this, url, options, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::DeleteCookieAsync(const GURL& url,
                                      const std::string& cookie_name,
                                      const base::Closure& callback) {
  scoped_refptr<DeleteCookieTask> task =
      new DeleteCookieTask(this, url, cookie_name, callback);

  DoCookieTaskForURL(task, url);
}

void CookieMonster::DeleteSessionCookiesAsync(
    const CookieStore::DeleteCallback& callback) {
  scoped_refptr<DeleteSessionCookiesTask> task =
      new DeleteSessionCookiesTask(this, callback);

  DoCookieTask(task);
}

void CookieMonster::DoCookieTask(
    const scoped_refptr<CookieMonsterTask>& task_item) {
  {
    base::AutoLock autolock(lock_);
    InitIfNecessary();
    if (!loaded_) {
      queue_.push(task_item);
      return;
    }
  }

  task_item->Run();
}

void CookieMonster::DoCookieTaskForURL(
    const scoped_refptr<CookieMonsterTask>& task_item,
    const GURL& url) {
  {
    base::AutoLock autolock(lock_);
    InitIfNecessary();
    // If cookies for the requested domain key (eTLD+1) have been loaded from DB
    // then run the task, otherwise load from DB.
    if (!loaded_) {
      // Checks if the domain key has been loaded.
      std::string key(cookie_util::GetEffectiveDomain(url.scheme(),
                                                       url.host()));
      if (keys_loaded_.find(key) == keys_loaded_.end()) {
        std::map<std::string, std::deque<scoped_refptr<CookieMonsterTask> > >
          ::iterator it = tasks_queued_.find(key);
        if (it == tasks_queued_.end()) {
          store_->LoadCookiesForKey(key,
            base::Bind(&CookieMonster::OnKeyLoaded, this, key));
          it = tasks_queued_.insert(std::make_pair(key,
            std::deque<scoped_refptr<CookieMonsterTask> >())).first;
        }
        it->second.push_back(task_item);
        return;
      }
    }
  }
  task_item->Run();
}

bool CookieMonster::SetCookieWithDetails(
    const GURL& url, const std::string& name, const std::string& value,
    const std::string& domain, const std::string& path,
    const base::Time& expiration_time, bool secure, bool http_only) {
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url))
    return false;

  Time creation_time = CurrentTime();
  last_time_seen_ = creation_time;

  // TODO(abarth): Take these values as parameters.
  std::string mac_key;
  std::string mac_algorithm;

  scoped_ptr<CanonicalCookie> cc;
  cc.reset(CanonicalCookie::Create(
      url, name, value, domain, path,
      mac_key, mac_algorithm,
      creation_time, expiration_time,
      secure, http_only));

  if (!cc.get())
    return false;

  CookieOptions options;
  options.set_include_httponly();
  return SetCanonicalCookie(&cc, creation_time, options);
}

bool CookieMonster::InitializeFrom(const CookieList& list) {
  base::AutoLock autolock(lock_);
  InitIfNecessary();
  for (net::CookieList::const_iterator iter = list.begin();
           iter != list.end(); ++iter) {
    scoped_ptr<CanonicalCookie> cookie(new CanonicalCookie(*iter));
    net::CookieOptions options;
    options.set_include_httponly();
    if (!SetCanonicalCookie(&cookie, cookie->CreationDate(), options))
      return false;
  }
  return true;
}

CookieList CookieMonster::GetAllCookies() {
  base::AutoLock autolock(lock_);

  // This function is being called to scrape the cookie list for management UI
  // or similar.  We shouldn't show expired cookies in this list since it will
  // just be confusing to users, and this function is called rarely enough (and
  // is already slow enough) that it's OK to take the time to garbage collect
  // the expired cookies now.
  //
  // Note that this does not prune cookies to be below our limits (if we've
  // exceeded them) the way that calling GarbageCollect() would.
  GarbageCollectExpired(Time::Now(),
                        CookieMapItPair(cookies_.begin(), cookies_.end()),
                        NULL);

  // Copy the CanonicalCookie pointers from the map so that we can use the same
  // sorter as elsewhere, then copy the result out.
  std::vector<CanonicalCookie*> cookie_ptrs;
  cookie_ptrs.reserve(cookies_.size());
  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end(); ++it)
    cookie_ptrs.push_back(it->second);
  std::sort(cookie_ptrs.begin(), cookie_ptrs.end(), CookieSorter);

  CookieList cookie_list;
  cookie_list.reserve(cookie_ptrs.size());
  for (std::vector<CanonicalCookie*>::const_iterator it = cookie_ptrs.begin();
       it != cookie_ptrs.end(); ++it)
    cookie_list.push_back(**it);

  return cookie_list;
}

CookieList CookieMonster::GetAllCookiesForURLWithOptions(
    const GURL& url,
    const CookieOptions& options) {
  base::AutoLock autolock(lock_);

  std::vector<CanonicalCookie*> cookie_ptrs;
  FindCookiesForHostAndDomain(url, options, false, &cookie_ptrs);
  std::sort(cookie_ptrs.begin(), cookie_ptrs.end(), CookieSorter);

  CookieList cookies;
  for (std::vector<CanonicalCookie*>::const_iterator it = cookie_ptrs.begin();
       it != cookie_ptrs.end(); it++)
    cookies.push_back(**it);

  return cookies;
}

CookieList CookieMonster::GetAllCookiesForURL(const GURL& url) {
  CookieOptions options;
  options.set_include_httponly();

  return GetAllCookiesForURLWithOptions(url, options);
}

int CookieMonster::DeleteAll(bool sync_to_store) {
  base::AutoLock autolock(lock_);

  int num_deleted = 0;
  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end();) {
    CookieMap::iterator curit = it;
    ++it;
    InternalDeleteCookie(curit, sync_to_store,
                         sync_to_store ? DELETE_COOKIE_EXPLICIT :
                             DELETE_COOKIE_DONT_RECORD /* Destruction. */);
    ++num_deleted;
  }

  return num_deleted;
}

int CookieMonster::DeleteAllCreatedBetween(const Time& delete_begin,
                                           const Time& delete_end) {
  base::AutoLock autolock(lock_);

  int num_deleted = 0;
  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end();) {
    CookieMap::iterator curit = it;
    CanonicalCookie* cc = curit->second;
    ++it;

    if (cc->CreationDate() >= delete_begin &&
        (delete_end.is_null() || cc->CreationDate() < delete_end)) {
      InternalDeleteCookie(curit,
                           true,  /*sync_to_store*/
                           DELETE_COOKIE_EXPLICIT);
      ++num_deleted;
    }
  }

  return num_deleted;
}

int CookieMonster::DeleteAllForHost(const GURL& url) {
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url))
    return 0;

  const std::string host(url.host());

  // We store host cookies in the store by their canonical host name;
  // domain cookies are stored with a leading ".".  So this is a pretty
  // simple lookup and per-cookie delete.
  int num_deleted = 0;
  for (CookieMapItPair its = cookies_.equal_range(GetKey(host));
       its.first != its.second;) {
    CookieMap::iterator curit = its.first;
    ++its.first;

    const CanonicalCookie* const cc = curit->second;

    // Delete only on a match as a host cookie.
    if (cc->IsHostCookie() && cc->IsDomainMatch(host)) {
      num_deleted++;

      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPLICIT);
    }
  }
  return num_deleted;
}

bool CookieMonster::DeleteCanonicalCookie(const CanonicalCookie& cookie) {
  base::AutoLock autolock(lock_);

  for (CookieMapItPair its = cookies_.equal_range(GetKey(cookie.Domain()));
       its.first != its.second; ++its.first) {
    // The creation date acts as our unique index...
    if (its.first->second->CreationDate() == cookie.CreationDate()) {
      InternalDeleteCookie(its.first, true, DELETE_COOKIE_EXPLICIT);
      return true;
    }
  }
  return false;
}

void CookieMonster::SetCookieableSchemes(
    const char* schemes[], size_t num_schemes) {
  base::AutoLock autolock(lock_);

  // Cookieable Schemes must be set before first use of function.
  DCHECK(!initialized_);

  cookieable_schemes_.clear();
  cookieable_schemes_.insert(cookieable_schemes_.end(),
                             schemes, schemes + num_schemes);
}

void CookieMonster::SetEnableFileScheme(bool accept) {
  // This assumes "file" is always at the end of the array. See the comment
  // above kDefaultCookieableSchemes.
  int num_schemes = accept ? kDefaultCookieableSchemesCount :
      kDefaultCookieableSchemesCount - 1;
  SetCookieableSchemes(kDefaultCookieableSchemes, num_schemes);
}

void CookieMonster::SetKeepExpiredCookies() {
  keep_expired_cookies_ = true;
}

// static
void CookieMonster::EnableFileScheme() {
  default_enable_file_scheme_ = true;
}

void CookieMonster::FlushStore(const base::Closure& callback) {
  base::AutoLock autolock(lock_);
  if (initialized_ && store_)
    store_->Flush(callback);
  else if (!callback.is_null())
    MessageLoop::current()->PostTask(FROM_HERE, callback);
}

bool CookieMonster::SetCookieWithOptions(const GURL& url,
                                         const std::string& cookie_line,
                                         const CookieOptions& options) {
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url)) {
    return false;
  }

  return SetCookieWithCreationTimeAndOptions(url, cookie_line, Time(), options);
}

std::string CookieMonster::GetCookiesWithOptions(const GURL& url,
                                                 const CookieOptions& options) {
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url))
    return std::string();

  TimeTicks start_time(TimeTicks::Now());

  std::vector<CanonicalCookie*> cookies;
  FindCookiesForHostAndDomain(url, options, true, &cookies);
  std::sort(cookies.begin(), cookies.end(), CookieSorter);

  std::string cookie_line = BuildCookieLine(cookies);

  histogram_time_get_->AddTime(TimeTicks::Now() - start_time);

  VLOG(kVlogGetCookies) << "GetCookies() result: " << cookie_line;

  return cookie_line;
}

void CookieMonster::GetCookiesWithInfo(const GURL& url,
                                       const CookieOptions& options,
                                       std::string* cookie_line,
                                       std::vector<CookieInfo>* cookie_infos) {
  DCHECK(cookie_line->empty());
  DCHECK(cookie_infos->empty());

  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url))
    return;

  TimeTicks start_time(TimeTicks::Now());

  std::vector<CanonicalCookie*> cookies;
  FindCookiesForHostAndDomain(url, options, true, &cookies);
  std::sort(cookies.begin(), cookies.end(), CookieSorter);
  *cookie_line = BuildCookieLine(cookies);

  histogram_time_get_->AddTime(TimeTicks::Now() - start_time);

  TimeTicks mac_start_time = TimeTicks::Now();
  BuildCookieInfoList(cookies, cookie_infos);
  histogram_time_mac_->AddTime(TimeTicks::Now() - mac_start_time);
}

void CookieMonster::DeleteCookie(const GURL& url,
                                 const std::string& cookie_name) {
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url))
    return;

  CookieOptions options;
  options.set_include_httponly();
  // Get the cookies for this host and its domain(s).
  std::vector<CanonicalCookie*> cookies;
  FindCookiesForHostAndDomain(url, options, true, &cookies);
  std::set<CanonicalCookie*> matching_cookies;

  for (std::vector<CanonicalCookie*>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    if ((*it)->Name() != cookie_name)
      continue;
    if (url.path().find((*it)->Path()))
      continue;
    matching_cookies.insert(*it);
  }

  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end();) {
    CookieMap::iterator curit = it;
    ++it;
    if (matching_cookies.find(curit->second) != matching_cookies.end()) {
      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPLICIT);
    }
  }
}

int CookieMonster::DeleteSessionCookies() {
  base::AutoLock autolock(lock_);

  int num_deleted = 0;
  for (CookieMap::iterator it = cookies_.begin(); it != cookies_.end();) {
    CookieMap::iterator curit = it;
    CanonicalCookie* cc = curit->second;
    ++it;

    if (!cc->IsPersistent()) {
      InternalDeleteCookie(curit,
                           true,  /*sync_to_store*/
                           DELETE_COOKIE_EXPIRED);
      ++num_deleted;
    }
  }

  return num_deleted;
}

CookieMonster* CookieMonster::GetCookieMonster() {
  return this;
}

void CookieMonster::SetPersistSessionCookies(bool persist_session_cookies) {
  // This function must be called before the CookieMonster is used.
  DCHECK(!initialized_);
  persist_session_cookies_ = persist_session_cookies;
}

void CookieMonster::SetForceKeepSessionState() {
  if (store_) {
    store_->SetForceKeepSessionState();
  }
}

CookieMonster::~CookieMonster() {
  DeleteAll(false);
}

bool CookieMonster::SetCookieWithCreationTime(const GURL& url,
                                              const std::string& cookie_line,
                                              const base::Time& creation_time) {
  DCHECK(!store_) << "This method is only to be used by unit-tests.";
  base::AutoLock autolock(lock_);

  if (!HasCookieableScheme(url)) {
    return false;
  }

  InitIfNecessary();
  return SetCookieWithCreationTimeAndOptions(url, cookie_line, creation_time,
                                             CookieOptions());
}

void CookieMonster::InitStore() {
  DCHECK(store_) << "Store must exist to initialize";

  // We bind in the current time so that we can report the wall-clock time for
  // loading cookies.
  store_->Load(base::Bind(&CookieMonster::OnLoaded, this, TimeTicks::Now()));
}

void CookieMonster::OnLoaded(TimeTicks beginning_time,
                             const std::vector<CanonicalCookie*>& cookies) {
  StoreLoadedCookies(cookies);
  histogram_time_blocked_on_load_->AddTime(TimeTicks::Now() - beginning_time);

  // Invoke the task queue of cookie request.
  InvokeQueue();
}

void CookieMonster::OnKeyLoaded(const std::string& key,
                                const std::vector<CanonicalCookie*>& cookies) {
  // This function does its own separate locking.
  StoreLoadedCookies(cookies);

  std::deque<scoped_refptr<CookieMonsterTask> > tasks_queued;
  {
    base::AutoLock autolock(lock_);
    keys_loaded_.insert(key);
    std::map<std::string, std::deque<scoped_refptr<CookieMonsterTask> > >
      ::iterator it = tasks_queued_.find(key);
    if (it == tasks_queued_.end())
      return;
    it->second.swap(tasks_queued);
    tasks_queued_.erase(it);
  }

  while (!tasks_queued.empty()) {
    scoped_refptr<CookieMonsterTask> task = tasks_queued.front();
    task->Run();
    tasks_queued.pop_front();
  }
}

void CookieMonster::StoreLoadedCookies(
    const std::vector<CanonicalCookie*>& cookies) {
  // Initialize the store and sync in any saved persistent cookies.  We don't
  // care if it's expired, insert it so it can be garbage collected, removed,
  // and sync'd.
  base::AutoLock autolock(lock_);

  for (std::vector<CanonicalCookie*>::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    int64 cookie_creation_time = (*it)->CreationDate().ToInternalValue();

    if (creation_times_.insert(cookie_creation_time).second) {
      InternalInsertCookie(GetKey((*it)->Domain()), *it, false);
      const Time cookie_access_time((*it)->LastAccessDate());
      if (earliest_access_time_.is_null() ||
          cookie_access_time < earliest_access_time_)
        earliest_access_time_ = cookie_access_time;
    } else {
      LOG(ERROR) << base::StringPrintf("Found cookies with duplicate creation "
                                       "times in backing store: "
                                       "{name='%s', domain='%s', path='%s'}",
                                       (*it)->Name().c_str(),
                                       (*it)->Domain().c_str(),
                                       (*it)->Path().c_str());
      // We've been given ownership of the cookie and are throwing it
      // away; reclaim the space.
      delete (*it);
    }
  }

  // After importing cookies from the PersistentCookieStore, verify that
  // none of our other constraints are violated.
  // In particular, the backing store might have given us duplicate cookies.

  // This method could be called multiple times due to priority loading, thus
  // cookies loaded in previous runs will be validated again, but this is OK
  // since they are expected to be much fewer than total DB.
  EnsureCookiesMapIsValid();
}

void CookieMonster::InvokeQueue() {
  while (true) {
    scoped_refptr<CookieMonsterTask> request_task;
    {
      base::AutoLock autolock(lock_);
      if (queue_.empty()) {
        loaded_ = true;
        creation_times_.clear();
        keys_loaded_.clear();
        break;
      }
      request_task = queue_.front();
      queue_.pop();
    }
    request_task->Run();
  }
}

void CookieMonster::EnsureCookiesMapIsValid() {
  lock_.AssertAcquired();

  int num_duplicates_trimmed = 0;

  // Iterate through all the of the cookies, grouped by host.
  CookieMap::iterator prev_range_end = cookies_.begin();
  while (prev_range_end != cookies_.end()) {
    CookieMap::iterator cur_range_begin = prev_range_end;
    const std::string key = cur_range_begin->first;  // Keep a copy.
    CookieMap::iterator cur_range_end = cookies_.upper_bound(key);
    prev_range_end = cur_range_end;

    // Ensure no equivalent cookies for this host.
    num_duplicates_trimmed +=
        TrimDuplicateCookiesForKey(key, cur_range_begin, cur_range_end);
  }

  // Record how many duplicates were found in the database.
  // See InitializeHistograms() for details.
  histogram_cookie_deletion_cause_->Add(num_duplicates_trimmed);
}

int CookieMonster::TrimDuplicateCookiesForKey(
    const std::string& key,
    CookieMap::iterator begin,
    CookieMap::iterator end) {
  lock_.AssertAcquired();

  // Set of cookies ordered by creation time.
  typedef std::set<CookieMap::iterator, OrderByCreationTimeDesc> CookieSet;

  // Helper map we populate to find the duplicates.
  typedef std::map<CookieSignature, CookieSet> EquivalenceMap;
  EquivalenceMap equivalent_cookies;

  // The number of duplicate cookies that have been found.
  int num_duplicates = 0;

  // Iterate through all of the cookies in our range, and insert them into
  // the equivalence map.
  for (CookieMap::iterator it = begin; it != end; ++it) {
    DCHECK_EQ(key, it->first);
    CanonicalCookie* cookie = it->second;

    CookieSignature signature(cookie->Name(), cookie->Domain(),
                              cookie->Path());
    CookieSet& set = equivalent_cookies[signature];

    // We found a duplicate!
    if (!set.empty())
      num_duplicates++;

    // We save the iterator into |cookies_| rather than the actual cookie
    // pointer, since we may need to delete it later.
    bool insert_success = set.insert(it).second;
    DCHECK(insert_success) <<
        "Duplicate creation times found in duplicate cookie name scan.";
  }

  // If there were no duplicates, we are done!
  if (num_duplicates == 0)
    return 0;

  // Make sure we find everything below that we did above.
  int num_duplicates_found = 0;

  // Otherwise, delete all the duplicate cookies, both from our in-memory store
  // and from the backing store.
  for (EquivalenceMap::iterator it = equivalent_cookies.begin();
       it != equivalent_cookies.end();
       ++it) {
    const CookieSignature& signature = it->first;
    CookieSet& dupes = it->second;

    if (dupes.size() <= 1)
      continue;  // This cookiename/path has no duplicates.
    num_duplicates_found += dupes.size() - 1;

    // Since |dups| is sorted by creation time (descending), the first cookie
    // is the most recent one, so we will keep it. The rest are duplicates.
    dupes.erase(dupes.begin());

    LOG(ERROR) << base::StringPrintf(
        "Found %d duplicate cookies for host='%s', "
        "with {name='%s', domain='%s', path='%s'}",
        static_cast<int>(dupes.size()),
        key.c_str(),
        signature.name.c_str(),
        signature.domain.c_str(),
        signature.path.c_str());

    // Remove all the cookies identified by |dupes|. It is valid to delete our
    // list of iterators one at a time, since |cookies_| is a multimap (they
    // don't invalidate existing iterators following deletion).
    for (CookieSet::iterator dupes_it = dupes.begin();
         dupes_it != dupes.end();
         ++dupes_it) {
      InternalDeleteCookie(*dupes_it, true,
                           DELETE_COOKIE_DUPLICATE_IN_BACKING_STORE);
    }
  }
  DCHECK_EQ(num_duplicates, num_duplicates_found);

  return num_duplicates;
}

// Note: file must be the last scheme.
const char* CookieMonster::kDefaultCookieableSchemes[] =
    { "http", "https", "file" };
const int CookieMonster::kDefaultCookieableSchemesCount =
    arraysize(CookieMonster::kDefaultCookieableSchemes);

void CookieMonster::SetDefaultCookieableSchemes() {
  int num_schemes = default_enable_file_scheme_ ?
      kDefaultCookieableSchemesCount : kDefaultCookieableSchemesCount - 1;
  SetCookieableSchemes(kDefaultCookieableSchemes, num_schemes);
}


void CookieMonster::FindCookiesForHostAndDomain(
    const GURL& url,
    const CookieOptions& options,
    bool update_access_time,
    std::vector<CanonicalCookie*>* cookies) {
  lock_.AssertAcquired();

  const Time current_time(CurrentTime());

  // Probe to save statistics relatively frequently.  We do it here rather
  // than in the set path as many websites won't set cookies, and we
  // want to collect statistics whenever the browser's being used.
  RecordPeriodicStats(current_time);

  // Can just dispatch to FindCookiesForKey
  const std::string key(GetKey(url.host()));
  FindCookiesForKey(key, url, options, current_time,
                    update_access_time, cookies);
}

void CookieMonster::FindCookiesForKey(
    const std::string& key,
    const GURL& url,
    const CookieOptions& options,
    const Time& current,
    bool update_access_time,
    std::vector<CanonicalCookie*>* cookies) {
  lock_.AssertAcquired();

  for (CookieMapItPair its = cookies_.equal_range(key);
       its.first != its.second; ) {
    CookieMap::iterator curit = its.first;
    CanonicalCookie* cc = curit->second;
    ++its.first;

    // If the cookie is expired, delete it.
    if (cc->IsExpired(current) && !keep_expired_cookies_) {
      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPIRED);
      continue;
    }

    // Filter out cookies that should not be included for a request to the
    // given |url|. HTTP only cookies are filtered depending on the passed
    // cookie |options|.
    if (!cc->IncludeForRequestURL(url, options))
      continue;

    // Add this cookie to the set of matching cookies. Update the access
    // time if we've been requested to do so.
    if (update_access_time) {
      InternalUpdateCookieAccessTime(cc, current);
    }
    cookies->push_back(cc);
  }
}

bool CookieMonster::DeleteAnyEquivalentCookie(const std::string& key,
                                              const CanonicalCookie& ecc,
                                              bool skip_httponly,
                                              bool already_expired) {
  lock_.AssertAcquired();

  bool found_equivalent_cookie = false;
  bool skipped_httponly = false;
  for (CookieMapItPair its = cookies_.equal_range(key);
       its.first != its.second; ) {
    CookieMap::iterator curit = its.first;
    CanonicalCookie* cc = curit->second;
    ++its.first;

    if (ecc.IsEquivalent(*cc)) {
      // We should never have more than one equivalent cookie, since they should
      // overwrite each other.
      CHECK(!found_equivalent_cookie) <<
          "Duplicate equivalent cookies found, cookie store is corrupted.";
      if (skip_httponly && cc->IsHttpOnly()) {
        skipped_httponly = true;
      } else {
        InternalDeleteCookie(curit, true, already_expired ?
            DELETE_COOKIE_EXPIRED_OVERWRITE : DELETE_COOKIE_OVERWRITE);
      }
      found_equivalent_cookie = true;
    }
  }
  return skipped_httponly;
}

void CookieMonster::InternalInsertCookie(const std::string& key,
                                         CanonicalCookie* cc,
                                         bool sync_to_store) {
  lock_.AssertAcquired();

  if ((cc->IsPersistent() || persist_session_cookies_) &&
      store_ && sync_to_store)
    store_->AddCookie(*cc);
  cookies_.insert(CookieMap::value_type(key, cc));
  if (delegate_.get()) {
    delegate_->OnCookieChanged(
        *cc, false, CookieMonster::Delegate::CHANGE_COOKIE_EXPLICIT);
  }
}

bool CookieMonster::SetCookieWithCreationTimeAndOptions(
    const GURL& url,
    const std::string& cookie_line,
    const Time& creation_time_or_null,
    const CookieOptions& options) {
  lock_.AssertAcquired();

  VLOG(kVlogSetCookies) << "SetCookie() line: " << cookie_line;

  Time creation_time = creation_time_or_null;
  if (creation_time.is_null()) {
    creation_time = CurrentTime();
    last_time_seen_ = creation_time;
  }

  scoped_ptr<CanonicalCookie> cc(
      CanonicalCookie::Create(url, cookie_line, creation_time, options));

  if (!cc.get()) {
    VLOG(kVlogSetCookies) << "WARNING: Failed to allocate CanonicalCookie";
    return false;
  }
  return SetCanonicalCookie(&cc, creation_time, options);
}

bool CookieMonster::SetCanonicalCookie(scoped_ptr<CanonicalCookie>* cc,
                                       const Time& creation_time,
                                       const CookieOptions& options) {
  const std::string key(GetKey((*cc)->Domain()));
  bool already_expired = (*cc)->IsExpired(creation_time);
  if (DeleteAnyEquivalentCookie(key, **cc, options.exclude_httponly(),
                                already_expired)) {
    VLOG(kVlogSetCookies) << "SetCookie() not clobbering httponly cookie";
    return false;
  }

  VLOG(kVlogSetCookies) << "SetCookie() key: " << key << " cc: "
                        << (*cc)->DebugString();

  // Realize that we might be setting an expired cookie, and the only point
  // was to delete the cookie which we've already done.
  if (!already_expired || keep_expired_cookies_) {
    // See InitializeHistograms() for details.
    if ((*cc)->IsPersistent()) {
      histogram_expiration_duration_minutes_->Add(
          ((*cc)->ExpiryDate() - creation_time).InMinutes());
    }

    InternalInsertCookie(key, cc->release(), true);
  }

  // We assume that hopefully setting a cookie will be less common than
  // querying a cookie.  Since setting a cookie can put us over our limits,
  // make sure that we garbage collect...  We can also make the assumption that
  // if a cookie was set, in the common case it will be used soon after,
  // and we will purge the expired cookies in GetCookies().
  GarbageCollect(creation_time, key);

  return true;
}

void CookieMonster::InternalUpdateCookieAccessTime(CanonicalCookie* cc,
                                                   const Time& current) {
  lock_.AssertAcquired();

  // Based off the Mozilla code.  When a cookie has been accessed recently,
  // don't bother updating its access time again.  This reduces the number of
  // updates we do during pageload, which in turn reduces the chance our storage
  // backend will hit its batch thresholds and be forced to update.
  if ((current - cc->LastAccessDate()) < last_access_threshold_)
    return;

  // See InitializeHistograms() for details.
  histogram_between_access_interval_minutes_->Add(
      (current - cc->LastAccessDate()).InMinutes());

  cc->SetLastAccessDate(current);
  if ((cc->IsPersistent() || persist_session_cookies_) && store_)
    store_->UpdateCookieAccessTime(*cc);
}

void CookieMonster::InternalDeleteCookie(CookieMap::iterator it,
                                         bool sync_to_store,
                                         DeletionCause deletion_cause) {
  lock_.AssertAcquired();

  // Ideally, this would be asserted up where we define ChangeCauseMapping,
  // but DeletionCause's visibility (or lack thereof) forces us to make
  // this check here.
  COMPILE_ASSERT(arraysize(ChangeCauseMapping) == DELETE_COOKIE_LAST_ENTRY + 1,
                 ChangeCauseMapping_size_not_eq_DeletionCause_enum_size);

  // See InitializeHistograms() for details.
  if (deletion_cause != DELETE_COOKIE_DONT_RECORD)
    histogram_cookie_deletion_cause_->Add(deletion_cause);

  CanonicalCookie* cc = it->second;
  VLOG(kVlogSetCookies) << "InternalDeleteCookie() cc: " << cc->DebugString();

  if ((cc->IsPersistent() || persist_session_cookies_)
      && store_ && sync_to_store)
    store_->DeleteCookie(*cc);
  if (delegate_.get()) {
    ChangeCausePair mapping = ChangeCauseMapping[deletion_cause];

    if (mapping.notify)
      delegate_->OnCookieChanged(*cc, true, mapping.cause);
  }
  cookies_.erase(it);
  delete cc;
}

// Domain expiry behavior is unchanged by key/expiry scheme (the
// meaning of the key is different, but that's not visible to this
// routine).
int CookieMonster::GarbageCollect(const Time& current,
                                  const std::string& key) {
  lock_.AssertAcquired();

  int num_deleted = 0;

  // Collect garbage for this key.
  if (cookies_.count(key) > kDomainMaxCookies) {
    VLOG(kVlogGarbageCollection) << "GarbageCollect() key: " << key;

    std::vector<CookieMap::iterator> cookie_its;
    num_deleted += GarbageCollectExpired(
        current, cookies_.equal_range(key), &cookie_its);
    base::Time oldest_removed;
    if (FindLeastRecentlyAccessed(kDomainMaxCookies, kDomainPurgeCookies,
                                  &oldest_removed, &cookie_its)) {
      // Delete in two passes so we can figure out what we're nuking
      // that would be kept at the global level.
      int num_subject_to_global_purge =
          GarbageCollectDeleteList(
              current,
              Time::Now() - TimeDelta::FromDays(kSafeFromGlobalPurgeDays),
              DELETE_COOKIE_EVICTED_DOMAIN_PRE_SAFE,
              cookie_its);
      num_deleted += num_subject_to_global_purge;
      // Correct because FindLeastRecentlyAccessed returns a sorted list.
      cookie_its.erase(cookie_its.begin(),
                       cookie_its.begin() + num_subject_to_global_purge);
      num_deleted +=
          GarbageCollectDeleteList(
              current,
              Time(),
              DELETE_COOKIE_EVICTED_DOMAIN_POST_SAFE,
              cookie_its);
    }
  }

  // Collect garbage for everything.  With firefox style we want to
  // preserve cookies touched in kSafeFromGlobalPurgeDays, otherwise
  // not.
  if (cookies_.size() > kMaxCookies &&
      (earliest_access_time_ <
       Time::Now() - TimeDelta::FromDays(kSafeFromGlobalPurgeDays))) {
    VLOG(kVlogGarbageCollection) << "GarbageCollect() everything";
    std::vector<CookieMap::iterator> cookie_its;
    base::Time oldest_left;
    num_deleted += GarbageCollectExpired(
        current, CookieMapItPair(cookies_.begin(), cookies_.end()),
        &cookie_its);
    if (FindLeastRecentlyAccessed(kMaxCookies, kPurgeCookies,
                                  &oldest_left, &cookie_its)) {
      Time oldest_safe_cookie(
          (Time::Now() - TimeDelta::FromDays(kSafeFromGlobalPurgeDays)));
      int num_evicted = GarbageCollectDeleteList(
          current,
          oldest_safe_cookie,
          DELETE_COOKIE_EVICTED_GLOBAL,
          cookie_its);

      // If no cookies were preserved by the time limit, the global last
      // access is set to the value returned from FindLeastRecentlyAccessed.
      // If the time limit preserved some cookies, we use the last access of
      // the oldest preserved cookie.
      if (num_evicted == static_cast<int>(cookie_its.size())) {
        earliest_access_time_ = oldest_left;
      } else {
        earliest_access_time_ =
            (*(cookie_its.begin() + num_evicted))->second->LastAccessDate();
      }
      num_deleted += num_evicted;
    }
  }

  return num_deleted;
}

int CookieMonster::GarbageCollectExpired(
    const Time& current,
    const CookieMapItPair& itpair,
    std::vector<CookieMap::iterator>* cookie_its) {
  if (keep_expired_cookies_)
    return 0;

  lock_.AssertAcquired();

  int num_deleted = 0;
  for (CookieMap::iterator it = itpair.first, end = itpair.second; it != end;) {
    CookieMap::iterator curit = it;
    ++it;

    if (curit->second->IsExpired(current)) {
      InternalDeleteCookie(curit, true, DELETE_COOKIE_EXPIRED);
      ++num_deleted;
    } else if (cookie_its) {
      cookie_its->push_back(curit);
    }
  }

  return num_deleted;
}

int CookieMonster::GarbageCollectDeleteList(
    const Time& current,
    const Time& keep_accessed_after,
    DeletionCause cause,
    std::vector<CookieMap::iterator>& cookie_its) {
  int num_deleted = 0;
  for (std::vector<CookieMap::iterator>::iterator it = cookie_its.begin();
       it != cookie_its.end(); it++) {
    if (keep_accessed_after.is_null() ||
        (*it)->second->LastAccessDate() < keep_accessed_after) {
      histogram_evicted_last_access_minutes_->Add(
          (current - (*it)->second->LastAccessDate()).InMinutes());
      InternalDeleteCookie((*it), true, cause);
      num_deleted++;
    }
  }
  return num_deleted;
}

// A wrapper around RegistryControlledDomainService::GetDomainAndRegistry
// to make clear we're creating a key for our local map.  Here and
// in FindCookiesForHostAndDomain() are the only two places where
// we need to conditionalize based on key type.
//
// Note that this key algorithm explicitly ignores the scheme.  This is
// because when we're entering cookies into the map from the backing store,
// we in general won't have the scheme at that point.
// In practical terms, this means that file cookies will be stored
// in the map either by an empty string or by UNC name (and will be
// limited by kMaxCookiesPerHost), and extension cookies will be stored
// based on the single extension id, as the extension id won't have the
// form of a DNS host and hence GetKey() will return it unchanged.
//
// Arguably the right thing to do here is to make the key
// algorithm dependent on the scheme, and make sure that the scheme is
// available everywhere the key must be obtained (specfically at backing
// store load time).  This would require either changing the backing store
// database schema to include the scheme (far more trouble than it's worth), or
// separating out file cookies into their own CookieMonster instance and
// thus restricting each scheme to a single cookie monster (which might
// be worth it, but is still too much trouble to solve what is currently a
// non-problem).
std::string CookieMonster::GetKey(const std::string& domain) const {
  std::string effective_domain(
      RegistryControlledDomainService::GetDomainAndRegistry(domain));
  if (effective_domain.empty())
    effective_domain = domain;

  if (!effective_domain.empty() && effective_domain[0] == '.')
    return effective_domain.substr(1);
  return effective_domain;
}

bool CookieMonster::IsCookieableScheme(const std::string& scheme) {
  base::AutoLock autolock(lock_);

  return std::find(cookieable_schemes_.begin(), cookieable_schemes_.end(),
                   scheme) != cookieable_schemes_.end();
}

bool CookieMonster::HasCookieableScheme(const GURL& url) {
  lock_.AssertAcquired();

  // Make sure the request is on a cookie-able url scheme.
  for (size_t i = 0; i < cookieable_schemes_.size(); ++i) {
    // We matched a scheme.
    if (url.SchemeIs(cookieable_schemes_[i].c_str())) {
      // We've matched a supported scheme.
      return true;
    }
  }

  // The scheme didn't match any in our whitelist.
  VLOG(kVlogPerCookieMonster) << "WARNING: Unsupported cookie scheme: "
                              << url.scheme();
  return false;
}

// Test to see if stats should be recorded, and record them if so.
// The goal here is to get sampling for the average browser-hour of
// activity.  We won't take samples when the web isn't being surfed,
// and when the web is being surfed, we'll take samples about every
// kRecordStatisticsIntervalSeconds.
// last_statistic_record_time_ is initialized to Now() rather than null
// in the constructor so that we won't take statistics right after
// startup, to avoid bias from browsers that are started but not used.
void CookieMonster::RecordPeriodicStats(const base::Time& current_time) {
  const base::TimeDelta kRecordStatisticsIntervalTime(
      base::TimeDelta::FromSeconds(kRecordStatisticsIntervalSeconds));

  // If we've taken statistics recently, return.
  if (current_time - last_statistic_record_time_ <=
      kRecordStatisticsIntervalTime) {
    return;
  }

  // See InitializeHistograms() for details.
  histogram_count_->Add(cookies_.size());

  // More detailed statistics on cookie counts at different granularities.
  TimeTicks beginning_of_time(TimeTicks::Now());

  for (CookieMap::const_iterator it_key = cookies_.begin();
       it_key != cookies_.end(); ) {
    const std::string& key(it_key->first);

    int key_count = 0;
    typedef std::map<std::string, unsigned int> DomainMap;
    DomainMap domain_map;
    CookieMapItPair its_cookies = cookies_.equal_range(key);
    while (its_cookies.first != its_cookies.second) {
      key_count++;
      const std::string& cookie_domain(its_cookies.first->second->Domain());
      domain_map[cookie_domain]++;

      its_cookies.first++;
    }
    histogram_etldp1_count_->Add(key_count);
    histogram_domain_per_etldp1_count_->Add(domain_map.size());
    for (DomainMap::const_iterator domain_map_it = domain_map.begin();
         domain_map_it != domain_map.end(); domain_map_it++)
      histogram_domain_count_->Add(domain_map_it->second);

    it_key = its_cookies.second;
  }

  VLOG(kVlogPeriodic)
      << "Time for recording cookie stats (us): "
      << (TimeTicks::Now() - beginning_of_time).InMicroseconds();

  last_statistic_record_time_ = current_time;
}

// Initialize all histogram counter variables used in this class.
//
// Normal histogram usage involves using the macros defined in
// histogram.h, which automatically takes care of declaring these
// variables (as statics), initializing them, and accumulating into
// them, all from a single entry point.  Unfortunately, that solution
// doesn't work for the CookieMonster, as it's vulnerable to races between
// separate threads executing the same functions and hence initializing the
// same static variables.  There isn't a race danger in the histogram
// accumulation calls; they are written to be resilient to simultaneous
// calls from multiple threads.
//
// The solution taken here is to have per-CookieMonster instance
// variables that are constructed during CookieMonster construction.
// Note that these variables refer to the same underlying histogram,
// so we still race (but safely) with other CookieMonster instances
// for accumulation.
//
// To do this we've expanded out the individual histogram macros calls,
// with declarations of the variables in the class decl, initialization here
// (done from the class constructor) and direct calls to the accumulation
// methods where needed.  The specific histogram macro calls on which the
// initialization is based are included in comments below.
void CookieMonster::InitializeHistograms() {
  // From UMA_HISTOGRAM_CUSTOM_COUNTS
  histogram_expiration_duration_minutes_ = base::Histogram::FactoryGet(
      "Cookie.ExpirationDurationMinutes",
      1, kMinutesInTenYears, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_between_access_interval_minutes_ = base::Histogram::FactoryGet(
      "Cookie.BetweenAccessIntervalMinutes",
      1, kMinutesInTenYears, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_evicted_last_access_minutes_ = base::Histogram::FactoryGet(
      "Cookie.EvictedLastAccessMinutes",
      1, kMinutesInTenYears, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_count_ = base::Histogram::FactoryGet(
      "Cookie.Count", 1, 4000, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_domain_count_ = base::Histogram::FactoryGet(
      "Cookie.DomainCount", 1, 4000, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_etldp1_count_ = base::Histogram::FactoryGet(
      "Cookie.Etldp1Count", 1, 4000, 50,
      base::Histogram::kUmaTargetedHistogramFlag);
  histogram_domain_per_etldp1_count_ = base::Histogram::FactoryGet(
      "Cookie.DomainPerEtldp1Count", 1, 4000, 50,
      base::Histogram::kUmaTargetedHistogramFlag);

  // From UMA_HISTOGRAM_COUNTS_10000 & UMA_HISTOGRAM_CUSTOM_COUNTS
  histogram_number_duplicate_db_cookies_ = base::Histogram::FactoryGet(
      "Net.NumDuplicateCookiesInDb", 1, 10000, 50,
      base::Histogram::kUmaTargetedHistogramFlag);

  // From UMA_HISTOGRAM_ENUMERATION
  histogram_cookie_deletion_cause_ = base::LinearHistogram::FactoryGet(
      "Cookie.DeletionCause", 1,
      DELETE_COOKIE_LAST_ENTRY - 1, DELETE_COOKIE_LAST_ENTRY,
      base::Histogram::kUmaTargetedHistogramFlag);

  // From UMA_HISTOGRAM_{CUSTOM_,}TIMES
  histogram_time_get_ = base::Histogram::FactoryTimeGet("Cookie.TimeGet",
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(1),
      50, base::Histogram::kUmaTargetedHistogramFlag);
  histogram_time_mac_ = base::Histogram::FactoryTimeGet("Cookie.TimeGetMac",
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(1),
      50, base::Histogram::kUmaTargetedHistogramFlag);
  histogram_time_blocked_on_load_ = base::Histogram::FactoryTimeGet(
      "Cookie.TimeBlockedOnLoad",
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(1),
      50, base::Histogram::kUmaTargetedHistogramFlag);
}


// The system resolution is not high enough, so we can have multiple
// set cookies that result in the same system time.  When this happens, we
// increment by one Time unit.  Let's hope computers don't get too fast.
Time CookieMonster::CurrentTime() {
  return std::max(Time::Now(),
      Time::FromInternalValue(last_time_seen_.ToInternalValue() + 1));
}

}  // namespace net
