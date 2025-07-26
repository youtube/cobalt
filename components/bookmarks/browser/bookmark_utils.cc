// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_utils.h"

#include <stdint.h>

#include <algorithm>
#include <array>
#include <memory>
#include <unordered_set>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/query_parser/query_parser.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/models/tree_node_iterator.h"
#include "url/gurl.h"

using base::Time;

namespace bookmarks {

namespace {

// The maximum length of URL or title returned by the Cleanup functions.
const size_t kCleanedUpUrlMaxLength = 1024u;
const size_t kCleanedUpTitleMaxLength = 1024u;

void CloneBookmarkNodeImpl(BookmarkModel* model,
                           const BookmarkNodeData::Element& element,
                           const BookmarkNode* parent,
                           size_t index_to_add_at,
                           bool reset_node_times) {
  // Make sure to not copy non clonable keys.
  BookmarkNode::MetaInfoMap meta_info_map = element.meta_info_map;
  if (element.is_url) {
    Time date_added = reset_node_times ? Time::Now() : element.date_added;
    DCHECK(!date_added.is_null());

    const BookmarkNode* node = model->AddURL(
        parent, index_to_add_at, element.title, element.url, &meta_info_map);
    model->SetDateAdded(node, date_added);

  } else {
    const BookmarkNode* cloned_node = model->AddFolder(
        parent, index_to_add_at, element.title, &meta_info_map);
    if (!reset_node_times) {
      DCHECK(!element.date_folder_modified.is_null());
      model->SetDateFolderModified(cloned_node, element.date_folder_modified);
    }
    for (int i = 0; i < static_cast<int>(element.children.size()); ++i)
      CloneBookmarkNodeImpl(model, element.children[i], cloned_node, i,
                            reset_node_times);
  }
}

// Comparison function that compares based on date modified of the two nodes.
bool MoreRecentlyModified(const BookmarkNode* n1, const BookmarkNode* n2) {
  return n1->date_folder_modified() > n2->date_folder_modified();
}

// Returns true if `text` contains each string in `words`. This is used when
// searching for bookmarks.
bool DoesBookmarkTextContainWords(const std::u16string& text,
                                  const std::vector<std::u16string>& words) {
  for (size_t i = 0; i < words.size(); ++i) {
    if (!base::i18n::StringSearchIgnoringCaseAndAccents(words[i], text, nullptr,
                                                        nullptr)) {
      return false;
    }
  }
  return true;
}

// Recursively searches for a node satisfying the functor `pred` . Returns
// nullptr if not found.
template <typename Predicate>
const BookmarkNode* FindNode(const BookmarkNode* node, Predicate pred) {
  if (pred(node))
    return node;

  for (const auto& child : node->children()) {
    const BookmarkNode* result = FindNode(child.get(), pred);
    if (result)
      return result;
  }
  return nullptr;
}

// Attempts to shorten a URL safely (i.e., by preventing the end of the URL
// from being in the middle of an escape sequence) to no more than
// kCleanedUpUrlMaxLength characters, returning the result.
std::string TruncateUrl(const std::string& url) {
  if (url.length() <= kCleanedUpUrlMaxLength)
    return url;

  // If we're in the middle of an escape sequence, truncate just before it.
  if (url[kCleanedUpUrlMaxLength - 1] == '%')
    return url.substr(0, kCleanedUpUrlMaxLength - 1);
  if (url[kCleanedUpUrlMaxLength - 2] == '%')
    return url.substr(0, kCleanedUpUrlMaxLength - 2);

  return url.substr(0, kCleanedUpUrlMaxLength);
}

template <class type>
std::vector<const BookmarkNode*> GetBookmarksMatchingPropertiesImpl(
    type& iterator,
    BookmarkModel* model,
    const QueryFields& query,
    const std::vector<std::u16string>& query_words,
    size_t max_count) {
  std::vector<const BookmarkNode*> nodes;
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if ((!query_words.empty() &&
         !DoesBookmarkContainWords(node->GetTitle(), node->url(),
                                   query_words)) ||
        model->is_permanent_node(node)) {
      continue;
    }
    if (query.title && node->GetTitle() != *query.title)
      continue;

    nodes.push_back(node);
    if (nodes.size() == max_count) {
      break;
    }
  }
  return nodes;
}

template <class Comparator>
void GetMostRecentEntries(
    BookmarkModel* model,
    size_t limit,
    std::multiset<const BookmarkNode*, Comparator>* nodes_set) {
  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if (node->is_url()) {
      nodes_set->insert(node);
      if (nodes_set->size() > limit) {
        nodes_set->erase(std::next(nodes_set->begin(), limit),
                         nodes_set->end());
      }
    }
  }
}

#if BUILDFLAG(IS_ANDROID)
// Returns whether or not a bookmark model contains any bookmarks aside of the
// permanent nodes.
bool HasUserCreatedBookmarks(BookmarkModel* model) {
  const BookmarkNode* root_node = model->root_node();

  return std::ranges::any_of(root_node->children(), [](const auto& node) {
    return !node->children().empty();
  });
}
#endif

}  // namespace

QueryFields::QueryFields() = default;
QueryFields::~QueryFields() = default;

VectorIterator::VectorIterator(
    std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>* nodes)
    : nodes_(nodes), current_(nodes->begin()) {}

VectorIterator::~VectorIterator() = default;

bool VectorIterator::has_next() {
  return (current_ != nodes_->end());
}

const BookmarkNode* VectorIterator::Next() {
  const BookmarkNode* result = *current_;
  ++current_;
  return result;
}

void CloneBookmarkNode(BookmarkModel* model,
                       const std::vector<BookmarkNodeData::Element>& elements,
                       const BookmarkNode* parent,
                       size_t index_to_add_at,
                       bool reset_node_times) {
  if (!parent->is_folder() || !model) {
    NOTREACHED();
  }
  for (size_t i = 0; i < elements.size(); ++i) {
    CloneBookmarkNodeImpl(model, elements[i], parent, index_to_add_at + i,
                          reset_node_times);
  }

  metrics::RecordCloneBookmarkNode(elements.size());
}

std::vector<const BookmarkNode*> GetMostRecentlyModifiedUserFolders(
    BookmarkModel* model) {
  std::vector<const BookmarkNode*> nodes;
  ui::TreeNodeIterator<const BookmarkNode> iterator(
      model->root_node(), base::BindRepeating(&PruneFoldersForDisplay, model));

  while (iterator.has_next()) {
    nodes.push_back(iterator.Next());
  }

  // TODO(crbug.com/354892429): Filter local permanent nodes if they shouldn't
  // visible (user has permanent account nodes but no local bookmarks).

  std::ranges::stable_sort(nodes, &MoreRecentlyModified);

  return nodes;
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
BookmarkNodesSplitByAccountAndLocal::BookmarkNodesSplitByAccountAndLocal() =
    default;
BookmarkNodesSplitByAccountAndLocal::BookmarkNodesSplitByAccountAndLocal(
    const BookmarkNodesSplitByAccountAndLocal&) = default;
BookmarkNodesSplitByAccountAndLocal&
BookmarkNodesSplitByAccountAndLocal::operator=(
    const BookmarkNodesSplitByAccountAndLocal&) = default;
BookmarkNodesSplitByAccountAndLocal::~BookmarkNodesSplitByAccountAndLocal() =
    default;

BookmarkNodesSplitByAccountAndLocal GetMostRecentlyUsedFoldersForDisplay(
    BookmarkModel* model,
    const BookmarkNode* displayed_node) {
  // `displayed_node` is meant to be a bookmark. Code below is not tested for
  // folders.
  CHECK(!displayed_node->is_folder());

  // Max number of most recently used non-permanent-node folders.
  static constexpr size_t kMaxMRUFolders = 5;

  std::vector<const BookmarkNode*> mru_nodes =
      bookmarks::GetMostRecentlyModifiedUserFolders(model);
  const BookmarkNode* const most_recent_node =
      mru_nodes.empty() ? nullptr : mru_nodes[0];

  // Special case the parent item, it'll either remain first or filtered out as
  // a permanent node and added back later.
  std::erase(mru_nodes, displayed_node->parent());  // No-op if not present.
  mru_nodes.insert(mru_nodes.begin(), displayed_node->parent());

  // Remove permanent nodes, they'll be re-added at the end if used later.
  std::erase_if(mru_nodes, [](const BookmarkNode* mru_node) {
    return mru_node->is_permanent_node();
  });

  // Figure out which permanent nodes to add.
  const bool account_nodes_exist =
      model->account_bookmark_bar_node() != nullptr;
  const std::vector<const BookmarkNode*> account_permanent_nodes(
      {model->account_bookmark_bar_node(), model->account_other_node(),
       model->account_mobile_node()});
  const std::vector<const BookmarkNode*> local_permanent_nodes(
      {model->bookmark_bar_node(), model->other_node(), model->mobile_node()});

  std::vector<const BookmarkNode*> permanent_nodes_included;
  for (const BookmarkNode* permanent_node :
       account_nodes_exist ? account_permanent_nodes : local_permanent_nodes) {
    if (!permanent_node->IsVisible()) {
      continue;
    }
    permanent_nodes_included.push_back(permanent_node);
  }

  if (account_nodes_exist) {
    // Add back the most recent node and the parent node if either of them are
    // local permanent nodes. Permanent account nodes are preferred.
    auto append_if_permanent_local_node = [model, &permanent_nodes_included](
                                              const BookmarkNode* mru_node) {
      if (mru_node->is_permanent_node() && model->IsLocalOnlyNode(*mru_node)) {
        permanent_nodes_included.push_back(mru_node);
      }
    };
    if (most_recent_node) {
      append_if_permanent_local_node(most_recent_node);
    }
    if (displayed_node->parent() != most_recent_node) {
      append_if_permanent_local_node(displayed_node->parent());
    }
  }

  // Cap total number of non-permanent nodes to kMaxMRUFolders.
  mru_nodes.resize(std::min(mru_nodes.size(), kMaxMRUFolders));

  // Add permanent nodes at the end (note that these lists are both sorted and
  // will remain sorted (permanent last) when split them up below.
  mru_nodes.insert(mru_nodes.end(), permanent_nodes_included.begin(),
                   permanent_nodes_included.end());

  // Split between account and local nodes if there are account nodes.
  BookmarkNodesSplitByAccountAndLocal result;
  if (account_nodes_exist) {
    std::vector<const BookmarkNode*> account_nodes;
    std::vector<const BookmarkNode*> local_nodes;
    for (const BookmarkNode* mru_node : mru_nodes) {
      (model->IsLocalOnlyNode(*mru_node) ? result.local_nodes
                                         : result.account_nodes)
          .push_back(mru_node);
    }
  } else {
    result.local_nodes = std::move(mru_nodes);
  }
  return result;
}

BookmarkNodesSplitByAccountAndLocal GetPermanentNodesForDisplay(
    const BookmarkModel* model) {
  BookmarkNodesSplitByAccountAndLocal permanent_nodes;
  const bool account_nodes_exists = model->account_bookmark_bar_node();
  if (account_nodes_exists) {
    for (const BookmarkNode* node :
         {model->account_bookmark_bar_node(), model->account_other_node(),
          model->account_mobile_node()}) {
      if (!bookmarks::PruneFoldersForDisplay(model, node)) {
        permanent_nodes.account_nodes.push_back(node);
      }
    }
    // Show only account nodes if we have no local/syncable bookmarks.
    if (!HasLocalOrSyncableBookmarks(model)) {
      return permanent_nodes;
    }
  }

  for (const BookmarkNode* node : {model->bookmark_bar_node(),
                                   model->other_node(), model->mobile_node()}) {
    if (!bookmarks::PruneFoldersForDisplay(model, node)) {
      permanent_nodes.local_nodes.push_back(node);
    }
  }

  return permanent_nodes;
}

bool HasLocalOrSyncableBookmarks(const BookmarkModel* model) {
  return std::ranges::any_of(
      std::array{model->bookmark_bar_node(), model->other_node(),
                 model->mobile_node()},
      [](const BookmarkNode* node) { return !node->children().empty(); });
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

void GetMostRecentlyAddedEntries(BookmarkModel* model,
                                 size_t count,
                                 std::vector<const BookmarkNode*>* nodes) {
  // std::set is used here since insert element into std::vector is slower than
  // std::set, so we use std::set to find the most recent bookmarks, and then
  // return to users as std::vector.
  std::multiset<const BookmarkNode*, decltype(&MoreRecentlyAdded)> nodes_set(
      &MoreRecentlyAdded);
  GetMostRecentEntries(model, count, &nodes_set);

  nodes->reserve(nodes_set.size());
  std::move(nodes_set.begin(), nodes_set.end(), std::back_inserter(*nodes));
}

bool MoreRecentlyAdded(const BookmarkNode* n1, const BookmarkNode* n2) {
  return n1->date_added() > n2->date_added();
}

void GetMostRecentlyUsedEntries(BookmarkModel* model,
                                size_t count,
                                std::vector<const BookmarkNode*>* nodes) {
  // std::set is used here since insert element into std::vector is slower than
  // std::set, so we use std::set to find the most recent bookmarks, and then
  // return to users as std::vector.
  auto lastUsedComp = [](const BookmarkNode* n1, const BookmarkNode* n2) {
    if (n1->date_last_used() == n2->date_last_used()) {
      // Both bookmarks have same used date, we compare added date instead,
      // normally this happens when both bookmarks are never used.
      return n1->date_added() > n2->date_added();
    }
    return n1->date_last_used() > n2->date_last_used();
  };
  std::multiset<const BookmarkNode*, decltype(lastUsedComp)> nodes_set(
      lastUsedComp);
  GetMostRecentEntries(model, count, &nodes_set);

  nodes->reserve(nodes_set.size());
  std::move(nodes_set.begin(), nodes_set.end(), std::back_inserter(*nodes));
}

std::vector<const BookmarkNode*> GetBookmarksMatchingProperties(
    BookmarkModel* model,
    const QueryFields& query,
    size_t max_count) {
  std::vector<std::u16string> query_words = ParseBookmarkQuery(query);
  if (query.word_phrase_query && query_words.empty()) {
    return {};
  }

  if (query.url) {
    // Shortcut into the BookmarkModel if searching for URL.
    GURL url(*query.url);
    std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>
        url_matched_nodes;
    if (url.is_valid()) {
      url_matched_nodes = model->GetNodesByURL(url);
    }
    VectorIterator iterator(&url_matched_nodes);
    return GetBookmarksMatchingPropertiesImpl<VectorIterator>(
        iterator, model, query, query_words, max_count);
  }

  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  return GetBookmarksMatchingPropertiesImpl<
      ui::TreeNodeIterator<const BookmarkNode>>(iterator, model, query,
                                                query_words, max_count);
}

// Parses the provided query and returns a vector of query words.
std::vector<std::u16string> ParseBookmarkQuery(
    const bookmarks::QueryFields& query) {
  std::vector<std::u16string> query_words;
  if (query.word_phrase_query) {
    query_parser::QueryParser::ParseQueryWords(
        base::i18n::ToLower(*query.word_phrase_query),
        query_parser::MatchingAlgorithm::DEFAULT, &query_words);
  }
  return query_words;
}

// Returns true if `node`s title or url contains the strings in `words`.
bool DoesBookmarkContainWords(const std::u16string& title,
                              const GURL& url,
                              const std::vector<std::u16string>& words) {
  return DoesBookmarkTextContainWords(title, words) ||
         DoesBookmarkTextContainWords(base::UTF8ToUTF16(url.spec()), words) ||
         DoesBookmarkTextContainWords(
             url_formatter::FormatUrl(url, url_formatter::kFormatUrlOmitNothing,
                                      base::UnescapeRule::NORMAL, nullptr,
                                      nullptr, nullptr),
             words);
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kShowBookmarkBar, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kEditBookmarksEnabled, true);
  registry->RegisterBooleanPref(
      prefs::kShowAppsShortcutInBookmarkBar, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kShowTabGroupsInBookmarkBar, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kShowManagedBookmarksInBookmarkBar, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kAddedBookmarkSincePowerBookmarksLaunch,
                                false);
  RegisterManagedBookmarksPrefs(registry);
}

void RegisterManagedBookmarksPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kManagedBookmarks);
  registry->RegisterStringPref(
      prefs::kManagedBookmarksFolderName, std::string());
}

void DeleteBookmarkFolders(BookmarkModel* model,
                           const std::vector<int64_t>& ids,
                           const base::Location& location) {
  // Remove the folders that were removed. This has to be done after all the
  // other changes have been committed.
  for (auto iter = ids.begin(); iter != ids.end(); ++iter) {
    const BookmarkNode* node = GetBookmarkNodeByID(model, *iter);
    if (!node)
      continue;
    model->Remove(node, metrics::BookmarkEditSource::kUser, location);
  }
}

const BookmarkNode* AddIfNotBookmarked(BookmarkModel* model,
                                       const GURL& url,
                                       const std::u16string& title,
                                       const BookmarkNode* parent) {
  // Nothing to do, a user bookmark with that url already exists.
  if (IsBookmarkedByUser(model, url))
    return nullptr;

  base::RecordAction(base::UserMetricsAction("BookmarkAdded"));

  const auto* parent_to_use =
      parent ? parent : GetParentForNewNodes(model, url);
  return model->AddNewURL(parent_to_use, parent_to_use->children().size(),
                          title, url);
}

void RemoveAllBookmarks(BookmarkModel* model,
                        const GURL& url,
                        const base::Location& location) {
  // Remove all the user bookmarks.
  for (const BookmarkNode* node : model->GetNodesByURL(url)) {
    if (!model->client()->IsNodeManaged(node)) {
      model->Remove(node, metrics::BookmarkEditSource::kUser, location);
    }
  }
}

std::u16string CleanUpUrlForMatching(
    const GURL& gurl,
    base::OffsetAdjuster::Adjustments* adjustments) {
  DCHECK(gurl.is_valid());

  base::OffsetAdjuster::Adjustments tmp_adjustments;
  return base::i18n::ToLower(url_formatter::FormatUrlWithAdjustments(
      GURL(TruncateUrl(gurl.spec())),
      url_formatter::kFormatUrlOmitUsernamePassword,
      base::UnescapeRule::SPACES | base::UnescapeRule::PATH_SEPARATORS |
          base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS,
      nullptr, nullptr, adjustments ? adjustments : &tmp_adjustments));
}

std::u16string CleanUpTitleForMatching(const std::u16string& title) {
  return base::i18n::ToLower(title.substr(0u, kCleanedUpTitleMaxLength));
}

bool IsBookmarkedByUser(BookmarkModel* model, const GURL& url) {
  for (const BookmarkNode* node : model->GetNodesByURL(url)) {
    if (!model->client()->IsNodeManaged(node)) {
      return true;
    }
  }
  return false;
}

const BookmarkNode* GetBookmarkNodeByID(const BookmarkModel* model,
                                        int64_t id) {
  return FindNode(model->root_node(),
                  [id](const BookmarkNode* node) { return node->id() == id; });
}

bool IsDescendantOf(const BookmarkNode* node, const BookmarkNode* root) {
  return node && node->HasAncestor(root);
}

bool HasDescendantsOf(
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>& list,
    const BookmarkNode* root) {
  for (const BookmarkNode* node : list) {
    if (IsDescendantOf(node, root))
      return true;
  }
  return false;
}

const BookmarkNode* GetParentForNewNodes(BookmarkModel* model,
                                         const GURL& url) {
#if BUILDFLAG(IS_ANDROID)
  if (!HasUserCreatedBookmarks(model))
    return model->mobile_node();
#endif
  const BookmarkNode* parent = model->client()->GetSuggestedSaveLocation(url);
  if (parent) {
    return parent;
  }

  std::vector<const BookmarkNode*> nodes =
      GetMostRecentlyModifiedUserFolders(model);
  CHECK(!nodes.empty());
  return nodes[0];
}

bool PruneFoldersForDisplay(const BookmarkModel* model,
                            const BookmarkNode* node) {
  return !node->IsVisible() || !node->is_folder() ||
         model->client()->IsNodeManaged(node);
}

}  // namespace bookmarks
