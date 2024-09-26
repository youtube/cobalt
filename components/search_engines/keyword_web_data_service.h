// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_KEYWORD_WEB_DATA_SERVICE_H__
#define COMPONENTS_SEARCH_ENGINES_KEYWORD_WEB_DATA_SERVICE_H__

#include <stddef.h>
#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "components/search_engines/keyword_table.h"
#include "components/search_engines/template_url_id.h"
#include "components/webdata/common/web_data_service_base.h"

namespace base {
class SingleThreadTaskRunner;
}

class WebDatabaseService;
struct TemplateURLData;

struct WDKeywordsResult {
  WDKeywordsResult();
  WDKeywordsResult(const WDKeywordsResult&);
  WDKeywordsResult& operator=(const WDKeywordsResult&);
  ~WDKeywordsResult();

  KeywordTable::Keywords keywords;
  // Identifies the ID of the TemplateURL that is the default search. A value of
  // 0 indicates there is no default search provider.
  int64_t default_search_provider_id = 0;
  // Version of the built-in keywords and starter pack engines. A value of 0
  // indicates a first run.
  int builtin_keyword_version = 0;
  int starter_pack_version = 0;
};

class WebDataServiceConsumer;

class KeywordWebDataService : public WebDataServiceBase {
 public:
  // Instantiate this to turn on batch mode on the provided |service|
  // until the scoper is destroyed.  When batch mode is on, calls to any of the
  // three keyword table modification functions below will result in locally
  // queueing the operation; on setting this back to false, after a short delay,
  // all the modifications will be performed at once.  This is a performance
  // optimization; see comments on KeywordTable::PerformOperations().
  //
  // If multiple scopers are in-scope simultaneously, batch mode will only be
  // exited when all are destroyed.  If |service| is NULL, the object will do
  // nothing.
  class BatchModeScoper {
   public:
    explicit BatchModeScoper(KeywordWebDataService* service);

    BatchModeScoper(const BatchModeScoper&) = delete;
    BatchModeScoper& operator=(const BatchModeScoper&) = delete;

    ~BatchModeScoper();

   private:
    raw_ptr<KeywordWebDataService> service_;
  };

  KeywordWebDataService(
      scoped_refptr<WebDatabaseService> wdbs,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  KeywordWebDataService(const KeywordWebDataService&) = delete;
  KeywordWebDataService& operator=(const KeywordWebDataService&) = delete;

  // As the database processes requests at a later date, all deletion is done on
  // the background sequence.
  //
  // Many of the keyword related methods do not return a handle. This is because
  // the caller (TemplateURLService) does not need to know when the request is
  // done.

  void AddKeyword(const TemplateURLData& data);
  void RemoveKeyword(TemplateURLID id);
  void UpdateKeyword(const TemplateURLData& data);

  // Fetches the keywords.
  // On success, consumer is notified with WDResult<KeywordTable::Keywords>.
  Handle GetKeywords(WebDataServiceConsumer* consumer);

  // Sets the ID of the default search provider.
  void SetDefaultSearchProviderID(TemplateURLID id);

  // Sets the version of the builtin keywords.
  void SetBuiltinKeywordVersion(int version);

  // Sets the version of the starter pack keywords.
  void SetStarterPackKeywordVersion(int version);

  // WebDataServiceBase:
  void ShutdownOnUISequence() override;

 protected:
  ~KeywordWebDataService() override;

 private:
  // Called by the BatchModeScoper (see comments there).
  void AdjustBatchModeLevel(bool entering_batch_mode);

  // Schedules a task to commit any |queued_keyword_operations_| immediately.
  void CommitQueuedOperations();

  size_t batch_mode_level_ = 0;
  KeywordTable::Operations queued_keyword_operations_;
  base::RetainingOneShotTimer timer_;  // Used to commit updates no more often
                                       // than every five seconds.
};

#endif  // COMPONENTS_SEARCH_ENGINES_KEYWORD_WEB_DATA_SERVICE_H__
