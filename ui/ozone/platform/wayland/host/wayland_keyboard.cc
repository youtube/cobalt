// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_keyboard.h"

#include <keyboard-extension-unstable-v1-client-protocol.h>
#include <keyboard-shortcuts-inhibit-unstable-v1-client-protocol.h>
#include <sys/mman.h>

#include <cstddef>
#include <cstring>
#include <utility>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/unguessable_token.h"
#include "ui/base/buildflags.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/keyboard/event_auto_repeat_handler.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/public/platform_keyboard_hook.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#endif

namespace ui {

namespace {

bool IsModifierKey(int key) {
  auto dom_code = KeycodeConverter::EvdevCodeToDomCode(key);
  switch (dom_code) {
    // Based on ui::NonPrintableCodeEntry map.
    case DomCode::ALT_LEFT:
    case DomCode::ALT_RIGHT:
    case DomCode::SHIFT_LEFT:
    case DomCode::SHIFT_RIGHT:
    case DomCode::CONTROL_LEFT:
    case DomCode::CONTROL_RIGHT:
    case DomCode::FN:
    case DomCode::FN_LOCK:
    case DomCode::HYPER:
    case DomCode::META_LEFT:
    case DomCode::META_RIGHT:
    case DomCode::CAPS_LOCK:
    case DomCode::NUM_LOCK:
    case DomCode::SCROLL_LOCK:
    case DomCode::SUPER:
      return true;
    default:
      return false;
  }
}

class WaylandKeyboardHook final : public PlatformKeyboardHook {
 public:
  explicit WaylandKeyboardHook(
      wl::Object<zwp_keyboard_shortcuts_inhibitor_v1> inhibitor)
      : inhibitor_(std::move(inhibitor)) {}
  WaylandKeyboardHook(const WaylandKeyboardHook&) = delete;
  WaylandKeyboardHook& operator=(const WaylandKeyboardHook&) = delete;
  ~WaylandKeyboardHook() final = default;

  // In Linux Desktop, the keyboard-lock implementation relies solely on
  // keyboard-shortcuts-inhibit-unstable-v1 protocol, which currently does
  // not support to specify a set of key codes to be captured nor a way
  // of reporting back to the compositor which keys were consumed or not (which
  // is done through zcr-keyboard-extension in Lacros), so it's not possible to
  // implement this until the protocol supports it.
  //
  // TODO(crbug.com/1408927): Update once it is supported in the protocol.
  bool IsKeyLocked(DomCode dom_code) const final {
    NOTIMPLEMENTED_LOG_ONCE();
    return true;
  }

 private:
  const wl::Object<zwp_keyboard_shortcuts_inhibitor_v1> inhibitor_;
};

}  // namespace

class WaylandKeyboard::ZCRExtendedKeyboard {
 public:
  // Takes the ownership of |extended_keyboard|.
  ZCRExtendedKeyboard(WaylandKeyboard* keyboard,
                      zcr_extended_keyboard_v1* extended_keyboard)
      : keyboard_(keyboard), obj_(extended_keyboard) {
    static constexpr zcr_extended_keyboard_v1_listener kListener = {
        &PeekKey,
    };
    zcr_extended_keyboard_v1_add_listener(obj_.get(), &kListener, this);
  }
  ZCRExtendedKeyboard(const ZCRExtendedKeyboard&) = delete;
  ZCRExtendedKeyboard& operator=(const ZCRExtendedKeyboard&) = delete;
  ~ZCRExtendedKeyboard() = default;

  void AckKey(uint32_t serial, bool handled) {
    zcr_extended_keyboard_v1_ack_key(obj_.get(), serial, handled);
    keyboard_->connection_->Flush();
  }

  // Returns true if connected object will send zcr_extended_keyboard::peek_key.
  bool IsPeekKeySupported() {
    return wl::get_version_of_object(obj_.get()) >=
           ZCR_EXTENDED_KEYBOARD_V1_PEEK_KEY_SINCE_VERSION;
  }

 private:
  static void PeekKey(void* data,
                      zcr_extended_keyboard_v1* obj,
                      uint32_t serial,
                      uint32_t time,
                      uint32_t key,
                      uint32_t state) {
    auto* extended_keyboard = static_cast<ZCRExtendedKeyboard*>(data);
    DCHECK(data);
    extended_keyboard->keyboard_->OnKey(
        serial, time, key, state, WaylandKeyboard::KeyEventKind::kPeekKey);
  }

  const raw_ptr<WaylandKeyboard> keyboard_;
  wl::Object<zcr_extended_keyboard_v1> obj_;
};

// static
const wl_callback_listener WaylandKeyboard::callback_listener_ = {
    WaylandKeyboard::SyncCallback,
};

WaylandKeyboard::WaylandKeyboard(
    wl_keyboard* keyboard,
    zcr_keyboard_extension_v1* keyboard_extension_v1,
    WaylandConnection* connection,
    KeyboardLayoutEngine* layout_engine,
    Delegate* delegate)
    : obj_(keyboard),
      connection_(connection),
      delegate_(delegate),
      auto_repeat_handler_(this),
      layout_engine_(static_cast<LayoutEngine*>(layout_engine)) {
  static constexpr wl_keyboard_listener listener = {
      &Keymap, &Enter, &Leave, &Key, &Modifiers, &RepeatInfo,
  };

  wl_keyboard_add_listener(obj_.get(), &listener, this);
  // TODO(tonikitoo): Default auto-repeat to ON here?

  if (keyboard_extension_v1) {
    extended_keyboard_ = std::make_unique<ZCRExtendedKeyboard>(
        this, zcr_keyboard_extension_v1_get_extended_keyboard(
                  keyboard_extension_v1, obj_.get()));
  }
}

WaylandKeyboard::~WaylandKeyboard() {
  // Reset keyboard modifiers on destruction.
  delegate_->OnKeyboardModifiersChanged(0);
}

void WaylandKeyboard::OnUnhandledKeyEvent(const KeyEvent& key_event) {
  // No way to send ack_key.
  if (!extended_keyboard_)
    return;

  // Obtain the serial from properties. See WaylandEventSource how to annotate.
  const auto* properties = key_event.properties();
  if (!properties)
    return;
  auto it = properties->find(kPropertyWaylandSerial);
  if (it == properties->end())
    return;
  DCHECK_EQ(it->second.size(), 4u);
  uint32_t serial = it->second[0] | (it->second[1] << 8) |
                    (it->second[2] << 16) | (it->second[3] << 24);

  extended_keyboard_->AckKey(serial, false);
}

// Two different behaviors are currently implemented for KeyboardLock support
// on Wayland:
//
// 1. On Lacros, shortcuts are kept inhibited since the window initialization.
// Such approach relies on the Exo-specific zcr-keyboard-extension protocol
// extension, which allows Lacros (ozone/wayland based) to report back to the
// Wayland compositor that a given key was not processed by the client, giving
// it a chance of processing global shortcuts (even with a shortcuts inhibitor
// in place), which is not currently possible with standard Wayland protocol
// and extensions. That is also required to keep Lacros behaving just like Ash
// Chrome's classic browser.
//
// 2. Otherwise, keyboard shortcuts will be inhibited only when in fullscreen
// and when a WaylandKeyboardHook is in place for a given widget. See
// KeyboardLock spec for more details: https://wicg.github.io/keyboard-lock
//
// TODO(https://crbug.com/1338554): Revisit once this scenario changes.
std::unique_ptr<PlatformKeyboardHook> WaylandKeyboard::CreateKeyboardHook(
    WaylandWindow* window,
    absl::optional<base::flat_set<DomCode>> dom_codes,
    PlatformKeyboardHook::KeyEventCallback callback) {
  DCHECK(window);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return std::make_unique<BaseKeyboardHook>(std::move(dom_codes),
                                            std::move(callback));
#else
  return std::make_unique<WaylandKeyboardHook>(
      CreateShortcutsInhibitor(window));
#endif
}

wl::Object<zwp_keyboard_shortcuts_inhibitor_v1>
WaylandKeyboard::CreateShortcutsInhibitor(WaylandWindow* window) {
  DCHECK(window);
  DCHECK(window->root_surface());
  if (auto* manager = connection_->keyboard_shortcuts_inhibit_manager_v1()) {
    return wl::Object<zwp_keyboard_shortcuts_inhibitor_v1>(
        zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts(
            manager, window->root_surface()->surface(),
            connection_->seat()->wl_object()));
  }
  return {};
}

void WaylandKeyboard::Keymap(void* data,
                             wl_keyboard* obj,
                             uint32_t format,
                             int32_t fd,
                             uint32_t size) {
  auto* keyboard = static_cast<WaylandKeyboard*>(data);
  DCHECK(keyboard);

  if (!data || format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
    return;

  // From the Wayland specification: "From version 7 onwards, the fd must be
  // mapped with MAP_PRIVATE by the recipient, as MAP_SHARED may fail."
  int map_flags = wl_keyboard_get_version(obj) >= 7 ? MAP_PRIVATE : MAP_SHARED;
  void* keymap = mmap(nullptr, size, PROT_READ, map_flags, fd, 0);
  if (keymap == MAP_FAILED) {
    DPLOG(ERROR) << "Failed to map XKB keymap.";
    return;
  }

  const char* keymap_string = static_cast<const char*>(keymap);
  if (!keyboard->layout_engine_->SetCurrentLayoutFromBuffer(
          keymap_string, strnlen(keymap_string, size))) {
    DLOG(ERROR) << "Failed to set XKB keymap.";
  }
  munmap(keymap, size);
}

void WaylandKeyboard::Enter(void* data,
                            wl_keyboard* obj,
                            uint32_t serial,
                            wl_surface* surface,
                            wl_array* keys) {
  // wl_surface might have been destroyed by this time.
  if (auto* window = wl::RootWindowFromWlSurface(surface)) {
    auto* self = static_cast<WaylandKeyboard*>(data);
    self->delegate_->OnKeyboardFocusChanged(window, /*focused=*/true);
  }
}

void WaylandKeyboard::Leave(void* data,
                            wl_keyboard* obj,
                            uint32_t serial,
                            wl_surface* surface) {
  // wl_surface might have been destroyed by this time.
  auto* self = static_cast<WaylandKeyboard*>(data);
  if (auto* window = wl::RootWindowFromWlSurface(surface))
    self->delegate_->OnKeyboardFocusChanged(window, /*focused=*/false);

  // Upon window focus lose, reset the key repeat timers.
  self->auto_repeat_handler_.StopKeyRepeat();
}

void WaylandKeyboard::Key(void* data,
                          wl_keyboard* obj,
                          uint32_t serial,
                          uint32_t time,
                          uint32_t key,
                          uint32_t state) {
  auto* keyboard = static_cast<WaylandKeyboard*>(data);
  DCHECK(keyboard);
  keyboard->OnKey(serial, time, key, state, KeyEventKind::kKey);
}

void WaylandKeyboard::Modifiers(void* data,
                                wl_keyboard* obj,
                                uint32_t serial,
                                uint32_t depressed,
                                uint32_t latched,
                                uint32_t locked,
                                uint32_t group) {
#if BUILDFLAG(USE_XKBCOMMON)
  auto* keyboard = static_cast<WaylandKeyboard*>(data);
  DCHECK(keyboard);

  int modifiers = keyboard->layout_engine_->UpdateModifiers(depressed, latched,
                                                            locked, group);
  keyboard->delegate_->OnKeyboardModifiersChanged(modifiers);
#endif
}

void WaylandKeyboard::RepeatInfo(void* data,
                                 wl_keyboard* obj,
                                 int32_t rate,
                                 int32_t delay) {
  // Negative values for either rate or delay are illegal.
  if (rate < 0 || delay < 0) {
    VLOG(1) << "Ignoring wl_keyboard.repeat_info event with illegal "
            << "values (rate=" << rate << " delay=" << delay << ").";
    return;
  }

  DCHECK(data);
  EventAutoRepeatHandler& handler =
      static_cast<WaylandKeyboard*>(data)->auto_repeat_handler_;

  // A rate of zero will disable any repeating.
  handler.SetAutoRepeatEnabled(rate != 0);
  if (handler.IsAutoRepeatEnabled()) {
    // The rate is in characters per second.
    handler.SetAutoRepeatRate(base::Milliseconds(delay),
                              base::Seconds(1.0 / rate));
  }
}

void WaylandKeyboard::FlushInput(base::OnceClosure closure) {
  if (sync_callback_)
    return;

  auto_repeat_closure_ = std::move(closure);

  // wl_display_sync gives a chance for any key "up" events to arrive.
  // With a well behaved wayland compositor this should ensure we never
  // get spurious repeats.
  sync_callback_.reset(wl_display_sync(connection_->display_wrapper()));
  wl_callback_add_listener(sync_callback_.get(), &callback_listener_, this);
  connection_->Flush();
}

void WaylandKeyboard::DispatchKey(unsigned int key,
                                  unsigned int scan_code,
                                  bool down,
                                  bool repeat,
                                  base::TimeTicks timestamp,
                                  int device_id,
                                  int flags) {
  // Key repeat is only triggered by wl_keyboard::key event,
  // but not by extended_keyboard::peek_key.
  DispatchKey(key, scan_code, down, repeat, absl::nullopt, timestamp, device_id,
              flags, KeyEventKind::kKey);
}

void WaylandKeyboard::OnKey(uint32_t serial,
                            uint32_t time,
                            uint32_t key,
                            uint32_t state,
                            KeyEventKind kind) {
  bool down = state == WL_KEYBOARD_KEY_STATE_PRESSED;
  if (down) {
    connection_->serial_tracker().UpdateSerial(wl::SerialType::kKeyPress,
                                               serial);
  }

  if (kind == KeyEventKind::kKey && !IsModifierKey(key)) {
    auto_repeat_handler_.UpdateKeyRepeat(key, 0 /*scan_code*/, down,
                                         /*suppress_auto_repeat=*/false,
                                         device_id());
  }

  // Block to dispatch RELEASE wl_keyboard::key event, if
  // zcr_extended_keyboard::peek_key is supported, since the event is
  // already dispatched.
  // If not supported, dispatch it for compatibility.
  if (kind == KeyEventKind::kKey && !down && extended_keyboard_ &&
      extended_keyboard_->IsPeekKeySupported()) {
    return;
  }

  DispatchKey(key, 0 /*scan_code*/, down, false /*repeat*/,
              absl::make_optional(serial), EventTimeForNow(), device_id(),
              EF_NONE, kind);
}

void WaylandKeyboard::DispatchKey(unsigned int key,
                                  unsigned int scan_code,
                                  bool down,
                                  bool repeat,
                                  absl::optional<uint32_t> serial,
                                  base::TimeTicks timestamp,
                                  int device_id,
                                  int flags,
                                  KeyEventKind kind) {
  DomCode dom_code = KeycodeConverter::EvdevCodeToDomCode(key);
  if (dom_code == ui::DomCode::NONE)
    return;

  // Pass empty DomKey and KeyboardCode here so the delegate can pre-process
  // and decode it when needed.
  uint32_t result = delegate_->OnKeyboardKeyEvent(
      down ? ET_KEY_PRESSED : ET_KEY_RELEASED, dom_code, repeat, serial,
      timestamp, device_id, kind);

  if (extended_keyboard_ && !(result & POST_DISPATCH_STOP_PROPAGATION) &&
      serial.has_value()) {
    // Not handled, so send ack_key event immediately.
    // If handled, we do not, because the event is at least sent to the client,
    // but later on, it may be returned as unhandled. If we send ack_key to the
    // compositor, there's no way to cancel it.
    extended_keyboard_->AckKey(serial.value(), false);
  }
}

void WaylandKeyboard::SyncCallback(void* data,
                                   struct wl_callback* cb,
                                   uint32_t time) {
  auto* keyboard = static_cast<WaylandKeyboard*>(data);
  DCHECK(keyboard);
  DCHECK(keyboard->auto_repeat_closure_);
  std::move(keyboard->auto_repeat_closure_).Run();
  keyboard->sync_callback_.reset();
}

}  // namespace ui
