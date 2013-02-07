// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUILD_LINUX_GSETTINGS_H_
#define BUILD_LINUX_GSETTINGS_H_

#include <gio/gio.h>

// The GSettings API was not part of GIO until GIO version 2.26,
// while Ubuntu 10.04 Lucid ships with version 2.24.
//
// To allow compiling on Lucid those forward declarations are provided.
//
// If compiling with GIO version 2.26, these won't conflict,
// because they're identical to the types defined.
//
// TODO(phajdan.jr): This will no longer be needed after switch to Precise,
// see http://crbug.com/158577 .
struct _GSettings;
typedef struct _GSettings GSettings;
GSettings* g_settings_new(const gchar* schema);
GSettings* g_settings_get_child(GSettings* settings, const gchar* name);
gboolean g_settings_get_boolean(GSettings* settings, const gchar* key);
gchar* g_settings_get_string(GSettings* settings, const gchar* key);
gint g_settings_get_int(GSettings* settings, const gchar* key);
gchar** g_settings_get_strv(GSettings* settings, const gchar* key);
const gchar* const* g_settings_list_schemas();

#endif  // BUILD_LINUX_GSETTINGS_H_
