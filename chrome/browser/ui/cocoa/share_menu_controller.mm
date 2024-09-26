// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/share_menu_controller.h"

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/accelerators_cocoa.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "net/base/mac/url_conversions.h"
#include "ui/base/accelerators/platform_accelerator_cocoa.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/snapshot/snapshot.h"
#include "ui/views/view.h"

// Private method, used to identify instantiated services.
@interface NSSharingService (ExposeName)
- (id)name;
@end

namespace {

// The reminder service doesn't have a convenient NSSharingServiceName*
// constant.
NSString* const kRemindersSharingServiceName =
    @"com.apple.reminders.RemindersShareExtension";

bool CanShare() {
  Browser* last_active_browser = chrome::FindLastActive();
  return last_active_browser &&
         last_active_browser->location_bar_model()->ShouldDisplayURL() &&
         last_active_browser->tab_strip_model()->GetActiveWebContents() &&
         last_active_browser->tab_strip_model()
             ->GetActiveWebContents()
             ->GetLastCommittedURL()
             .is_valid();
}

}  // namespace

@implementation ShareMenuController {
  // The following three ivars are provided to the system via NSSharingService
  // delegates. They're needed for the transition animation, and to provide a
  // screenshot of the shared site for services that support it.
  NSWindow* _windowForShare;  // weak
  NSRect _rectForShare;
  base::scoped_nsobject<NSImage> _snapshotForShare;
  // The Reminders share extension reads title/URL from the currently active
  // activity.
  base::scoped_nsobject<NSUserActivity> _activity;
}

// NSMenuDelegate

- (BOOL)menuHasKeyEquivalent:(NSMenu*)menu
                    forEvent:(NSEvent*)event
                      target:(id*)target
                      action:(SEL*)action {
  // Load the menu if it hasn't loaded already.
  if ([menu numberOfItems] == 0) {
    [self menuNeedsUpdate:menu];
  }
  // Per tapted@'s comment in BookmarkMenuCocoaController, it's fine
  // to return NO here if an item will handle this. This is why it's
  // necessary to ensure the menu is loaded above.
  return NO;
}

- (void)menuNeedsUpdate:(NSMenu*)menu {
  [menu removeAllItems];
  [menu setAutoenablesItems:NO];

  bool canShare = CanShare();
  // Using a real URL instead of empty string to avoid system log about relative
  // URLs in the pasteboard. This URL will not actually be shared to, just used
  // to fetch sharing services that can handle the NSURL type.
  NSArray* services = [NSSharingService
      sharingServicesForItems:@[ [NSURL URLWithString:@"https://google.com"] ]];
  for (NSSharingService* service in services) {
    // Don't include "Add to Reading List".
    if ([[service name]
            isEqualToString:NSSharingServiceNameAddToSafariReadingList])
      continue;
    NSMenuItem* item = [self menuItemForService:service];
    [item setEnabled:canShare];
    [menu addItem:item];
  }
  base::scoped_nsobject<NSMenuItem> moreItem([[NSMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_SHARING_MORE_MAC)
             action:@selector(openSharingPrefs:)
      keyEquivalent:@""]);
  [moreItem setTarget:self];
  [moreItem setImage:[self moreImage]];
  [menu addItem:moreItem];
}

// NSSharingServiceDelegate

- (void)sharingService:(NSSharingService*)service
         didShareItems:(NSArray*)items {
  UMA_HISTOGRAM_BOOLEAN("Mac.FileMenuNativeShare", true);
  [self clearTransitionData];
}

- (void)sharingService:(NSSharingService*)service
    didFailToShareItems:(NSArray*)items
                  error:(NSError*)error {
  UMA_HISTOGRAM_BOOLEAN("Mac.FileMenuNativeShare", false);
  [self clearTransitionData];
}

- (NSRect)sharingService:(NSSharingService*)service
    sourceFrameOnScreenForShareItem:(id)item {
  return _rectForShare;
}

- (NSWindow*)sharingService:(NSSharingService*)service
    sourceWindowForShareItems:(NSArray*)items
          sharingContentScope:(NSSharingContentScope*)scope {
  *scope = NSSharingContentScopeFull;
  return _windowForShare;
}

- (NSImage*)sharingService:(NSSharingService*)service
    transitionImageForShareItem:(id)item
                    contentRect:(NSRect*)contentRect {
  return _snapshotForShare;
}

// Private methods

// Saves details required by delegate methods for the transition animation.
- (void)saveTransitionDataFromBrowser:(Browser*)browser {
  _windowForShare = browser->window()->GetNativeWindow().GetNativeNSWindow();
  BrowserView* browserView = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browserView)
    return;

  views::View* contentsView = browserView->contents_container();
  if (!contentsView)
    return;

  gfx::Rect screenRect = contentsView->bounds();
  views::View::ConvertRectToScreen(browserView, &screenRect);

  _rectForShare = ScreenRectToNSRect(screenRect);

  gfx::Rect rectInWidget =
      browserView->ConvertRectToWidget(contentsView->bounds());
  gfx::Image image;
  if (ui::GrabWindowSnapshot(_windowForShare, rectInWidget, &image)) {
    _snapshotForShare.reset([image.ToNSImage() retain]);
  }
}

- (void)clearTransitionData {
  _windowForShare = nil;
  _rectForShare = NSZeroRect;
  _snapshotForShare.reset();
  [_activity invalidate];
  _activity.reset();
}

// Performs the share action using the sharing service represented by |sender|.
- (void)performShare:(NSMenuItem*)sender {
  DCHECK(CanShare());
  Browser* browser = chrome::FindLastActive();
  DCHECK(browser);
  [self saveTransitionDataFromBrowser:browser];

  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  NSURL* url = net::NSURLWithGURL(contents->GetLastCommittedURL());
  NSString* title = base::SysUTF16ToNSString(contents->GetTitle());

  NSSharingService* service =
      base::mac::ObjCCastStrict<NSSharingService>([sender representedObject]);
  [service setDelegate:self];
  [service setSubject:title];

  NSArray* itemsToShare = @[ url ];
  if (@available(macOS 10.14, *)) {
  } else {
    if ([[service name] isEqual:NSSharingServiceNamePostOnTwitter]) {
      // The Twitter share service expects the title as an additional share
      // item. This is the same approach system apps use.
      itemsToShare = @[ url, title ];
    }
  }
  if ([[service name] isEqual:kRemindersSharingServiceName]) {
    _activity.reset([[NSUserActivity alloc]
        initWithActivityType:NSUserActivityTypeBrowsingWeb]);
    // webpageURL must be http or https or an exception is thrown.
    if ([url.scheme hasPrefix:@"http"]) {
      [_activity setWebpageURL:url];
    }
    [_activity setTitle:title];
    [_activity becomeCurrent];
  }
  [service performWithItems:itemsToShare];
}

// Opens the "Sharing" subpane of the "Extensions" macOS preference pane.
- (void)openSharingPrefs:(NSMenuItem*)sender {
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kPrivacySecurity_Extensions_Sharing);
}

// Returns the image to be used for the "More..." menu item, or nil on macOS
// version where this private method is unsupported.
- (NSImage*)moreImage {
  if ([NSSharingServicePicker
          respondsToSelector:@selector(sharedMoreMenuImage)]) {
    return
        [NSSharingServicePicker performSelector:@selector(sharedMoreMenuImage)];
  }
  return nil;
}

// Creates a menu item that calls |service| when invoked.
- (NSMenuItem*)menuItemForService:(NSSharingService*)service {
  BOOL isMail = [[service name] isEqual:NSSharingServiceNameComposeEmail];
  NSString* keyEquivalent = isMail ? [self keyEquivalentForMail] : @"";
  NSString* title = isMail ? l10n_util::GetNSString(IDS_EMAIL_LINK_MAC)
                           : service.menuItemTitle;
  base::scoped_nsobject<NSMenuItem> item([[NSMenuItem alloc]
      initWithTitle:title
             action:@selector(performShare:)
      keyEquivalent:keyEquivalent]);
  [item setTarget:self];
  [item setImage:[service image]];
  [item setRepresentedObject:service];
  return item.autorelease();
}

- (NSString*)keyEquivalentForMail {
  ui::Accelerator accelerator;
  bool found = GetDefaultMacAcceleratorForCommandId(IDC_EMAIL_PAGE_LOCATION,
                                                    &accelerator);
  DCHECK(found);
  NSString* key_equivalent;
  NSUInteger modifier_mask;
  GetKeyEquivalentAndModifierMaskFromAccelerator(accelerator, &key_equivalent,
                                                 &modifier_mask);
  return key_equivalent;
}

@end
