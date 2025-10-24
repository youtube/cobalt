// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/share_extension/extended_share_view_controller.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/ios/block_types.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/app_group/app_group_command.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/crash_report/crash_helper.h"
#import "ios/chrome/common/extension_open_url.h"
#import "ios/chrome/share_extension/share_extension_delegate.h"
#import "ios/chrome/share_extension/share_extension_sheet.h"
#import "ios/chrome/share_extension/ui_util.h"

// Type for completion handler to fetch the components of the share items.
// `idResponse` type depends on the element beeing fetched.
using ItemBlock = void (^)(id idResponse, NSError* error);

namespace {

const CGFloat kShareSheetCornerRadius = 20;

const CGFloat kShareImageTargetHeight = 1024;
const CGFloat kShareImageTargetWidth = 1024;

// Character limit for the text search.
const NSUInteger kSearchCharacterLimit = 1000;

}  // namespace

@interface ExtendedShareViewController () <ShareExtensionDelegate>

// The sheet to display when an item is shared.
@property(nonatomic, strong) ShareExtensionSheet* shareSheet;

// Whether a image was loaded already.
@property(nonatomic, assign) BOOL imageLoaded;

// Whether a URL was loaded already.
@property(nonatomic, assign) BOOL URLLoaded;

// The item type that can be created by the `ExtendedShareViewController`.
@property(nonatomic, assign) app_group::ShareExtensionItemType itemType;

// The item to share.
@property(nonatomic, strong) NSExtensionItem* shareItem;

// URL sharing content (URL, title and preview)
@property(nonatomic, strong) NSURL* shareURL;
@property(nonatomic, copy) NSString* shareTitle;
@property(nonatomic, strong) UIImage* preview;

// The image to share.
@property(nonatomic, strong) UIImage* shareImage;
@property(nonatomic, strong) NSData* shareImageData;

// The text to share.
@property(nonatomic, copy) NSString* shareText;

// Whether a text reach the character limit.
@property(nonatomic, assign) BOOL characterLimitReached;

// Creates a file in `app_group::ShareExtensionItemsFolder()` containing a
// serialized NSDictionary.
// If `cancel` is true, `actionType` is ignored.
- (void)queueActionItemURL:(NSURL*)URL
                     title:(NSString*)title
                    action:(app_group::ShareExtensionItemType)actionType
                    cancel:(BOOL)cancel
                completion:(ProceduralBlock)completion;

// Loads all the shared elements from the extension context and update the UI.
- (void)loadElementsFromContext;
- (void)displayShareSheet;
- (void)displayErrorView;

@end

@implementation ExtendedShareViewController

+ (void)initialize {
  if (self == [ExtendedShareViewController self]) {
    crash_helper::common::StartCrashpad();
  }
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor clearColor];
  self.shareSheet = [[ShareExtensionSheet alloc] init];
  self.shareSheet.delegate = self;
  self.shareSheet.modalPresentationStyle = UIModalPresentationFormSheet;
  UISheetPresentationController* presentationController =
      self.shareSheet.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent
  ];
  presentationController.preferredCornerRadius = kShareSheetCornerRadius;

  [self loadElementsFromContext];
}

#pragma mark - ShareExtensionDelegate

- (void)didTapCloseShareExtensionSheet:
    (ShareExtensionSheet*)shareExtensionSheet {
  self.shareSheet.dismissedFromSheetAction = YES;
  __weak ExtendedShareViewController* weakSelf = self;
  [self
      queueActionItemURL:nil
                   title:nil
                  action:app_group::READING_LIST_ITEM  // Ignored
                  cancel:YES
              completion:^{
                [weakSelf
                    dismissAndReturnItem:nil
                                   error:
                                       [NSError
                                           errorWithDomain:NSCocoaErrorDomain
                                                      code:NSUserCancelledError
                                                  userInfo:nil]];
              }];
}

- (void)didTapOpenInChromeShareExtensionSheet:
    (ShareExtensionSheet*)shareExtensionSheet {
  self.shareSheet.dismissedFromSheetAction = YES;
  __weak ExtendedShareViewController* weakSelf = self;
  AppGroupCommand* command = [[AppGroupCommand alloc]
      initWithSourceApp:app_group::kOpenCommandSourceShareExtension
         URLOpenerBlock:^(NSURL* openURL) {
           ExtensionOpenURL(openURL, weakSelf, nil);
         }];
  [command prepareToOpenURL:_shareURL];
  [command executeInApp];

  [self queueActionItemURL:_shareURL
                     title:_shareTitle
                    action:app_group::OPEN_IN_CHROME_ITEM
                    cancel:NO
                completion:^{
                  [weakSelf dismissAndShowShareItem];
                }];
}

- (void)didTapMoreOptionsShareExtensionSheet:
    (ShareExtensionSheet*)shareExtensionSheet {
  UIAlertController* moreActionsAlertController = [UIAlertController
      alertControllerWithTitle:nil
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  NSString* cancelTitle = NSLocalizedString(
      @"IDS_IOS_CANCEL_BUTTON_SHARE_EXTENSION",
      @"The label of the cancel alert button in share extension.");
  UIAlertAction* cancelAlertAction =
      [UIAlertAction actionWithTitle:cancelTitle
                               style:UIAlertActionStyleCancel
                             handler:nil];

  [moreActionsAlertController addAction:[self addToBookmarksAlertAction]];
  [moreActionsAlertController addAction:[self addToReadingListAlertAction]];
  [moreActionsAlertController addAction:[self openInIncognitoAlertAction]];
  [moreActionsAlertController addAction:cancelAlertAction];

  moreActionsAlertController.popoverPresentationController.sourceView =
      self.shareSheet.secondaryActionButton;
  moreActionsAlertController.popoverPresentationController.sourceRect =
      self.shareSheet.secondaryActionButton.bounds;

  [self.shareSheet presentViewController:moreActionsAlertController
                                animated:YES
                              completion:nil];
}

- (void)didTapSearchInChromeShareExtensionSheet:
    (ShareExtensionSheet*)shareExtensionSheet {
  self.shareSheet.dismissedFromSheetAction = YES;
  CHECK(!self.shareURL);
  __weak ExtendedShareViewController* weakSelf = self;
  AppGroupCommand* command = [[AppGroupCommand alloc]
      initWithSourceApp:app_group::kOpenCommandSourceShareExtension
         URLOpenerBlock:^(NSURL* openURL) {
           ExtensionOpenURL(openURL, weakSelf, nil);
         }];
  if (self.shareText) {
    [command prepareToSearchText:self.shareText];
    [command executeInApp];
    [self queueActionItemURL:_shareURL
                       title:_shareText
                      action:app_group::TEXT_SEARCH_ITEM
                      cancel:NO
                  completion:^{
                    [weakSelf dismissAndReturnItem:weakSelf.shareItem
                                             error:nil];
                  }];
    return;
  }

  if (self.shareImageData) {
    [command prepareToSearchImageData:self.shareImageData
                           completion:^{
                             [weakSelf handleImageSharingForCommand:command
                                                          incognito:NO];
                           }];

    return;
  }
}

- (void)didTapSearchInIncognitoShareExtensionSheet:
    (ShareExtensionSheet*)shareExtensionSheet {
  self.shareSheet.dismissedFromSheetAction = YES;
  CHECK(!self.shareURL);
  __weak ExtendedShareViewController* weakSelf = self;
  AppGroupCommand* command = [[AppGroupCommand alloc]
      initWithSourceApp:app_group::kOpenCommandSourceShareExtension
         URLOpenerBlock:^(NSURL* openURL) {
           ExtensionOpenURL(openURL, weakSelf, nil);
         }];
  if (self.shareText) {
    [command prepareToIncognitoSearchText:self.shareText];
    [command executeInApp];
    [self queueActionItemURL:_shareURL
                       title:_shareText
                      action:app_group::INCOGNITO_TEXT_SEARCH_ITEM
                      cancel:NO
                  completion:^{
                    [weakSelf dismissAndReturnItem:weakSelf.shareItem
                                             error:nil];
                  }];
    return;
  }

  if (self.shareImageData) {
    [command prepareToIncognitoSearchImageData:self.shareImageData
                                    completion:^{
                                      [weakSelf
                                          handleImageSharingForCommand:command
                                                             incognito:YES];
                                    }];
    return;
  }
}

- (void)shareExtensionSheetDidDisappear:
    (ShareExtensionSheet*)shareExtensionSheet {
  [self
      handleSheetDismissalForItem:nil
                            error:[NSError errorWithDomain:NSCocoaErrorDomain
                                                      code:NSUserCancelledError
                                                  userInfo:nil]];
}

#pragma mark - Private methods

- (void)handleImageSharingForCommand:(AppGroupCommand*)command
                           incognito:(BOOL)incognito {
  __weak ExtendedShareViewController* weakSelf = self;
  app_group::ShareExtensionItemType action =
      (incognito) ? app_group::INCOGNITO_IMAGE_SEARCH_ITEM
                  : app_group::IMAGE_SEARCH_ITEM;
  [command executeInApp];
  [self queueActionItemURL:_shareURL
                     title:_shareTitle
                    action:action
                    cancel:NO
                completion:^{
                  [weakSelf dismissAndShowShareItem];
                }];
}

- (void)displayShareSheet {
  if (_shareURL) {
    self.shareSheet.sharedURL = _shareURL;
    self.shareSheet.sharedTitle = _shareTitle;
  } else if (_shareImage) {
    self.shareSheet.sharedImage = _shareImage;
  } else if (_shareText) {
    self.shareSheet.sharedText = _shareText;
    self.shareSheet.displayMaxLimit = _characterLimitReached;
  }

  __weak ExtendedShareViewController* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf presentViewController:weakSelf.shareSheet
                           animated:YES
                         completion:nil];
  });
}

- (void)displayErrorView {
  __weak ExtendedShareViewController* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf displayErrorViewMainThread];
  });
}

- (void)displayErrorViewMainThread {
  NSString* errorMessage =
      NSLocalizedString(@"IDS_IOS_ERROR_MESSAGE_SHARE_EXTENSION",
                        @"The error message to display to the user.");
  NSString* okButton =
      NSLocalizedString(@"IDS_IOS_OK_BUTTON_SHARE_EXTENSION",
                        @"The label of the OK button in share extension.");
  NSString* applicationName = [[base::apple::FrameworkBundle() infoDictionary]
      valueForKey:@"CFBundleDisplayName"];
  errorMessage =
      [errorMessage stringByReplacingOccurrencesOfString:@"APPLICATION_NAME"
                                              withString:applicationName];
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:errorMessage
                                          message:[_shareURL absoluteString]
                                   preferredStyle:UIAlertControllerStyleAlert];
  __weak ExtendedShareViewController* weakSelf = self;
  UIAlertAction* defaultAction = [UIAlertAction
      actionWithTitle:okButton
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                NSError* unsupportedURLError =
                    [NSError errorWithDomain:NSURLErrorDomain
                                        code:NSURLErrorUnsupportedURL
                                    userInfo:nil];
                [weakSelf handleSheetDismissalForItem:nil
                                                error:unsupportedURLError];
              }];
  [alert addAction:defaultAction];
  [self presentViewController:alert animated:YES completion:nil];
}

- (void)handleURL:(id)idURL
          forItem:(NSExtensionItem*)item
        withError:(NSError*)error {
  NSURL* URL = base::apple::ObjCCast<NSURL>(idURL);
  if (!URL) {
    [self displayErrorView];
    return;
  }
  self.shareItem = item;
  self.shareURL = URL;
  self.shareTitle = [[item attributedContentText] string];
  if ([self.shareTitle length] == 0) {
    self.shareTitle = [URL host];
  }
  if ([[self.shareURL scheme] isEqualToString:@"http"] ||
      [[self.shareURL scheme] isEqualToString:@"https"]) {
    self.URLLoaded = YES;
    if (self.imageLoaded) {
      [self displayShareSheet];
    }
  } else {
    [self displayErrorView];
  }
}

- (void)handlePreview:(id)idPreview
              forItem:(NSExtensionItem*)item
            withError:(NSError*)error {
  self.preview = base::apple::ObjCCast<UIImage>(idPreview);
  if (self.preview) {
    self.shareSheet.sharedURLPreview = self.preview;
  }
  self.imageLoaded = YES;
  if (self.URLLoaded) {
    [self displayShareSheet];
  }
}

- (void)handleSharedImage:(id)idImage
                  forItem:(NSExtensionItem*)item
                withError:(NSError*)error {
  if ([idImage isKindOfClass:[UIImage class]]) {
    self.shareImage = base::apple::ObjCCast<UIImage>(idImage);
  } else if ([idImage isKindOfClass:[NSURL class]]) {
    self.shareImage = [UIImage
        imageWithData:[NSData
                          dataWithContentsOfURL:base::apple::ObjCCast<NSURL>(
                                                    idImage)]];
  }

  self.shareItem = item;
  if (self.shareImage) {
    [self resizeAndScaleShareImage];
    [self displayShareSheet];
  } else {
    [self displayErrorView];
  }
}

- (void)resizeAndScaleShareImage {
  CHECK(self.shareImage);
  CGSize originalSize = self.shareImage.size;
  if ((originalSize.width > 0 && originalSize.height > 0) &&
      (originalSize.width > kShareImageTargetWidth ||
       originalSize.height > kShareImageTargetHeight)) {
    CGSize targetSize =
        CGSizeMake(kShareImageTargetWidth, kShareImageTargetHeight);
    CGSize scaledSize;

    CGFloat widthRatio = targetSize.width / originalSize.width;
    CGFloat heightRatio = targetSize.height / originalSize.height;

    if (heightRatio < widthRatio) {
      scaledSize =
          CGSizeMake(originalSize.width * heightRatio, targetSize.height);
    } else {
      scaledSize =
          CGSizeMake(targetSize.width, originalSize.height * widthRatio);
    }

    self.shareImage =
        [self.shareImage imageByPreparingThumbnailOfSize:scaledSize];
  }

  self.shareImageData = UIImageJPEGRepresentation(self.shareImage, 1.0);
}

- (void)handleText:(id)idText
           forItem:(NSExtensionItem*)item
         withError:(NSError*)error {
  NSString* shareText = [[item attributedContentText] string];
  if ([shareText length] > kSearchCharacterLimit) {
    self.shareText =
        [shareText substringWithRange:NSMakeRange(0, kSearchCharacterLimit)];
    self.characterLimitReached = YES;
  } else {
    self.shareText = shareText;
    self.characterLimitReached = NO;
  }
  self.shareText = [[item attributedContentText] string];
  self.shareItem = item;

  if (self.shareText) {
    [self displayShareSheet];
  } else {
    [self displayErrorView];
  }
}

- (void)loadElementsFromContext {
  NSString* typeURL = UTTypeURL.identifier;
  NSString* typeImage = UTTypeImage.identifier;
  NSString* typeText = UTTypeText.identifier;
  // TODO(crbug.com/40278725): Reorganize sharing extension handler.
  for (NSExtensionItem* item in self.extensionContext.inputItems) {
    for (NSItemProvider* itemProvider in item.attachments) {
      if ([itemProvider hasItemConformingToTypeIdentifier:typeURL]) {
        [self handleURLItem:item itemProvider:itemProvider];
        return;
      }
    }
  }

  for (NSExtensionItem* item in self.extensionContext.inputItems) {
    for (NSItemProvider* itemProvider in item.attachments) {
      if ([itemProvider hasItemConformingToTypeIdentifier:typeImage]) {
        [self handleImageItem:item itemProvider:itemProvider];
        return;
      }
    }
  }

  for (NSExtensionItem* item in self.extensionContext.inputItems) {
    for (NSItemProvider* itemProvider in item.attachments) {
      if ([itemProvider hasItemConformingToTypeIdentifier:typeText]) {
        [self handleTextItem:item itemProvider:itemProvider];
        return;
      }
    }
  }

  // Display the error view when no match have been found.
  [self displayErrorView];
}

- (void)dismissAndReturnItem:(NSExtensionItem*)item error:(NSError*)error {
  __weak ExtendedShareViewController* weakSelf = self;
  [self.shareSheet.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakSelf handleSheetDismissalForItem:item
                                                           error:error];
                         }];
}

- (void)queueActionItemURL:(NSURL*)URL
                     title:(NSString*)title
                    action:(app_group::ShareExtensionItemType)actionType
                    cancel:(BOOL)cancel
                completion:(ProceduralBlock)completion {
  NSURL* readingListURL = app_group::ExternalCommandsItemsFolder();
  if (![[NSFileManager defaultManager]
          fileExistsAtPath:[readingListURL path]]) {
    [[NSFileManager defaultManager] createDirectoryAtPath:[readingListURL path]
                              withIntermediateDirectories:YES
                                               attributes:nil
                                                    error:nil];
  }
  NSDate* date = [NSDate date];
  NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
  // This format sorts files by alphabetical order.
  [dateFormatter setDateFormat:@"yyyy-MM-dd-HH-mm-ss.SSSSSS"];
  NSTimeZone* timeZone = [NSTimeZone timeZoneWithName:@"UTC"];
  [dateFormatter setTimeZone:timeZone];
  NSString* dateString = [dateFormatter stringFromDate:date];
  NSURL* fileURL = [readingListURL URLByAppendingPathComponent:dateString
                                                   isDirectory:NO];

  NSMutableDictionary* dict = [[NSMutableDictionary alloc] init];
  if (URL) {
    [dict setObject:URL forKey:app_group::kShareItemURL];
  }
  if (title) {
    [dict setObject:title forKey:app_group::kShareItemTitle];
  }
  [dict setObject:date forKey:app_group::kShareItemDate];
  [dict setObject:app_group::kShareItemSourceShareExtension
           forKey:app_group::kShareItemSource];

  if (!cancel) {
    NSNumber* entryType = [NSNumber numberWithInteger:actionType];
    [dict setObject:entryType forKey:app_group::kShareItemType];
  }

  [dict setValue:[NSNumber numberWithBool:cancel]
          forKey:app_group::kShareItemCancel];
  NSError* error = nil;
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:dict
                                       requiringSecureCoding:NO
                                                       error:&error];

  if (!data || error) {
    DLOG(WARNING) << "Error serializing data for title: "
                  << base::SysNSStringToUTF8(title)
                  << base::SysNSStringToUTF8([error description]);
    return;
  }

  [[NSFileManager defaultManager] createFileAtPath:[fileURL path]
                                          contents:data
                                        attributes:nil];
  if (completion) {
    completion();
  }
}

- (void)handleURLItem:(NSExtensionItem*)item
         itemProvider:(NSItemProvider*)itemProvider {
  NSString* typeURL = UTTypeURL.identifier;
  __weak ExtendedShareViewController* weakSelf = self;
  ItemBlock URLCompletion = ^(id idURL, NSError* error) {
    // Crash reports showed that this block can be called on a background
    // thread. Move back the UI updating code to main thread.
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf handleURL:idURL forItem:item withError:error];
    });
  };
  [itemProvider loadItemForTypeIdentifier:typeURL
                                  options:nil
                        completionHandler:URLCompletion];
  ItemBlock imageCompletion = ^(id previewData, NSError* error) {
    // Crash reports showed that this block can be called on a background
    // thread. Move back the UI updating code to main thread.
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf handlePreview:previewData forItem:item withError:error];
    });
  };
  [itemProvider loadPreviewImageWithOptions:@{}
                          completionHandler:imageCompletion];
}

- (void)handleImageItem:(NSExtensionItem*)item
           itemProvider:(NSItemProvider*)itemProvider {
  NSString* typeImage = UTTypeImage.identifier;
  __weak ExtendedShareViewController* weakSelf = self;
  ItemBlock imageCompletion = ^(id idImage, NSError* error) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf handleSharedImage:idImage forItem:item withError:error];
    });
  };
  [itemProvider loadItemForTypeIdentifier:typeImage
                                  options:nil
                        completionHandler:imageCompletion];
}

- (void)handleTextItem:(NSExtensionItem*)item
          itemProvider:(NSItemProvider*)itemProvider {
  NSString* typeText = UTTypeText.identifier;
  __weak ExtendedShareViewController* weakSelf = self;
  ItemBlock textCompletion = ^(id idText, NSError* error) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf handleText:idText forItem:item withError:error];
    });
  };
  [itemProvider loadItemForTypeIdentifier:typeText
                                  options:nil
                        completionHandler:textCompletion];
}

- (void)handleSheetDismissalForItem:(NSExtensionItem*)item
                              error:(NSError*)error {
  NSArray* returnItem = item ? @[ item ] : @[];
  if (error) {
    [self.extensionContext cancelRequestWithError:error];
  } else {
    [self.extensionContext completeRequestReturningItems:returnItem
                                       completionHandler:nil];
  }
}

- (UIAlertAction*)addToBookmarksAlertAction {
  __weak ExtendedShareViewController* weakSelf = self;
  NSString* addToBookmarksTitle = NSLocalizedString(
      @"IDS_IOS_ADD_BOOKMARKS_SHARE_EXTENSION",
      @"The Add to bookmarks button text in share extension.");
  return [UIAlertAction actionWithTitle:addToBookmarksTitle
                                  style:UIAlertActionStyleDefault
                                handler:^(UIAlertAction* action) {
                                  [weakSelf handleAddingToBookmark];
                                }];
}

- (UIAlertAction*)addToReadingListAlertAction {
  __weak ExtendedShareViewController* weakSelf = self;
  NSString* addToReadingListTitle = NSLocalizedString(
      @"IDS_IOS_ADD_READING_LIST_SHARE_EXTENSION",
      @"The add to reading list button text in share extension.");
  return [UIAlertAction actionWithTitle:addToReadingListTitle
                                  style:UIAlertActionStyleDefault
                                handler:^(UIAlertAction* action) {
                                  [weakSelf handleAddingToReadingList];
                                }];
}

- (UIAlertAction*)openInIncognitoAlertAction {
  __weak ExtendedShareViewController* weakSelf = self;
  NSString* openInIncognitoTitle = NSLocalizedString(
      @"IDS_IOS_OPEN_IN_INCOGNITO_BUTTON_SHARE_EXTENSION",
      @"The add to reading list button text in share extension.");
  return [UIAlertAction actionWithTitle:openInIncognitoTitle
                                  style:UIAlertActionStyleDefault
                                handler:^(UIAlertAction* action) {
                                  [weakSelf handleOpeningInIncognito];
                                }];
}

- (void)handleAddingToBookmark {
  self.shareSheet.dismissedFromSheetAction = YES;
  __weak ExtendedShareViewController* weakSelf = self;
  [self queueActionItemURL:_shareURL
                     title:_shareTitle
                    action:app_group::BOOKMARK_ITEM
                    cancel:NO
                completion:^{
                  [weakSelf dismissAndReturnItem:weakSelf.shareItem error:nil];
                }];
}

- (void)handleAddingToReadingList {
  self.shareSheet.dismissedFromSheetAction = YES;
  __weak ExtendedShareViewController* weakSelf = self;
  [self queueActionItemURL:_shareURL
                     title:_shareTitle
                    action:app_group::READING_LIST_ITEM
                    cancel:NO
                completion:^{
                  [weakSelf dismissAndReturnItem:weakSelf.shareItem error:nil];
                }];
}

- (void)handleOpeningInIncognito {
  self.shareSheet.dismissedFromSheetAction = YES;
  __weak ExtendedShareViewController* weakSelf = self;
  AppGroupCommand* command = [[AppGroupCommand alloc]
      initWithSourceApp:app_group::kOpenCommandSourceShareExtension
         URLOpenerBlock:^(NSURL* openURL) {
           ExtensionOpenURL(openURL, weakSelf, nil);
         }];
  [command prepareToOpenURLInIncognito:_shareURL];
  [command executeInApp];

  [self queueActionItemURL:_shareURL
                     title:_shareTitle
                    action:app_group::OPEN_IN_CHROME_INCOGNITO_ITEM
                    cancel:NO
                completion:^{
                  [weakSelf dismissAndShowShareItem];
                }];
}

- (void)dismissAndShowShareItem {
  [self dismissAndReturnItem:_shareItem error:nil];
}
@end
