// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_VIRTUAL_KEYBOARD_PRIVATE_VIRTUAL_KEYBOARD_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_VIRTUAL_KEYBOARD_PRIVATE_VIRTUAL_KEYBOARD_PRIVATE_API_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
class ClipboardHistoryItem;
}

namespace content {
class BrowserContext;
}

namespace extensions {

class VirtualKeyboardDelegate;

class VirtualKeyboardPrivateFunction : public ExtensionFunction {
 public:
  bool PreRunValidation(std::string* error) override;

 protected:
  VirtualKeyboardDelegate* delegate() { return delegate_; }
  ~VirtualKeyboardPrivateFunction() override = default;

 private:
  raw_ptr<VirtualKeyboardDelegate> delegate_ = nullptr;
};

class VirtualKeyboardPrivateInsertTextFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.insertText",
                             VIRTUALKEYBOARDPRIVATE_INSERTTEXT)

 protected:
  ~VirtualKeyboardPrivateInsertTextFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class VirtualKeyboardPrivateSendKeyEventFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.sendKeyEvent",
                             VIRTUALKEYBOARDPRIVATE_SENDKEYEVENT)

 protected:
  ~VirtualKeyboardPrivateSendKeyEventFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class VirtualKeyboardPrivateHideKeyboardFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.hideKeyboard",
                             VIRTUALKEYBOARDPRIVATE_HIDEKEYBOARD)

 protected:
  ~VirtualKeyboardPrivateHideKeyboardFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class VirtualKeyboardPrivateSetHotrodKeyboardFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.setHotrodKeyboard",
                             VIRTUALKEYBOARDPRIVATE_SETHOTRODKEYBOARD)

 protected:
  ~VirtualKeyboardPrivateSetHotrodKeyboardFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class VirtualKeyboardPrivateLockKeyboardFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.lockKeyboard",
                             VIRTUALKEYBOARDPRIVATE_LOCKKEYBOARD)

 protected:
  ~VirtualKeyboardPrivateLockKeyboardFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class VirtualKeyboardPrivateKeyboardLoadedFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.keyboardLoaded",
                             VIRTUALKEYBOARDPRIVATE_KEYBOARDLOADED)

 protected:
  ~VirtualKeyboardPrivateKeyboardLoadedFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class VirtualKeyboardPrivateGetKeyboardConfigFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.getKeyboardConfig",
                             VIRTUALKEYBOARDPRIVATE_GETKEYBOARDCONFIG)

 protected:
  ~VirtualKeyboardPrivateGetKeyboardConfigFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnKeyboardConfig(absl::optional<base::Value::Dict> results);
};

class VirtualKeyboardPrivateOpenSettingsFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.openSettings",
                             VIRTUALKEYBOARDPRIVATE_OPENSETTINGS)

 protected:
  ~VirtualKeyboardPrivateOpenSettingsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class VirtualKeyboardPrivateOpenSuggestionSettingsFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.openSuggestionSettings",
                             VIRTUALKEYBOARDPRIVATE_OPENSUGGESTIONSETTINGS)

 protected:
  ~VirtualKeyboardPrivateOpenSuggestionSettingsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class VirtualKeyboardPrivateSetContainerBehaviorFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.setContainerBehavior",
                             VIRTUALKEYBOARDPRIVATE_SETCONTAINERBEHAVIOR)

 protected:
  ~VirtualKeyboardPrivateSetContainerBehaviorFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnSetContainerBehavior(bool success);
};

class VirtualKeyboardPrivateSetDraggableAreaFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.setDraggableArea",
                             VIRTUALKEYBOARDPRIVATE_SETDRAGGABLEAREA)

 protected:
  ~VirtualKeyboardPrivateSetDraggableAreaFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class VirtualKeyboardPrivateSetKeyboardStateFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.setKeyboardState",
                             VIRTUALKEYBOARDPRIVATE_SETKEYBOARDSTATE)

 protected:
  ~VirtualKeyboardPrivateSetKeyboardStateFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class VirtualKeyboardPrivateSetOccludedBoundsFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.setOccludedBounds",
                             VIRTUALKEYBOARDPRIVATE_SETOCCLUDEDBOUNDS)

 protected:
  ~VirtualKeyboardPrivateSetOccludedBoundsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class VirtualKeyboardPrivateSetHitTestBoundsFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.setHitTestBounds",
                             VIRTUALKEYBOARDPRIVATE_SETHITTESTBOUNDS)

 protected:
  ~VirtualKeyboardPrivateSetHitTestBoundsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class VirtualKeyboardPrivateSetAreaToRemainOnScreenFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.setAreaToRemainOnScreen",
                             VIRTUALKEYBOARDPRIVATE_SETAREATOREMAINONSCREEN)

 protected:
  ~VirtualKeyboardPrivateSetAreaToRemainOnScreenFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class VirtualKeyboardPrivateSetWindowBoundsInScreenFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.setWindowBoundsInScreen",
                             VIRTUALKEYBOARDPRIVATE_SETWINDOWBOUNDSINSCREEN)

 protected:
  ~VirtualKeyboardPrivateSetWindowBoundsInScreenFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
class VirtualKeyboardPrivateGetClipboardHistoryFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.getClipboardHistory",
                             VIRTUALKEYBOARDPRIVATE_GETCLIPBOARDHISTORY)

 protected:
  ~VirtualKeyboardPrivateGetClipboardHistoryFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnGetClipboardHistory(std::vector<ash::ClipboardHistoryItem> items);
};

class VirtualKeyboardPrivatePasteClipboardItemFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.pasteClipboardItem",
                             VIRTUALKEYBOARDPRIVATE_PASTECLIPBOARDITEM)

 protected:
  ~VirtualKeyboardPrivatePasteClipboardItemFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class VirtualKeyboardPrivateDeleteClipboardItemFunction
    : public VirtualKeyboardPrivateFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("virtualKeyboardPrivate.deleteClipboardItem",
                             VIRTUALKEYBOARDPRIVATE_DELETECLIPBOARDITEM)

 protected:
  ~VirtualKeyboardPrivateDeleteClipboardItemFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class VirtualKeyboardDelegate;

class VirtualKeyboardAPI : public BrowserContextKeyedAPI {
 public:
  explicit VirtualKeyboardAPI(content::BrowserContext* context);
  ~VirtualKeyboardAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<VirtualKeyboardAPI>*
  GetFactoryInstance();

  VirtualKeyboardDelegate* delegate() { return delegate_.get(); }

 private:
  friend class BrowserContextKeyedAPIFactory<VirtualKeyboardAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "VirtualKeyboardAPI"; }

  // Require accces to delegate while incognito or during login.
  static const bool kServiceHasOwnInstanceInIncognito = true;

  std::unique_ptr<VirtualKeyboardDelegate> delegate_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_VIRTUAL_KEYBOARD_PRIVATE_VIRTUAL_KEYBOARD_PRIVATE_API_H_
