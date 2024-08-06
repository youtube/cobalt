// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_MIDI_MIDI_EXPORT_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_MIDI_MIDI_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(MIDI_IMPLEMENTATION)
#define MIDI_EXPORT __declspec(dllexport)
#else
#define MIDI_EXPORT __declspec(dllimport)
#endif  // defined(MIDI_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(MIDI_IMPLEMENTATION)
#define MIDI_EXPORT __attribute__((visibility("default")))
#else
#define MIDI_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define MIDI_EXPORT
#endif

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_MIDI_MIDI_EXPORT_H_
