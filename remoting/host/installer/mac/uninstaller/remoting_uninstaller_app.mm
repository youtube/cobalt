// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/mac/uninstaller/remoting_uninstaller_app.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/installer/mac/uninstaller/remoting_uninstaller.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

base::scoped_nsobject<NSMenu> BuildMainMenu() {
  base::scoped_nsobject<NSMenu> main_menu([[NSMenu alloc] initWithTitle:@""]);

  NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle:@""
                                                 action:NULL
                                          keyEquivalent:@""] autorelease];

  // The title is not used, as the title will always be the name of the App.
  item.submenu = [[[NSMenu alloc] initWithTitle:@""] autorelease];
  [item.submenu addItem:[[[NSMenuItem alloc] initWithTitle:@"Close"
                                                    action:@selector(terminate:)
                                             keyEquivalent:@"w"] autorelease]];
  [item.submenu
      addItem:[[[NSMenuItem alloc]
                  initWithTitle:@"Quit Chrome Remote Desktop Uninstaller"
                         action:@selector(terminate:)
                  keyEquivalent:@"q"] autorelease]];
  [main_menu addItem:item];

  return main_menu;
}

}  // namespace

@interface RemotingUninstallerAppDelegate () <NSApplicationDelegate>
@property(nonatomic, retain) NSWindow* window;
@end

@implementation RemotingUninstallerAppDelegate
@synthesize window = _window;

- (void)dealloc {
  [_window release];
  [super dealloc];
}

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification {
  base::scoped_nsobject<NSMenu> main_menu = BuildMainMenu();
  [[NSApplication sharedApplication] setMainMenu:main_menu];

  NSRect frame = NSMakeRect(0, 0, 499, 112);
  self.window = [[[NSWindow alloc] initWithContentRect:frame
                                             styleMask:NSWindowStyleMaskTitled
                                               backing:NSBackingStoreBuffered
                                                 defer:NO] autorelease];
  self.window.title = @"Chrome Remote Desktop Uninstaller";

  NSTextField* title = [[[NSTextField alloc]
      initWithFrame:NSMakeRect(103, 75, 379, 17)] autorelease];
  title.stringValue =
      @"This will completely remove Chrome Remote Desktop Host.";
  title.drawsBackground = NO;
  title.bezeled = NO;
  title.editable = NO;
  title.font = [NSFont systemFontOfSize:13];
  [self.window.contentView addSubview:title];

  NSImageView* icon = [[[NSImageView alloc]
      initWithFrame:NSMakeRect(8, 24, 76, 68)] autorelease];
  icon.image = [[NSApplication sharedApplication] applicationIconImage];
  [self.window.contentView addSubview:icon];

  NSButton* cancelButton = [[[NSButton alloc]
      initWithFrame:NSMakeRect(308, 13, 82, 32)] autorelease];
  cancelButton.buttonType = NSButtonTypeMomentaryPushIn;
  cancelButton.bezelStyle = NSBezelStyleRounded;
  cancelButton.title = @"Cancel";
  cancelButton.action = @selector(cancel:);
  cancelButton.target = self;
  [self.window.contentView addSubview:cancelButton];

  NSButton* uninstallButton = [[[NSButton alloc]
      initWithFrame:NSMakeRect(390, 13, 95, 32)] autorelease];
  uninstallButton.buttonType = NSButtonTypeMomentaryPushIn;
  uninstallButton.bezelStyle = NSBezelStyleRounded;
  uninstallButton.title = @"Uninstall";
  uninstallButton.action = @selector(uninstall:);
  uninstallButton.target = self;
  [self.window.contentView addSubview:uninstallButton];

  [self.window makeKeyAndOrderFront:NSApp];
  [self.window center];
}

- (void)showSuccess:(bool)success withMessage:(NSString*)message {
  NSString* summary = success ? @"Uninstall succeeded" : @"Uninstall failed";
  base::scoped_nsobject<NSAlert> alert([[NSAlert alloc] init]);
  [alert setMessageText:summary];
  [alert setInformativeText:message];
  [alert setAlertStyle:(success ? NSAlertStyleInformational
                                : NSAlertStyleCritical)];
  // This line crashes the app because ui::ResourceBundle::GetSharedInstance()
  // cannot find a shared instance. https://crbug.com/968257.
  [alert addButtonWithTitle:l10n_util::GetNSString(IDS_OK)];
  [alert runModal];
}

- (IBAction)uninstall:(NSButton*)sender {
  @try {
    NSLog(@"Chrome Remote Desktop uninstall starting.");

    RemotingUninstaller* uninstaller =
        [[[RemotingUninstaller alloc] init] autorelease];
    OSStatus status = [uninstaller remotingUninstall];

    NSLog(@"Chrome Remote Desktop Host uninstall complete.");

    bool success = false;
    NSString* message = nullptr;
    if (status == errAuthorizationSuccess) {
      success = true;
      message = @"Chrome Remote Desktop Host successfully uninstalled.";
    } else if (status == errAuthorizationCanceled) {
      message = @"Chrome Remote Desktop Host uninstall canceled.";
    } else if (status == errAuthorizationDenied) {
      message = @"Chrome Remote Desktop Host uninstall authorization denied.";
    } else {
      [NSException raise:@"AuthorizationCopyRights Failure"
                  format:@"Error during AuthorizationCopyRights status=%d",
                         static_cast<int>(status)];
    }
    if (message != nullptr) {
      NSLog(@"Uninstall %s: %@", success ? "succeeded" : "failed", message);
      [self showSuccess:success withMessage:message];
    }
  } @catch (NSException* exception) {
    NSLog(@"Exception %@ %@", [exception name], [exception reason]);
    NSString* message =
        @"Error! Unable to uninstall Chrome Remote Desktop Host.";
    [self showSuccess:false withMessage:message];
  }

  [NSApp terminate:self];
}

- (IBAction)cancel:(id)sender {
  [NSApp terminate:self];
}

- (IBAction)handleMenuClose:(NSMenuItem*)sender {
  [NSApp terminate:self];
}

@end

int main(int argc, char* argv[]) {
  // The no-ui option skips the UI confirmation dialogs. This is provided as
  // a convenience for our automated testing.
  // There will still be an elevation prompt unless the command is run as root.
  @autoreleasepool {
    if (argc == 2 && !strcmp(argv[1], "--no-ui")) {
      NSLog(@"Chrome Remote Desktop uninstall starting.");
      NSLog(@"--no-ui : Suppressing UI");

      RemotingUninstaller* uninstaller =
          [[[RemotingUninstaller alloc] init] autorelease];
      OSStatus status = [uninstaller remotingUninstall];

      NSLog(@"Chrome Remote Desktop Host uninstall complete.");
      NSLog(@"Status = %d", static_cast<int>(status));
      return status != errAuthorizationSuccess;
    } else {
      RemotingUninstallerAppDelegate* delegate =
          [[[RemotingUninstallerAppDelegate alloc] init] autorelease];

      [NSApplication sharedApplication];
      [NSApp setDelegate:delegate];
      [NSApp activateIgnoringOtherApps:YES];
      [NSApp run];

      return EXIT_SUCCESS;
    }
  }
}
