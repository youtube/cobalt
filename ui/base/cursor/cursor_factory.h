// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_FACTORY_H_
#define UI_BASE_CURSOR_CURSOR_FACTORY_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"

class SkBitmap;

template <class T>
class scoped_refptr;

namespace base {
class TimeDelta;
}

namespace gfx {
class Point;
}

namespace ui {
class PlatformCursor;
struct CursorData;

class COMPONENT_EXPORT(UI_BASE_CURSOR) CursorFactoryObserver {
 public:
  // Called by the factory after it has loaded the cursor theme.
  virtual void OnThemeLoaded() = 0;

  virtual ~CursorFactoryObserver();
};

class COMPONENT_EXPORT(UI_BASE_CURSOR) CursorFactory {
 public:
  CursorFactory();
  virtual ~CursorFactory();

  // Returns the thread-local instance.
  static CursorFactory* GetInstance();

  void AddObserver(CursorFactoryObserver* observer);
  void RemoveObserver(CursorFactoryObserver* observer);
  void NotifyObserversOnThemeLoaded();

  // Return the default cursor of the specified type. When a default cursor is
  // not available, nullptr is returned.
  virtual scoped_refptr<PlatformCursor> GetDefaultCursor(
      mojom::CursorType type);

  // Return the {bitmaps, hotspot} for the default cursor of the specified
  // `type`. If that cursor is not available or the extraction of the data
  // fails, return `absl::nullopt`.
  virtual absl::optional<CursorData> GetCursorData(mojom::CursorType type);

  // Return an image cursor for the specified `type` with a `bitmap` and
  // `hotspot`.
  virtual scoped_refptr<PlatformCursor> CreateImageCursor(
      mojom::CursorType type,
      const SkBitmap& bitmap,
      const gfx::Point& hotspot);

  // Return a animated cursor from the specified `bitmaps` and `hotspot`.
  // `frame_delay` is the delay between frames.
  virtual scoped_refptr<PlatformCursor> CreateAnimatedCursor(
      mojom::CursorType type,
      const std::vector<SkBitmap>& bitmaps,
      const gfx::Point& hotspot,
      base::TimeDelta frame_delay);

  // Called after CursorThemeManager is initialized, to be able to track
  // cursor theme and size changes.
  virtual void ObserveThemeChanges();

  // Sets the device scale factor that CursorFactory may use when creating
  // cursors.
  virtual void SetDeviceScaleFactor(float scale);

 private:
  base::ObserverList<CursorFactoryObserver>::Unchecked observers_;
};

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(UI_BASE_CURSOR)
std::vector<std::string> CursorNamesFromType(mojom::CursorType type);
#endif

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_FACTORY_H_
