// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atk/atk.h>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#if defined(USE_GIO)
#include <gio/gio.h>
#endif

#include "base/environment.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "ui/accessibility/platform/atk_util_auralinux.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

namespace {

const char* kAccessibilityEnabledVariables[] = {
    "ACCESSIBILITY_ENABLED",
    "GNOME_ACCESSIBILITY",
    "QT_ACCESSIBILITY",
};

//
// AtkUtilAuraLinux definition and implementation.
//
struct AtkUtilAuraLinux {
  AtkUtil parent;
};

struct AtkUtilAuraLinuxClass {
  AtkUtilClass parent_class;
};

G_DEFINE_TYPE(AtkUtilAuraLinux, atk_util_auralinux, ATK_TYPE_UTIL)

static void atk_util_auralinux_init(AtkUtilAuraLinux *ax_util) {
}

static AtkObject* AtkUtilAuraLinuxGetRoot() {
  ui::AXPlatformNode* application = ui::AXPlatformNodeAuraLinux::application();
  if (application) {
    return application->GetNativeViewAccessible();
  }
  return nullptr;
}

using KeySnoopFuncMap = std::map<guint, std::pair<AtkKeySnoopFunc, gpointer>>;
static KeySnoopFuncMap& GetActiveKeySnoopFunctions() {
  static base::NoDestructor<KeySnoopFuncMap> active_key_snoop_functions;
  return *active_key_snoop_functions;
}

using AXPlatformNodeSet = std::set<ui::AXPlatformNodeAuraLinux*>;
static AXPlatformNodeSet& GetNodesWithPostponedEvents() {
  static base::NoDestructor<AXPlatformNodeSet> nodes_with_postponed_events_list;
  return *nodes_with_postponed_events_list;
}

static void RunPostponedEvents() {
  for (ui::AXPlatformNodeAuraLinux* entry : GetNodesWithPostponedEvents()) {
    entry->RunPostponedEvents();
  }
  GetNodesWithPostponedEvents().clear();
}

static guint AtkUtilAuraLinuxAddKeyEventListener(
    AtkKeySnoopFunc key_snoop_function,
    gpointer data) {
  static guint current_key_event_listener_id = 0;

  // AtkUtilAuraLinuxAddKeyEventListener is called by
  // spi_atk_register_event_listeners in the at-spi2-atk library after it has
  // initialized all its other listeners. Our internal knowledge of the
  // internals of the AT-SPI/ATK bridge allows us to use this call as an
  // indication of AT-SPI being ready, so we can finally run any events that had
  // been waiting.
  ui::AtkUtilAuraLinux::GetInstance()->SetAtSpiReady(true);

  current_key_event_listener_id++;
  GetActiveKeySnoopFunctions()[current_key_event_listener_id] =
      std::make_pair(key_snoop_function, data);
  return current_key_event_listener_id;
}

static void AtkUtilAuraLinuxRemoveKeyEventListener(guint listener_id) {
  GetActiveKeySnoopFunctions().erase(listener_id);
}

static void atk_util_auralinux_class_init(AtkUtilAuraLinuxClass *klass) {
  AtkUtilClass* atk_class = ATK_UTIL_CLASS(g_type_class_peek(ATK_TYPE_UTIL));

  atk_class->get_root = AtkUtilAuraLinuxGetRoot;
  atk_class->get_toolkit_name = []() { return "Chromium"; };
  atk_class->get_toolkit_version = []() { return "1.0"; };
  atk_class->add_key_event_listener = AtkUtilAuraLinuxAddKeyEventListener;
  atk_class->remove_key_event_listener = AtkUtilAuraLinuxRemoveKeyEventListener;
}

}  // namespace

namespace ui {

// static
AtkUtilAuraLinux* AtkUtilAuraLinux::GetInstance() {
  return base::Singleton<AtkUtilAuraLinux>::get();
}

bool AtkUtilAuraLinux::ShouldEnableAccessibility() {
  // Check enabled/disabled accessibility based on env variable
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  for (const auto* variable : kAccessibilityEnabledVariables) {
    std::string enable_accessibility;
    env->GetVar(variable, &enable_accessibility);
    if (enable_accessibility == "1")
      return true;
    if (enable_accessibility == "0") {
      return false;
    }
  }

#if defined(USE_GIO)
  // Do not run additional checks when Chrome runs in headless mode, which means
  // we are in a test environment
  std::string chrome_headless;
  env->GetVar("CHROME_HEADLESS", &chrome_headless);
  if (chrome_headless == "1") {
    return false;
  }

  // Check enabled accessibility based on a11y DBus interface
  GDBusConnection* connection =
      g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);

  if (connection) {
    GVariant* result = g_dbus_connection_call_sync(
        connection, "org.a11y.Bus", "/org/a11y/bus",
        "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", "org.a11y.Status", "IsEnabled"), nullptr,
        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr);
    g_object_unref(connection);

    if (result) {
      GVariant* property;
      g_variant_get(result, "(v)", &property);
      const bool accessibilityEnabled = g_variant_get_boolean(property);
      g_variant_unref(result);
      g_variant_unref(property);
      return accessibilityEnabled;
    }
  }

  // Check enabled accessibility based on GSettings
  GSettingsSchemaSource* source = g_settings_schema_source_get_default();
  GSettingsSchema* gschema = nullptr;

  gschema = g_settings_schema_source_lookup(
      source, "org.gnome.desktop.interface", TRUE);
  if (gschema) {
    GSettings* settings = g_settings_new("org.gnome.desktop.interface");
    const bool accessibilityEnabled =
        g_settings_get_boolean(settings, "toolkit-accessibility");
    g_settings_schema_unref(gschema);
    g_object_unref(settings);
    return accessibilityEnabled;
  }
#endif

  return false;
}

void AtkUtilAuraLinux::InitializeAsync() {
  static bool initialized = false;

  if (initialized || !ShouldEnableAccessibility())
    return;

  initialized = true;

  // Register our util class.
  g_type_class_unref(g_type_class_ref(atk_util_auralinux_get_type()));

  PlatformInitializeAsync();
}

void AtkUtilAuraLinux::InitializeForTesting() {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar(kAccessibilityEnabledVariables[0], "1");

  InitializeAsync();
}

// static
// Disable CFI-icall since the key snooping function could be in another DSO.
__attribute__((no_sanitize("cfi-icall")))
DiscardAtkKeyEvent AtkUtilAuraLinux::HandleAtkKeyEvent(
    AtkKeyEventStruct* key_event) {
  DCHECK(key_event);

  if (!ui::AXPlatformNode::GetAccessibilityMode().has_mode(
          ui::AXMode::kNativeAPIs))
    return DiscardAtkKeyEvent::Retain;

  GetInstance()->InitializeAsync();

  bool discard = false;
  for (auto& entry : GetActiveKeySnoopFunctions()) {
    AtkKeySnoopFunc key_snoop_function = entry.second.first;
    gpointer data = entry.second.second;

    // We want to ensure that all functions are called. We will discard this
    // event if at least one function suggests that we do it, but we still
    // need to call the functions that follow it in the map iterator.
    if (key_snoop_function(key_event, data) != 0)
      discard = true;
  }
  return discard ? DiscardAtkKeyEvent::Discard : DiscardAtkKeyEvent::Retain;
}

bool AtkUtilAuraLinux::IsAtSpiReady() {
  return at_spi_ready_;
}

void AtkUtilAuraLinux::SetAtSpiReady(bool ready) {
  at_spi_ready_ = ready;
  if (ready)
    RunPostponedEvents();
}

void AtkUtilAuraLinux::PostponeEventsFor(AXPlatformNodeAuraLinux* node) {
  GetNodesWithPostponedEvents().insert(node);
}

void AtkUtilAuraLinux::CancelPostponedEventsFor(AXPlatformNodeAuraLinux* node) {
  GetNodesWithPostponedEvents().erase(node);
}

}  // namespace ui
