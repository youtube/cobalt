# App Service

Chrome, and especially Chrome OS, has apps, e.g. chat apps and camera apps.

There are a number (lets call it `M`) of different places or app Consumers,
usually UI (user interfaces) related, where users interact with their installed
apps: e.g. launcher, search bar, shelf, New Tab Page, the App Management
settings page, permissions or settings pages, picking and running default
handlers for URLs, MIME types or intents, etc.

There is also a different number (`N`) of app platforms or app Publishers:
built-in apps, extension-backed apps, PWAs (progressive web apps), ARC++
(Android apps), Crostini (Linux apps), etc.

Historically, each of the `M` Consumers hard-coded each of the `N` Publishers,
leading to `M×N` code paths that needed maintaining, or updating every time `M`
or `N` was incremented.

This document describes the App Service, an intermediary between app Consumers
and app Publishers. This simplifies `M×N` code paths to `M+N` code paths, each
side with a uniform API, reducing code duplication and improving behavioral
consistency.

The App Service can be decomposed into a number of aspects. In all cases, it
provides to Consumers a uniform API over the various Publisher implementations,
for these aspects:

  - App Registry: list the installed apps.
  - App Icon Factory: load an app's icon, at various resolutions.
  - App Runner: launch apps and track app instances.
  - App Installer: install, uninstall and update apps.
  - App Coordinator: keep system-wide settings, e.g. default handlers.

Some things are still the responsbility of individual Consumers or Publishers.
For example, the order in which the apps' icons are presented in the launcher
is a launcher-specific detail, not a system-wide detail, and is managed by the
launcher, not the App Service. Similarly, Android-specific VM (Virtual Machine)
configuration is Android-specific, not generalizable system-wide, and is
managed by the Android provider (ARC++).


## Profiles

Talk of *the* App Service is an over-simplification. There will be *an* App
Service instance(AppServiceProxy) per Profile, as apps can be installed for
*a* Profile.

# App Registry

The App Registry's one-liner mission is:

  - I would like to be able to for-each over all the apps in Chrome.

The App Registry design involves three actors (Consumers ⇔ AppRegistryCache
⇔ Publisher), with the AppRegistryCache providing a platform-independent
abstraction over the per-platform implementation details.

The AppRegistryCache is owned by the AppServiceProxy as an implementation
detail. Being able to for-each over all the apps is:

    proxy->AppRegistryCache().ForEachApp([..]() {
      DoSomethingWith(app);
    });

The Proxy is expected to be in the same process as its Consumers, and the Proxy
would be a singleton (per Profile) within that process: Consumers would connect
to *the* in-process Proxy. If all of the app UI code is in the browser process,
the Proxy would also be in the browser process. If app UI code migrated to e.g.
a separate Ash process, then the Proxy would move with them. There might be
multiple Proxies, one per process (per Profile).


## Code Location

Some code is tied to a particular process, some code is not. For example, the
per-Profile `AppServiceProxy` obviously contains Profile-related code (i.e. a
`KeyedService`, so that browser code can find *the* `AppServiceProxy` for a
given Profile) that is tied to being in the browser process. The
`AppServiceProxy` also contains process-agnostic code (code that could
conceivably be used by an `AppServiceProxy` living in an Ash process), such as
code to cache and update the set of known apps (as in, the `App` type).
Specifically, the `AppServiceProxy` code is split into two locations, one under
`//chrome/browser` and one not:

  - `//chrome/browser/apps/app_service`
  - `//components/services/app_service`

Code specific to publishers lives under:

  - `//chrome/browser/apps/app_service/publishers`

Code specific to web application publishers lives under:

  - `//chrome/browser/web_applications/app_service`


## The App Type

The one struct type, `App`, represents both a state, "an app that's ready
to run", and a delta or change in state, "here's what's new about an app".
Deltas include events like "an app was just installed" or "just uninstalled" or
"its icon was updated".

This is achieved by having every `App` field (other than `App.app_type` and
`App.app_id`) be optional.

An `App.readiness` field represents whether an app is installed (i.e. ready to
launch), uninstalled or otherwise disabled. "An app was just installed" is
represented by a delta whose `readiness` is `kReady` and the old state's
`readiness` being some other value. The `AppUpdate` wrapper type (see below)
can provide helper `WasJustInstalled` methods.

The `App`, `Readiness` and `OptionalBool` types are:

    struct App {
      AppType app_type;
      string app_id;

      // The fields above are mandatory. Everything else below is optional.

      Readiness readiness;
      absl::optional<std::string> name;
      absl::optional<IconKey> icon_key;
      absl::optional<bool> show_in_launcher;
      // etc.
    };

    enum Readiness {
      kUnknown = 0,
      kReady,                // Installed and launchable.
      kDisabledByBlocklist,  // Disabled by SafeBrowsing.
      kDisabledByPolicy,     // Disabled by admin policy.
      kDisabledByUser,       // Disabled by explicit user action.
      kUninstalledByUser,
    };

    // struct IconKey is discussed in the "App Icon Factory" section.

A new state can be mechanically computed from an old state and a delta (both of
which have the same type: `App`). Specifically, last known value wins. Any
known field in the delta overwrites the corresponding field in the old state,
any unknown field in the delta is ignored. For example, if an app's name
changed but its icon didn't, the delta's `App.name` field (a
`absl::optional<std::string>`) would be known (not `absl::nullopt`) and copied
over but its `App.icon` field would be unknown (`absl::nullopt`) and not copied
over.

The current state is thus the merger or sum of all previous deltas, including
the initial state being a delta against the ground state of "all unknown". The
`AppServiceProxy` tracks the state of its apps, and implements the
(in-process) Observer pattern so that UI surfaces can e.g. update themselves as
new apps are installed. There's only one method, `OnAppUpdate`, as opposed to
separate `OnAppInstalled`, `OnAppUninstalled`, `OnAppNameChanged`, etc.
methods. An `AppUpdate` is a pair of `App` values: old state and delta.

    class AppRegistryCache {
     public:
      class Observer : public base::CheckedObserver {
       public:
        ~Observer() override {}
        virtual void OnAppUpdate(const AppUpdate& update) = 0;
      };

      // Etc.
    };


## Registering Publisher

Once a `Publisher` registers itself to `AppServiceProxy`, the publisher sends
a stream of `App`s (calling the `OnApps` API method). On the initial
registering, the `Publisher` calls `OnApps` with "here's all the apps that I
(the `Publisher`) know about", with additional `OnApps` calls made as apps are
installed, uninstalled, updated, etc.

The key recipient of these calls is the AppRegistryCache, which coalesces the
updates into AppUpdate objects that are the core way that Consumers of the App
Service query the installed apps on the OS and interact with them.


# App Icon Factory

Icon data (even compressed as a PNG) is bulky, relative to the rest of the
`App` type. `Publisher`s will generally serve icon data lazily, on demand,
especially as the desired icon resolutions (e.g. 64dip or 256dip) aren't known
up-front. Instead of sending an icon at all possible resolutions, the
`Publisher` sends an `IconKey`: enough information to load the icon at given
resolutions.

An `IconKey` augments the `AppType app_type` and `string app_id`. For example,
some icons are statically built into the Chrome or Chrome OS binary, as
PNG-formatted resources, and can be loaded (synchronously, without sandboxing).
They can be loaded from the `IconKey.resource_id`. Other icons are dynamically
(and asynchronously) loaded from the extension database on disk. The base icon
can be loaded just from the `app_id` alone.

In either case, the `IconKey.icon_effects` bitmask holds whether to apply
further image processing effects such as desaturation to gray.

    AppServiceProxy {
      // App Icon Factory methods.
      LoadIcon(
          AppType app_type,
          string app_id,
          IconKey icon_key,
          IconType icon_type,
          int32 size_hint_in_dip,
          bool allow_placeholder_icon) => (IconValue icon_value);

      // Some additional methods; not App Icon Factory related.
    };

    Publisher {
      // App Icon Factory methods.
      LoadIcon(
          string app_id,
          IconKey icon_key,
          IconType icon_type,
          int32 size_hint_in_dip,
          bool allow_placeholder_icon) => (IconValue icon_value);

      // Some additional methods; not App Icon Factory related.
    };

    struct IconKey {
      // A monotonically increasing number so that, after an icon update, a new
      // IconKey, one that is different in terms of field-by-field equality, can be
      // broadcast by a Publisher.
      //
      // The exact value of the number isn't important, only that newer IconKey's
      // (those that were created more recently) have a larger timeline than older
      // IconKey's.
      //
      // This is, in some sense, *a* version number, but the field is not called
      // "version", to avoid any possible confusion that it encodes *the* app's
      // version number, e.g. the "2.3.5" in "FooBar version 2.3.5 is installed".
      //
      // For example, if an app is disabled for some reason (so that its icon is
      // grayed out), this would result in a different timeline even though the
      // app's version is unchanged.
      uint64 timeline;
      // If non-zero, the compressed icon is compiled into the Chromium binary
      // as a statically available, int-keyed resource.
      int32 resource_id;
      // A bitmask of icon post-processing effects, such as desaturation to
      // gray and rounding the corners.
      uint32 icon_effects;
    };

    enum IconType {
      kUnknown,
      kUncompressed,
      kCompressed,
      kStandard,
    };

    struct IconValue {
      IconType icon_type;
      gfx::ImageSkia uncompressed;
      std::vector<uint8_t> compressed;
      bool is_placeholder_icon;
    };


## Icon Changes

Apps can change their icons, e.g. after a new version is installed. From the
App Service's point of view, an icon change is like any other change: Publishers
broadcast an `App` value representing what's changed (icon or otherwise) about
an app, the Proxy's `AppRegistryCache` enriches this `App` struct to be an
`AppUpdate`, and `AppRegistryCache` observers can, if that `AppUpdate` shows
that the icon has changed, issue a new `LoadIcon` call. A new call is
necessary, because a callback is a `base::OnceCallback`, so the same
callback can't be used for both the old and the new icon.


## Caching and Other Optimizations

Grouping the `IconKey` with the other `LoadIcon` arguments, the combination
identifies a static (unchanging, but possibly obsolete) image: if a new version
of an app results in a new icon, or if a change in app state results in a
grayed out icon, this is represented by a different, larger `IconKey.timeline`.
As a consequence, the combined `LoadIcon` arguments can be used to key a cache
or map of `IconValue`s, or to recognize and coalesce multiple concurrent
requests to the same combination.

Such optimizations can be implemented as a series of "wrapper" classes (as in
the classic "decorator" or "wrapper" design pattern) that all implement the
same C++ interface (an `IconLoader` interface). They add their specific feature
(e.g. caching) by wrapping another `IconLoader`, doing feature-specific work on
every call or reply before sending the call forward or the reply backward.

There may be multiple caches, as there may be multiple cache eviction policies
(also known as garbage collection policies), spanning the trade-off from
favoring minimizing memory use to favoring maximizing cache hit rates. The
Proxy may have a single cache, with a relatively aggressive eviction policy,
which applies to all of its Consumer clients. A Consumer might have an
additional Consumer-specific cache, with a more relaxed eviction policy, if it
has additional Consumer-specific UI signals to guide when icon-loading requests
and cache hits are more or less likely.

Note that cache values (the `IconValue` struct) are, primarily, a
gfx.ImageSkia, which are cheap to share. Copying an ImageSkia value does
not duplicate any underlying pixel buffers.

As a separate optimization, if the `AppServiceProxy` knows how to load an icon
for a given `IconKey`, it can skip the round trip and bulk data IPC and load
it directly instead. For example, it may know how to load icons from a
statically built resource ID.


## Placeholder Icons

It can take some time for `Publisher`s to provide an icon. For example, loading
the canonical icon for an ARC++ or Crostini app might require waiting for a VM
to start. Such icons are often cached on the file system, but on a cache miss,
there may be a number of seconds before the system can present an icon. In this
case, we might want to present a `Publisher`-specific placeholder, typically
loaded from a resource (an asset statically compiled into the binary).

There are two boolean fields that facilitate this: `allow_placeholder_icon` is
sent from a `Subscriber` to a `Publisher` and `is_placeholder_icon` is sent in
the response.

`LoadIcon`'s `allow_placeholder_icon` states whether the the caller will accept
a placeholder if the real icon can not be provided quickly. Native user
interfaces like the app launcher will probably set this to true. On the other
hand, serving Web-UI URLs such as `chrome://app-icon/app_id/icon_size` will set
this to false, as that URL should identify a particular icon, not one that
changes over time. Web-UI that wants to display placeholder icons and be
notified of when real icons are ready will require some mechanism other than a
`chrome:://app-icon/etc` URL.

`IconValue`'s `is_placeholder_icon` states whether the icon provided is a
placeholder. That field should only be true if the corresponding `LoadIcon`
call had `allow_placeholder_icon` true. When the `LoadIcon` caller receives a
placeholder icon, it is up to the caller to issue a new `LoadIcon` call, this
time with `allow_placeholder_icon` false. As before, a new call is necessary,
because a callback is a `base::OnceCallback`, so the same callback can't be
used for both the placeholder and the real icon.


## Provider-Specific Subtleties

Some concerns (like caching and coalescing multiple in-flight calls with the
same `IconKey`) are not specific to any particular Publishers like ARC++ or
Crostini, and can be solved by the Proxy.

Other concerns are Publisher-specific, and are generally solved in Publisher
implementations, albeit often with non-Publisher-specific support (such as for
placeholder icons, discussed above). Such concerns include:

  - Multiple icon sources: some icons for built-in VM-based apps (e.g. ARC++ or
    Crostini) should be served from a compiled-into-the-browser resource
    instead of from the VM.
  - Pending LoadIcon calls: some `LoadIcon` calls might need to wait on
    bringing up a VM.
  - Potential on-disk corruption: for whatever reason, an on-disk file that's
    meant to hold a cached icon may be missing or invalid. In that case, the
    Publisher should still provide a (placeholder) icon, and trigger
    Publisher-specific clean-up and re-load of the real app icon.

All of these concerns listed should be straightforward to handle, and don't
invalidate the overall App Service `Publisher.LoadIcon` design, including
its non-Publisher-specific caching and other optimization layers.

There are also yet another category of concerns that are Publisher-specific, but
also outside the purview of the App Service. For example, the file system
layout of ARC++'s on-disk icon cache is, from the App Service's point of view,
considered a private ARC++ implementation detail. As long as ARC++'s API
remains the same, and if ARC++ can notify the App Service if the App Service
needs to reload any or all icons, then any change in ARC++'s file system layout
isn't a direct concern to the App Service.


# App Runner

Each `Publisher` has (`Publisher`-specific) implementations of e.g. launching an
app and populating a context menu. The `AppService` presents a uniform API to
trigger these, forwarding each call on to the relevant `Publisher`:

    AppServiceProxy {
      // App Runner methods.
      Launch(AppType app_type, string app_id, LaunchOptions? opts);
      // etc.

      // Some additional methods; not App Runner related.
    };

    Publisher {
      // App Runner methods.
      Launch(string app_id, LaunchOptions? opts);
      // etc.

      // Some additional methods; not App Runner related.
    };

    struct LaunchOptions {
      // TBD.
    };

TBD: details for context menus.

TBD: be able to for-each over all the app *instances*, including multiple
instances (e.g. multiple windows) of the one app.


# App Installer

This includes Publisher-facing API (not Consumer-facing API like the majority of
the `AppService`) to help install and uninstall apps consistently. For example,
one part of app installation is adding an icon shortcut (e.g. on the Desktop
for Windows, on the Shelf for Chrome OS). This helper code should be written
once (in the `AppService`), not `N` times in `N` Publishers.

TBD: details.


# App Coordinator

This keeps system-wide or for-apps-as-a-whole preferences and settings, e.g.
out of all of the installed apps, which app has the user preferred for photo
editing. Consumer- or Publisher-specific settings, e.g. icon order in the Chrome
OS shelf, or Crostini VM configuration, is out of scope of the App Service.

TBD: details.


---

Updated on 2022-09-27.
