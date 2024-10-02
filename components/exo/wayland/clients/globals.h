// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_CLIENTS_GLOBALS_H_
#define COMPONENTS_EXO_WAYLAND_CLIENTS_GLOBALS_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "components/exo/wayland/clients/client_helper.h"

namespace exo::wayland::clients {

struct Globals {
  template <typename T>
  class Object : public std::unique_ptr<T> {
   public:
    Object() = default;
    explicit Object(T* ptr) : std::unique_ptr<T>(ptr) {}
    explicit Object(std::unique_ptr<T>&& from)
        : std::unique_ptr<T>(std::move(from)) {}
    Object& operator=(std::unique_ptr<T>&& from) {
      std::unique_ptr<T>::operator=(std::move(from));
      name_ = 0;
      return *this;
    }
    ~Object() = default;

    // Numeric name of the global object.
    uint32_t name() const { return name_; }
    void set_name(uint32_t name) { name_ = name; }

   private:
    uint32_t name_ = 0;
  };

  Globals();
  ~Globals();

  // Initializes the members. `versions` optionally specify the maximum
  // versions to use for corresponding interfaces.
  void Init(wl_display* display,
            base::flat_map<std::string, uint32_t> in_requested_versions);

  std::unique_ptr<wl_registry> registry;

  // TODO(aluh): Support multiple outputs, and probably other globals like
  // aura_output, seat, etc.
  Object<wl_output> output;
  Object<wl_compositor> compositor;
  Object<wl_shm> shm;
  Object<wp_presentation> presentation;
  Object<zwp_linux_dmabuf_v1> linux_dmabuf;
  Object<wl_shell> shell;
  Object<wl_seat> seat;
  Object<wl_subcompositor> subcompositor;
  Object<wl_touch> touch;
  Object<zaura_shell> aura_shell;
  Object<zaura_output> aura_output;
  Object<zxdg_shell_v6> xdg_shell_v6;
  Object<xdg_wm_base> xdg_wm_base;
  Object<zwp_fullscreen_shell_v1> fullscreen_shell;
  Object<zwp_input_timestamps_manager_v1> input_timestamps_manager;
  Object<zwp_linux_explicit_synchronization_v1> linux_explicit_synchronization;
  Object<zcr_vsync_feedback_v1> vsync_feedback;
  Object<zcr_color_manager_v1> color_manager;
  Object<zcr_stylus_v2> stylus;
  Object<zcr_remote_shell_v1> cr_remote_shell_v1;
  Object<zcr_remote_shell_v2> cr_remote_shell_v2;
  Object<surface_augmenter> surface_augmenter;

  base::flat_map<std::string, uint32_t> requested_versions;
};

}  // namespace exo::wayland::clients

#endif  // COMPONENTS_EXO_WAYLAND_CLIENTS_GLOBALS_H_
