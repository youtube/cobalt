// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/topsites_spotlight_manager.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/ref_counted.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/history/core/browser/history_types.h"
#import "components/history/core/browser/top_sites.h"
#import "components/history/core/browser/top_sites_observer.h"
#import "components/sync/driver/sync_service.h"
#import "ios/chrome/app/spotlight/spotlight_interface.h"
#import "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/top_sites_factory.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class SpotlightTopSitesBridge;
class SpotlightTopSitesCallbackBridge;

@interface TopSitesSpotlightManager ()<SyncObserverModelBridge> {
  // Bridge to register for top sites changes. It's important that this instance
  // variable is released before the _topSite one.
  std::unique_ptr<SpotlightTopSitesBridge> _topSitesBridge;

  // Bridge to register for top sites callbacks.
  std::unique_ptr<SpotlightTopSitesCallbackBridge> _topSitesCallbackBridge;

  bookmarks::BookmarkModel* _bookmarkModel;             // weak

  scoped_refptr<history::TopSites> _topSites;

  // Indicates if a reindex is pending. Reindexes made by calling the external
  // reindexTopSites method are executed at most every second.
  BOOL _isReindexPending;
}
@property(nonatomic, readonly) scoped_refptr<history::TopSites> topSites;

- (instancetype)
    initWithLargeIconService:(favicon::LargeIconService*)largeIconService
                    topSites:(scoped_refptr<history::TopSites>)topSites
               bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
          spotlightInterface:(SpotlightInterface*)spotlightInterface;

// Updates all indexed top sites from appropriate source, within limit of number
// of sites shown on NTP.
- (void)updateAllTopSitesSpotlightItems;
// Adds all top sites from appropriate source, within limit of number of sites
// shown on NTP.
- (void)addAllTopSitesSpotlightItems;
// Adds all top sites from TopSites source (most visited sites on device),
// within limit of number of sites shown on NTP.
- (void)addAllLocalTopSitesItems;
// Adds all top sites from Suggestions source (server-based), within limit of
// number of sites shown on NTP.
- (void)onMostVisitedURLsAvailable:
    (const history::MostVisitedURLList&)top_sites;

@end

class SpotlightTopSitesCallbackBridge
    : public base::SupportsWeakPtr<SpotlightTopSitesCallbackBridge> {
 public:
  explicit SpotlightTopSitesCallbackBridge(TopSitesSpotlightManager* owner)
      : owner_(owner) {}

  ~SpotlightTopSitesCallbackBridge() {}

  void OnMostVisitedURLsAvailable(const history::MostVisitedURLList& data) {
    [owner_ onMostVisitedURLsAvailable:data];
  }

 private:
  __weak TopSitesSpotlightManager* owner_;
};

class SpotlightTopSitesBridge : public history::TopSitesObserver {
 public:
  SpotlightTopSitesBridge(TopSitesSpotlightManager* owner,
                          history::TopSites* top_sites)
      : owner_(owner), top_sites_(top_sites) {
    top_sites->AddObserver(this);
  }

  ~SpotlightTopSitesBridge() override {
    top_sites_->RemoveObserver(this);
    top_sites_ = nullptr;
  }

  void TopSitesLoaded(history::TopSites* top_sites) override {}

  void TopSitesChanged(history::TopSites* top_sites,
                       ChangeReason change_reason) override {
    [owner_ updateAllTopSitesSpotlightItems];
  }

 private:
  __weak TopSitesSpotlightManager* owner_;
  history::TopSites* top_sites_;
};

@implementation TopSitesSpotlightManager
@synthesize topSites = _topSites;

+ (TopSitesSpotlightManager*)topSitesSpotlightManagerWithBrowserState:
    (ChromeBrowserState*)browserState {
  return [[TopSitesSpotlightManager alloc]
      initWithLargeIconService:IOSChromeLargeIconServiceFactory::
                                   GetForBrowserState(browserState)
                      topSites:ios::TopSitesFactory::GetForBrowserState(
                                   browserState)
                 bookmarkModel:ios::LocalOrSyncableBookmarkModelFactory::
                                   GetForBrowserState(browserState)
            spotlightInterface:[SpotlightInterface defaultInterface]];
}

- (instancetype)
    initWithLargeIconService:(favicon::LargeIconService*)largeIconService
                    topSites:(scoped_refptr<history::TopSites>)topSites
               bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
          spotlightInterface:(SpotlightInterface*)spotlightInterface {
  self = [super initWithLargeIconService:largeIconService
                                  domain:spotlight::DOMAIN_TOPSITES
                      spotlightInterface:spotlightInterface];
  if (self) {
    DCHECK(topSites);
    DCHECK(bookmarkModel);
    _topSites = topSites;
    _topSitesBridge.reset(new SpotlightTopSitesBridge(self, _topSites.get()));
    _topSitesCallbackBridge.reset(new SpotlightTopSitesCallbackBridge(self));
    _bookmarkModel = bookmarkModel;
    _isReindexPending = false;
  }
  return self;
}

- (void)updateAllTopSitesSpotlightItems {
  __weak TopSitesSpotlightManager* weakSelf = self;
  [self clearAllSpotlightItems:^(NSError* error) {
    [weakSelf addAllTopSitesSpotlightItems];
  }];
}

- (void)addAllTopSitesSpotlightItems {
  if (!_topSites)
    return;

  [self addAllLocalTopSitesItems];
}

- (void)addAllLocalTopSitesItems {
  _topSites->GetMostVisitedURLs(base::BindOnce(
      &SpotlightTopSitesCallbackBridge::OnMostVisitedURLsAvailable,
      _topSitesCallbackBridge->AsWeakPtr()));
}

- (BOOL)isURLBookmarked:(const GURL&)URL {
  if (!_bookmarkModel->loaded())
    return NO;

  std::vector<const bookmarks::BookmarkNode*> nodes;
  _bookmarkModel->GetNodesByURL(URL, &nodes);
  return nodes.size() > 0;
}

- (void)onMostVisitedURLsAvailable:
    (const history::MostVisitedURLList&)top_sites {
  NSUInteger sitesToIndex =
      MIN(top_sites.size(), [ContentSuggestionsMediator maxSitesShown]);
  for (size_t i = 0; i < sitesToIndex; i++) {
    const GURL& URL = top_sites[i].url;

    // Check if the item is bookmarked, in which case it is already indexed.
    if ([self isURLBookmarked:URL]) {
      continue;
    }

    [self refreshItemsWithURL:URL
                        title:base::SysUTF16ToNSString(top_sites[i].title)];
  }
}

- (void)reindexTopSites {
  if (_isReindexPending) {
    return;
  }
  _isReindexPending = true;
  __weak TopSitesSpotlightManager* weakSelf = self;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(1 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        TopSitesSpotlightManager* strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        [strongSelf updateAllTopSitesSpotlightItems];
        strongSelf->_isReindexPending = false;
      });
}

- (void)shutdown {
  _topSitesBridge.reset();
  _topSitesCallbackBridge.reset();

  _topSites = nullptr;
  _bookmarkModel = nullptr;

  [super shutdown];
}

#pragma mark -
#pragma mark SyncObserverModelBridge

- (void)onSyncStateChanged {
  [self updateAllTopSitesSpotlightItems];
}

@end
