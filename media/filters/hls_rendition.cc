// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_rendition.h"

#include "media/filters/hls_rendition_impl.h"
#include "media/filters/manifest_demuxer.h"

namespace media {

namespace {

std::optional<base::TimeDelta> GetPlaylistDuration(
    hls::MediaPlaylist* playlist) {
  if (!playlist->HasMediaSequenceTag()) {
    // Live playbacks have a media sequence tag, so if that's missing, then this
    // playback is VOD, and we can use it's computed duration.
    return playlist->GetComputedDuration();
  }

  auto segments = playlist->GetSegments();
  // Usually manifests use an Endlist tag to end a live playback, but its
  // also fairly common to see these on VOD content that was clipped from
  // live content.
  if (playlist->IsEndList()) {
    if (!segments.empty()) {
      return playlist->GetComputedDuration();
    }
  }

  // Live content doesn't have a defined duration.
  return std::nullopt;
}

}  // namespace

// Static
std::unique_ptr<HlsRendition> HlsRendition::CreateRendition(
    ManifestDemuxerEngineHost* engine_host,
    HlsRenditionHost* rendition_host,
    std::string role,
    scoped_refptr<hls::MediaPlaylist> playlist,
    GURL uri,
    MediaLog* media_log) {
  auto duration = GetPlaylistDuration(playlist.get());
  return std::make_unique<HlsRenditionImpl>(
      engine_host, rendition_host, std::move(role), std::move(playlist),
      duration, std::move(uri), media_log);
}

}  // namespace media
