// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import <memory>

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_mutator.h"

@protocol DriveFilePickerMediatorDelegate;
@class DriveFilePickerMetricsHelper;
@protocol DriveFilePickerConsumer;
@protocol DriveFilePickerCommands;
@protocol SystemIdentity;

namespace drive {
class DriveService;
}

namespace signin {
class IdentityManager;
}

namespace web {
class WebState;
}

namespace image_fetcher {
class ImageDataFetcher;
}

class ChromeAccountManagerService;

// Mediator of the Drive file picker.
@interface DriveFilePickerMediator : NSObject <DriveFilePickerMutator>

// A delegate to browse a given drive folder or search in drive.
@property(nonatomic, weak) id<DriveFilePickerMediatorDelegate> delegate;

@property(nonatomic, weak) id<DriveFilePickerConsumer> consumer;

// Drive file picker handler.
@property(nonatomic, weak) id<DriveFilePickerCommands> driveFilePickerHandler;

// Whether the mediator is active (a.k.a at the top of the navigation stack).
@property(nonatomic, assign, getter=isActive) BOOL active;

// Initializes the mediator with a given `webState`.
- (instancetype)
         initWithWebState:(web::WebState*)webState
                 identity:(id<SystemIdentity>)identity
                    title:(NSString*)title
            imagesPending:(NSMutableSet<NSString*>*)imagesPending
               imageCache:(NSCache<NSString*, UIImage*>*)imageCache
           collectionType:(DriveFilePickerCollectionType)collectionType
         folderIdentifier:(NSString*)folderIdentifier
                   filter:(DriveFilePickerFilter)filter
      ignoreAcceptedTypes:(BOOL)ignoreAcceptedTypes
          sortingCriteria:(DriveItemsSortingType)sortingCriteria
         sortingDirection:(DriveItemsSortingOrder)sortingDirection
             driveService:(drive::DriveService*)driveService
          identityManager:(signin::IdentityManager*)identityManager
    accountManagerService:(ChromeAccountManagerService*)accountManagerService
             imageFetcher:
                 (std::unique_ptr<image_fetcher::ImageDataFetcher>)imageFetcher
            metricsHelper:(DriveFilePickerMetricsHelper*)metricsHelper
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnects from the model layer.
- (void)disconnect;

// Sets the identity to `selectedIdentity`.
- (void)setSelectedIdentity:(id<SystemIdentity>)selectedIdentity;

// Sets a pending filter or sorting criteria. This will take effect when the
// mediator is set to active.
- (void)setPendingFilter:(DriveFilePickerFilter)filter
         sortingCriteria:(DriveItemsSortingType)sortingCriteria
        sortingDirection:(DriveItemsSortingOrder)sortingDirection
     ignoreAcceptedTypes:(BOOL)ignoreAcceptedTypes;

@end

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_MEDIATOR_H_
