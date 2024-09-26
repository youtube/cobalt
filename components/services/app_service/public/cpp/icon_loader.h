// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_LOADER_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_LOADER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

// An abstract class for something that can load App Service icons, either
// directly or by wrapping another IconLoader.
class IconLoader {
 public:
  // An RAII-style object that, when destroyed, runs |closure|.
  //
  // For example, that |closure| can inform an IconLoader that an icon is no
  // longer actively used by whoever held this Releaser (an object returned by
  // IconLoader::LoadIconFromIconKey). This is merely advisory: the IconLoader
  // is free to ignore the Releaser-was-destroyed hint and to e.g. keep any
  // cache entries alive for a longer or shorter time.
  //
  // These can be chained, so that |this| is the head of a linked list of
  // Releaser's. Destroying the head will destroy the rest of the list.
  //
  // Destruction must happen on the same sequence (in the
  // base/sequence_checker.h sense) as the LoadIcon or LoadIconFromIconKey call
  // that returned |this|.
  class Releaser {
   public:
    Releaser(std::unique_ptr<Releaser> next, base::OnceClosure closure);

    Releaser(const Releaser&) = delete;
    Releaser& operator=(const Releaser&) = delete;

    virtual ~Releaser();

   private:
    std::unique_ptr<Releaser> next_;
    base::OnceClosure closure_;
  };

  IconLoader();
  virtual ~IconLoader();

  // Looks up the IconKey for the given app ID. Return a fake icon key as the
  // default implementation to simplify the sub class implementation in test
  // code.
  virtual absl::optional<IconKey> GetIconKey(const std::string& app_id);

  // This can return nullptr, meaning that the IconLoader does not track when
  // the icon is no longer actively used by the caller. `callback` may be
  // dispatched synchronously if it's possible to quickly return a result.
  virtual std::unique_ptr<Releaser> LoadIconFromIconKey(
      AppType app_type,
      const std::string& app_id,
      const IconKey& icon_key,
      IconType icon_type,
      int32_t size_hint_in_dip,
      bool allow_placeholder_icon,
      apps::LoadIconCallback callback) = 0;

  // Convenience method that calls "LoadIconFromIconKey(app_type, app_id,
  // GetIconKey(app_id), etc)". `callback` may be dispatched synchronously if
  // it's possible to quickly return a result.
  std::unique_ptr<Releaser> LoadIcon(AppType app_type,
                                     const std::string& app_id,
                                     const IconType& icon_type,
                                     int32_t size_hint_in_dip,
                                     bool allow_placeholder_icon,
                                     apps::LoadIconCallback callback);

 protected:
  // A struct containing the arguments (other than the callback) to
  // Loader::LoadIconFromIconKey, including a flattened apps::IconKey.
  //
  // It implements operator<, so that it can be the "K" in a "map<K, V>".
  //
  // Only IconLoader subclasses (i.e. implementations), not IconLoader's
  // callers, are expected to refer to a Key.
  class Key {
   public:
    AppType app_type_;
    std::string app_id_;
    // IconKey fields.
    uint64_t timeline_;
    int32_t resource_id_;
    uint32_t icon_effects_;
    // Other fields.
    IconType icon_type_;
    int32_t size_hint_in_dip_;
    bool allow_placeholder_icon_;

    Key(AppType app_type,
        const std::string& app_id,
        const IconKey& icon_key,
        IconType icon_type,
        int32_t size_hint_in_dip,
        bool allow_placeholder_icon);

    Key(const Key& other);

    bool operator<(const Key& that) const;
  };
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_LOADER_H_
