/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/network/persistent_cookie_store.h"

#include <vector>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "sql/connection.h"
#include "sql/statement.h"

namespace cobalt {
namespace network {

namespace {
const int kOriginalCookieSchemaVersion = 1;
const int kLatestCookieSchemaVersion = 1;
const base::TimeDelta kMaxCookieLifetime = base::TimeDelta::FromDays(365 * 2);

std::vector<net::CanonicalCookie*> GetAllCookies(sql::Connection* conn) {
  base::Time maximum_expiry = base::Time::Now() + kMaxCookieLifetime;

  std::vector<net::CanonicalCookie*> actual_cookies;
  sql::Statement get_all(conn->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT url, name, value, domain, path, mac_key, mac_algorithm, "
      "creation, expiration, last_access, secure, http_only "
      "FROM CookieTable"));
  while (get_all.Step()) {
    base::Time expiry = base::Time::FromInternalValue(get_all.ColumnInt64(8));
    if (expiry > maximum_expiry) {
      expiry = maximum_expiry;
    }

    net::CanonicalCookie* cookie = net::CanonicalCookie::Create(
        GURL(get_all.ColumnString(0)), get_all.ColumnString(1),
        get_all.ColumnString(2), get_all.ColumnString(3),
        get_all.ColumnString(4), get_all.ColumnString(5),
        get_all.ColumnString(6),
        base::Time::FromInternalValue(get_all.ColumnInt64(7)), expiry,
        get_all.ColumnBool(10), get_all.ColumnBool(11));
    if (cookie) {
      cookie->SetLastAccessDate(
          base::Time::FromInternalValue(get_all.ColumnInt64(9)));
      actual_cookies.push_back(cookie);
    } else {
      DLOG(ERROR) << "Failed to create cookie.";
    }
  }

  return actual_cookies;
}

void SqlInit(const PersistentCookieStore::LoadedCallback& loaded_callback,
             storage::SqlContext* sql_context) {
  TRACE_EVENT0("cobalt::network", "PersistentCookieStore::SqlInit()");

  sql::Connection* conn = sql_context->sql_connection();

  // Check the table's schema version.
  int schema_version;
  bool table_exists =
      sql_context->GetSchemaVersion("CookieTable", &schema_version);

  if (table_exists) {
    if (schema_version == storage::StorageManager::kSchemaTableIsNew) {
      // This savegame predates the existence of the schema table.
      // Since the cookie table did not change between the initial release of
      // the app and the introduction of the schema table, assume that this
      // existing cookie table is schema version 1.  This avoids a loss of
      // cookies on upgrade.
      DLOG(INFO) << "Updating CookieTable schema version to "
                 << kOriginalCookieSchemaVersion;
      sql_context->UpdateSchemaVersion("CookieTable",
                                       kOriginalCookieSchemaVersion);
    } else if (schema_version == storage::StorageManager::kSchemaVersionLost) {
      // Since there has only been one schema so far, treat this the same as
      // kSchemaTableIsNew.  When there are multiple schemas in the wild,
      // we may want to drop the table instead.
      sql_context->UpdateSchemaVersion("CookieTable",
                                       kOriginalCookieSchemaVersion);
    }
  } else {
    // The table does not exist, so create it in its latest form.
    sql::Statement create_table(conn->GetUniqueStatement(
        "CREATE TABLE CookieTable ("
        "url TEXT, "
        "name TEXT, "
        "value TEXT, "
        "domain TEXT, "
        "path TEXT, "
        "mac_key TEXT, "
        "mac_algorithm TEXT, "
        "creation INTEGER, "
        "expiration INTEGER, "
        "last_access INTEGER, "
        "secure INTEGER, "
        "http_only INTEGER, "
        "UNIQUE(name, domain, path) ON CONFLICT REPLACE)"));
    bool ok = create_table.Run();
    DCHECK(ok);
    sql_context->UpdateSchemaVersion("CookieTable", kLatestCookieSchemaVersion);
  }

  loaded_callback.Run(GetAllCookies(conn));
}

void SqlAddCookie(const net::CanonicalCookie& cc,
                  storage::SqlContext* sql_context) {
  base::Time maximum_expiry = base::Time::Now() + kMaxCookieLifetime;
  base::Time expiry = cc.ExpiryDate();
  if (expiry > maximum_expiry) {
    expiry = maximum_expiry;
  }
  sql::Connection* conn = sql_context->sql_connection();
  sql::Statement insert_cookie(conn->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO CookieTable ("
      "url, name, value, domain, path, mac_key, mac_algorithm, "
      "creation, expiration, last_access, secure, http_only"
      ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

  insert_cookie.BindString(0, cc.Source());
  insert_cookie.BindString(1, cc.Name());
  insert_cookie.BindString(2, cc.Value());
  insert_cookie.BindString(3, cc.Domain());
  insert_cookie.BindString(4, cc.Path());
  insert_cookie.BindString(5, cc.MACKey());
  insert_cookie.BindString(6, cc.MACAlgorithm());
  insert_cookie.BindInt64(7, cc.CreationDate().ToInternalValue());
  insert_cookie.BindInt64(8, expiry.ToInternalValue());
  insert_cookie.BindInt64(9, cc.LastAccessDate().ToInternalValue());
  insert_cookie.BindBool(10, cc.IsSecure());
  insert_cookie.BindBool(11, cc.IsHttpOnly());
  bool ok = insert_cookie.Run();
  DCHECK(ok);
  sql_context->Flush();
}

void SqlUpdateCookieAccessTime(const net::CanonicalCookie& cc,
                               storage::SqlContext* sql_context) {
  sql::Connection* conn = sql_context->sql_connection();
  sql::Statement touch_cookie(
      conn->GetCachedStatement(SQL_FROM_HERE,
                               "UPDATE CookieTable SET last_access = ? WHERE "
                               "name = ? AND domain = ? AND path = ?"));

  base::Time maximum_expiry = base::Time::Now() + kMaxCookieLifetime;
  base::Time expiry = cc.ExpiryDate();
  if (expiry > maximum_expiry) {
    expiry = maximum_expiry;
  }

  touch_cookie.BindInt64(0, expiry.ToInternalValue());
  touch_cookie.BindString(1, cc.Name());
  touch_cookie.BindString(2, cc.Domain());
  touch_cookie.BindString(3, cc.Path());
  bool ok = touch_cookie.Run();
  DCHECK(ok);
  sql_context->Flush();
}

void SqlDeleteCookie(const net::CanonicalCookie& cc,
                     storage::SqlContext* sql_context) {
  sql::Connection* conn = sql_context->sql_connection();
  sql::Statement delete_cookie(conn->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM CookieTable WHERE name = ? AND domain = ? AND path = ?"));
  delete_cookie.BindString(0, cc.Name());
  delete_cookie.BindString(1, cc.Domain());
  delete_cookie.BindString(2, cc.Path());
  bool ok = delete_cookie.Run();
  DCHECK(ok);
  sql_context->Flush();
}

void SqlSendEmptyCookieList(
    const PersistentCookieStore::LoadedCallback& loaded_callback,
    storage::SqlContext* sql_context) {
  UNREFERENCED_PARAMETER(sql_context);
  std::vector<net::CanonicalCookie*> empty_cookie_list;
  loaded_callback.Run(empty_cookie_list);
}

}  // namespace

PersistentCookieStore::PersistentCookieStore(storage::StorageManager* storage)
    : storage_(storage) {}

PersistentCookieStore::~PersistentCookieStore() {}

void PersistentCookieStore::Load(const LoadedCallback& loaded_callback) {
  storage_->GetSqlContext(base::Bind(&SqlInit, loaded_callback));
}

void PersistentCookieStore::LoadCookiesForKey(
    const std::string& key, const LoadedCallback& loaded_callback) {
  UNREFERENCED_PARAMETER(key);
  // We don't support loading of individual cookies.
  // This is always called after Load(), so just post the callback to the SQL
  // thread to make sure it is run after Load() has finished.
  // See comments in net/cookie_monster.cc for more information.
  storage_->GetSqlContext(base::Bind(&SqlSendEmptyCookieList,
                                     loaded_callback));
}

void PersistentCookieStore::AddCookie(const net::CanonicalCookie& cc) {
  // We expect that all cookies we are fed are meant to persist.
  DCHECK(cc.IsPersistent());
  storage_->GetSqlContext(base::Bind(&SqlAddCookie, cc));
}

void PersistentCookieStore::UpdateCookieAccessTime(
    const net::CanonicalCookie& cc) {
  storage_->GetSqlContext(base::Bind(&SqlUpdateCookieAccessTime, cc));
}

void PersistentCookieStore::DeleteCookie(const net::CanonicalCookie& cc) {
  storage_->GetSqlContext(base::Bind(&SqlDeleteCookie, cc));
}

void PersistentCookieStore::SetForceKeepSessionState() {
  // We don't expect this to be called, and we don't implement these semantics.
  NOTREACHED();
}

void PersistentCookieStore::Flush(const base::Closure& callback) {
  storage_->FlushNow(callback);
}

}  // namespace network
}  // namespace cobalt
