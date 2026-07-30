// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/coordinator/level_up_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_category.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_stat.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_task.h"
#import "ios/chrome/browser/level_up/model/level_up_service.h"
#import "ios/chrome/browser/level_up/model/task_info.h"
#import "ios/chrome/browser/level_up/model/task_types.h"
#import "ios/chrome/browser/level_up/ui/level_up_consumer.h"
#import "ios/chrome/browser/level_up/ui/level_up_profile_consumer.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/avatar/resized_avatar_cache.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation LevelUpMediator {
  // The authentication service.
  raw_ptr<AuthenticationService> _authService;
  // The level up service.
  raw_ptr<LevelUpService> _levelUpService;
  // Image cache for user avatars.
  ResizedAvatarCache* _avatarCache;
  // The list of task categories.
  NSArray<LevelUpCategory*>* _categories;
}

- (instancetype)initWithAuthenticationService:
                    (AuthenticationService*)authService
                               levelUpService:(LevelUpService*)levelUpService {
  self = [super init];
  if (self) {
    _authService = authService;
    _levelUpService = levelUpService;
    _avatarCache = [[ResizedAvatarCache alloc]
        initWithIdentityAvatarSize:IdentityAvatarSize::Large];
  }
  return self;
}

- (void)setConsumer:(id<LevelUpConsumer>)consumer {
  _consumer = consumer;

  id<SystemIdentity> identity = _authService->GetPrimaryIdentity();
  NSString* userFullName = identity.userFullName;
  UIImage* userAvatar = [_avatarCache resizedAvatarForIdentity:identity];

  int level = _levelUpService->GetCurrentLevel();

  NSMutableArray<LevelUpTask*>* productivityTasks =
      [[NSMutableArray alloc] init];
  NSMutableArray<LevelUpTask*>* safetyTasks = [[NSMutableArray alloc] init];
  NSMutableArray<LevelUpTask*>* searchTasks = [[NSMutableArray alloc] init];
  NSMutableArray<LevelUpTask*>* allTasks = [[NSMutableArray alloc] init];

  const auto& tasks = _levelUpService->GetTasks();
  for (const auto& [type, info] : tasks) {
    BOOL completed = _levelUpService->IsTaskCompleted(type);
    LevelUpTask* task = [[LevelUpTask alloc] initWithTaskInfo:info.get()
                                                    completed:completed];
    [allTasks addObject:task];

    switch (task.category) {
      case LevelUpTaskCategory::kProductivity:
        [productivityTasks addObject:task];
        break;
      case LevelUpTaskCategory::kSafety:
        [safetyTasks addObject:task];
        break;
      case LevelUpTaskCategory::kSearch:
        [searchTasks addObject:task];
        break;
    }
  }

  // TODO(crbug.com/523325903): Update this to match the final design spec. Use
  // the first 4 uncompleted tasks for now.
  NSMutableArray<LevelUpTask*>* uncompletedTasks =
      [[NSMutableArray alloc] init];
  for (LevelUpTask* task in allTasks) {
    if (!task.completed) {
      [uncompletedTasks addObject:task];
      if (uncompletedTasks.count == 4) {
        break;
      }
    }
  }
  NSArray<LevelUpTask*>* tasksForCurrentLevel = uncompletedTasks;

  if ([self.consumer respondsToSelector:@selector(setLevel:tasksForLevel:)]) {
    [self.consumer setLevel:level tasksForLevel:tasksForCurrentLevel];
  }

  _categories = @[
    [[LevelUpCategory alloc] initWithTitle:@"Productivity"
                                     tasks:productivityTasks],
    [[LevelUpCategory alloc] initWithTitle:@"Safety" tasks:safetyTasks],
    [[LevelUpCategory alloc] initWithTitle:@"Search" tasks:searchTasks]
  ];

  if ([self.consumer respondsToSelector:@selector(addCategoryCard:)]) {
    for (LevelUpCategory* category in _categories) {
      [self.consumer addCategoryCard:category];
    }
  }
  [self configureTaskStat:allTasks];

  [self.profileConsumer setUserFullName:userFullName userAvatar:userAvatar];
}

- (void)configureAllTasksConsumer:(id<LevelUpConsumer>)allTasksConsumer {
  if ([allTasksConsumer respondsToSelector:@selector(addCategoryCard:)]) {
    for (LevelUpCategory* category in _categories) {
      [allTasksConsumer addCategoryCard:category];
    }
  }
}

#pragma mark - Private

// Configures the task stat.
- (void)configureTaskStat:(NSArray<LevelUpTask*>*)allTasks {
  NSMutableArray<LevelUpStat*>* stats = [[NSMutableArray alloc] init];

  NSString* title1 =
      l10n_util::GetPluralNSStringF(IDS_IOS_LEVEL_UP_STAT_TABS_DECLUTTERED, 3);
  NSString* subtitle1 =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_STAT_SUBTITLE_TABS_DECLUTTERED);
  LevelUpStat* stat1 = [[LevelUpStat alloc]
      initWithTitle:title1
           subtitle:subtitle1
              image:DefaultSymbolTemplateWithPointSize(kBookmarksSymbol, 28.0)
               type:LevelUpTaskStatType::kTabsDecluttered];
  [stats addObject:stat1];

  NSString* title2 =
      l10n_util::GetPluralNSStringF(IDS_IOS_LEVEL_UP_STAT_TYPING_SAVED, 5);
  NSString* subtitle2 =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_STAT_SUBTITLE_TYPING_SAVED);
  LevelUpStat* stat2 = [[LevelUpStat alloc]
      initWithTitle:title2
           subtitle:subtitle2
              image:DefaultSymbolTemplateWithPointSize(kKeySymbol, 28.0)
               type:LevelUpTaskStatType::kTypingSaved];
  [stats addObject:stat2];

  NSString* title3 = l10n_util::GetPluralNSStringF(
      IDS_IOS_LEVEL_UP_STAT_PASSWORDS_VERIFIED, 5);
  NSString* subtitle3 =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_STAT_SUBTITLE_PASSWORDS_VERIFIED);
  LevelUpStat* stat3 = [[LevelUpStat alloc]
      initWithTitle:title3
           subtitle:subtitle3
              image:DefaultSymbolTemplateWithPointSize(kKeySymbol, 28.0)
               type:LevelUpTaskStatType::kPasswordsVerified];
  [stats addObject:stat3];

  NSString* title4 =
      l10n_util::GetPluralNSStringF(IDS_IOS_LEVEL_UP_STAT_SEARCHES_SKIPPED, 3);
  NSString* subtitle4 =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_STAT_SUBTITLE_SEARCHES_SKIPPED);
  LevelUpStat* stat4 =
      [[LevelUpStat alloc] initWithTitle:title4
                                subtitle:subtitle4
                                   image:DefaultSymbolTemplateWithPointSize(
                                             kDefaultBrowserSymbol, 28.0)
                                    type:LevelUpTaskStatType::kSearchesSkipped];
  [stats addObject:stat4];

  if ([self.consumer respondsToSelector:@selector(setStats:)]) {
    [self.consumer setStats:stats];
  }
}

@end
