// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_CREATION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_CREATION_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/browsing_data_migration_view_controller.h"

@protocol ManagedProfileCreationConsumer;

namespace signin {
class IdentityManager;
}  // namespace signin

// Mediator that handles the sign-in operation.
@interface ManagedProfileCreationMediator
    : NSObject <BrowsingDataMigrationViewControllerMutator>

// Consumer for this mediator.
@property(nonatomic, weak) id<ManagedProfileCreationConsumer> consumer;

@property(nonatomic, assign) BOOL keepBrowsingDataSeparate;

- (instancetype)initWithIdentityManager:
                    (signin::IdentityManager*)identityManager
                skipBrowsingDataMigration:(BOOL)skipBrowsingDataMigration
               mergeBrowsingDataByDefault:(BOOL)mergeBrowsingDataByDefault
    browsingDataMigrationDisabledByPolicy:
        (BOOL)browsingDataMigrationDisabledByPolicy NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_CREATION_MEDIATOR_H_
