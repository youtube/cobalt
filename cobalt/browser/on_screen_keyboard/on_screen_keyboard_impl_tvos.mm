// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/on_screen_keyboard/on_screen_keyboard_impl_tvos.h"

#import <UIKit/UIKit.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "cobalt/browser/on_screen_keyboard/public/mojom/on_screen_keyboard.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "starboard/tvos/shared/starboard_application.h"

@interface OnScreenKeyboardObserver
    : NSObject <SBDOnScreenKeyboardManagerDelegate>

- (instancetype)initWithCallbacks:(base::RepeatingClosure)blurredCallback
                  focusedCallback:(base::RepeatingClosure)focusedCallback
              textChangedCallback:
                  (base::RepeatingCallback<void(const std::string&)>)
                      textChangedCallback;

@end

@implementation OnScreenKeyboardObserver {
 @private
  base::RepeatingClosure _blurredCallback;
  base::RepeatingClosure _focusedCallback;
  base::RepeatingCallback<void(const std::string&)> _textChangedCallback;
}

- (instancetype)initWithCallbacks:(base::RepeatingClosure)blurredCallback
                  focusedCallback:(base::RepeatingClosure)focusedCallback
              textChangedCallback:
                  (base::RepeatingCallback<void(const std::string&)>)
                      textChangedCallback {
  if (!(self = [super init])) {
    return nil;
  }
  _blurredCallback = std::move(blurredCallback);
  _focusedCallback = std::move(focusedCallback);
  _textChangedCallback = std::move(textChangedCallback);
  return self;
}

#pragma mark - SBDOnScreenKeyboardManagerDelegate

- (void)keyboardBlurred {
  _blurredCallback.Run();
}

- (void)keyboardFocused {
  _focusedCallback.Run();
}

- (void)keyboardTextChanged:(NSString*)text {
  _textChangedCallback.Run(base::SysNSStringToUTF8(text));
}

@end

namespace on_screen_keyboard {

namespace {

// Computes the default background color for the on-screen keyboard by first
// trying to read it from the bundle and then falling back to a hardcoded
// default.
//
// Note: this logic was copied from C25. In practice, Kabuki always ends up
// overriding the background color to a specific value.
UIColor* defaultKeyboardBackgroundColor() {
  static UIColor* background_color;
  if (!background_color) {
    NSString* colorFromPList = [[NSBundle mainBundle]
        objectForInfoDictionaryKey:@"YTOSKBackgroundColor"];
    if (!colorFromPList) {
      colorFromPList = [[NSBundle mainBundle]
          objectForInfoDictionaryKey:@"YTApplicationBackgroundColor"];
    }
    NSArray<NSString*>* colorComponents =
        [colorFromPList componentsSeparatedByString:@", "];
    if (colorComponents.count == 4) {
      background_color =
          [UIColor colorWithRed:[colorComponents[0] floatValue] / 255.0
                          green:[colorComponents[1] floatValue] / 255.0
                           blue:[colorComponents[2] floatValue] / 255.0
                          alpha:[colorComponents[3] floatValue]];
    } else {
      background_color = [UIColor colorWithRed:30 / 255.0
                                         green:30 / 255.0
                                          blue:30 / 255.0
                                         alpha:1];
    }
  }
  return background_color;
}

}  // namespace

struct OnScreenKeyboardImplTvos::ObjCStorage {
  OnScreenKeyboardObserver* __strong on_screen_keyboard_observer;
};

OnScreenKeyboardImplTvos::OnScreenKeyboardImplTvos(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::OnScreenKeyboard> receiver)
    : OnScreenKeyboardImpl(render_frame_host, std::move(receiver)),
      objc_storage_(std::make_unique<ObjCStorage>()) {
  objc_storage_->on_screen_keyboard_observer = [[OnScreenKeyboardObserver alloc]
        initWithCallbacks:base::BindRepeating(
                              &OnScreenKeyboardImplTvos::KeyboardBlurred,
                              weak_ptr_factory_.GetWeakPtr())
          focusedCallback:base::BindRepeating(
                              &OnScreenKeyboardImplTvos::KeyboardFocused,
                              weak_ptr_factory_.GetWeakPtr())
      textChangedCallback:base::BindRepeating(
                              &OnScreenKeyboardImplTvos::KeyboardTextChanged,
                              weak_ptr_factory_.GetWeakPtr())];
  SBDGetApplication().onScreenKeyboardManager.keyboardManagerDelegate =
      objc_storage_->on_screen_keyboard_observer;
}

void OnScreenKeyboardImplTvos::Show(const std::string& text,
                                    mojom::KeyboardOptionsPtr options,
                                    ShowCallback callback) {
  UIUserInterfaceStyle theme_override = UIUserInterfaceStyleUnspecified;
  if (options->theme_override.has_value()) {
    theme_override =
        *options->theme_override == mojom::ThemeOverride::kDarkTheme
            ? UIUserInterfaceStyleDark
            : UIUserInterfaceStyleLight;
  }
  UIColor* color_override = defaultKeyboardBackgroundColor();
  if (options->background_color) {
    color_override =
        [UIColor colorWithRed:options->background_color->red / 255.0
                        green:options->background_color->green / 255.0
                         blue:options->background_color->blue / 255.0
                        alpha:1];
  }

  [SBDGetApplication().onScreenKeyboardManager
      showOnScreenKeyboard:base::SysUTF8ToNSString(text)
             keyboardStyle:theme_override
             colorOverride:color_override
         completionHandler:base::CallbackToBlock(std::move(callback))];
}

void OnScreenKeyboardImplTvos::Hide(HideCallback callback) {
  [SBDGetApplication().onScreenKeyboardManager hideOnScreenKeyboard];
  std::move(callback).Run();
}

void OnScreenKeyboardImplTvos::Focus(FocusCallback callback) {
  [SBDGetApplication().onScreenKeyboardManager
      focusOnScreenKeyboard:base::CallbackToBlock(std::move(callback))];
}

void OnScreenKeyboardImplTvos::Blur(BlurCallback callback) {
  // TODO: b/513162149 - This method was present in C25 and added here for
  // backward compatibility. The current implementation is empty because not
  // only is it not called from Kabuki but it is not entirely clear what should
  // happen here given how the tvOS search keyboard is presented on the screen:
  // if this is called from Kabuki, the web view already has focus anyway.
  std::move(callback).Run();
}

void OnScreenKeyboardImplTvos::UpdateSuggestions(
    const std::vector<std::string>& suggestions,
    UpdateSuggestionsCallback callback) {
  std::move(callback).Run();
}

void OnScreenKeyboardImplTvos::KeepFocus(bool keep_focus) {
  // Do nothing on tvOS. Focus handling works as expected without having to
  // force it to be on the keyboard (and having to deal with
  // -preferredFocusEnvironments when focus() and blur() are called).
}

void OnScreenKeyboardImplTvos::SupportsSuggestions(
    SupportsSuggestionsCallback callback) {
  std::move(callback).Run(false);
}

void OnScreenKeyboardImplTvos::IsShown(IsShownCallback callback) {
  std::move(callback).Run(
      SBDGetApplication().onScreenKeyboardManager.isShowing);
}

void OnScreenKeyboardImplTvos::BoundingRect(BoundingRectCallback callback) {
  std::move(callback).Run(
      gfx::RectF(SBDGetApplication().onScreenKeyboardManager.boundingRect));
}

}  // namespace on_screen_keyboard
