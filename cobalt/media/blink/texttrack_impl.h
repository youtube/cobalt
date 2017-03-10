// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BLINK_TEXTTRACK_IMPL_H_
#define COBALT_MEDIA_BLINK_TEXTTRACK_IMPL_H_

#include <memory>
#include <string>

#include "base/basictypes.h"
#include "cobalt/media/base/text_track.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebInbandTextTrackClient;
class WebMediaPlayerClient;
}

namespace cobalt {
namespace media {

class WebInbandTextTrackImpl;

class TextTrackImpl : public TextTrack {
 public:
  // Constructor assumes ownership of the |text_track| object.
  TextTrackImpl(const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
                blink::WebMediaPlayerClient* client,
                std::unique_ptr<WebInbandTextTrackImpl> text_track);

  ~TextTrackImpl() OVERRIDE;

  void addWebVTTCue(const base::TimeDelta& start, const base::TimeDelta& end,
                    const std::string& id, const std::string& content,
                    const std::string& settings) OVERRIDE;

 private:
  static void OnAddCue(WebInbandTextTrackImpl* text_track,
                       const base::TimeDelta& start, const base::TimeDelta& end,
                       const std::string& id, const std::string& content,
                       const std::string& settings);

  static void OnRemoveTrack(blink::WebMediaPlayerClient* client,
                            std::unique_ptr<WebInbandTextTrackImpl> text_track);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  blink::WebMediaPlayerClient* client_;
  std::unique_ptr<WebInbandTextTrackImpl> text_track_;
  DISALLOW_COPY_AND_ASSIGN(TextTrackImpl);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BLINK_TEXTTRACK_IMPL_H_
