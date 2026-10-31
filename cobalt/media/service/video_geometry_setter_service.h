// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_SERVICE_VIDEO_GEOMETRY_SETTER_SERVICE_H_
#define COBALT_MEDIA_SERVICE_VIDEO_GEOMETRY_SETTER_SERVICE_H_

#include <cstdint>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "cobalt/media/service/mojom/video_geometry_setter.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/overlay_transform.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace cobalt {
namespace media {

// This service runs and destructs on the sequence where it's constructed, but
// the public methods can be run on any sequence.
//
// The z-index sent to each VideoGeometryChangeClient comes from the order of
// the video holes reported by SetVideoGeometries(): the front-most hole gets
// the largest z-index. The z-indices only change when that order changes, and
// each new order gets values larger than any handed out before (values are
// never reused). A video whose hole is missing from the latest frame, or whose
// player is still being torn down, keeps its older, smaller z-index, so it
// stays below every video that is present.
class VideoGeometrySetterService final
    : public mojom::VideoGeometryChangeSubscriber,
      public mojom::VideoGeometrySetter {
 public:
  VideoGeometrySetterService();

  VideoGeometrySetterService(const VideoGeometrySetterService&) = delete;
  VideoGeometrySetterService& operator=(const VideoGeometrySetterService&) =
      delete;

  ~VideoGeometrySetterService() override;

  void GetVideoGeometryChangeSubscriber(
      mojo::PendingReceiver<mojom::VideoGeometryChangeSubscriber>
          pending_receiver);
  void GetVideoGeometrySetter(
      mojo::PendingReceiver<mojom::VideoGeometrySetter> pending_receiver);
  base::RepeatingCallback<
      void(mojo::PendingReceiver<mojom::VideoGeometryChangeSubscriber>)>
  GetBindSubscriberCallback();

 private:
  // What a VideoGeometryChangeClient is told about its video hole: its geometry
  // and its z-index (stacking order).
  struct Geometry {
    gfx::RectF rect_f;
    gfx::OverlayTransform transform = gfx::OVERLAY_TRANSFORM_NONE;
    int32_t z_index = 0;

    friend bool operator==(const Geometry&, const Geometry&) = default;
  };

  // mojom::VideoGeometryChangeSubscriber implementation.
  void SubscribeToVideoGeometryChange(
      const base::UnguessableToken& overlay_plane_id,
      mojo::PendingRemote<mojom::VideoGeometryChangeClient>
          client_pending_remote,
      SubscribeToVideoGeometryChangeCallback callback) override;
  // mojom::VideoGeometrySetter implementation.
  void SetVideoGeometries(
      std::vector<mojom::VideoHoleGeometryPtr> front_to_back) override;

  // Sends the latest geometry of |overlay_plane_id| to its client, unless the
  // client is not subscribed yet, there is no geometry for it, or the client
  // already has it.
  void MaybeNotifyClient(const base::UnguessableToken& overlay_plane_id);

  void OnVideoGeometryChangeClientGone(
      const base::UnguessableToken overlay_plane_id);

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::flat_map<base::UnguessableToken,
                 mojo::Remote<mojom::VideoGeometryChangeClient>>
      video_geometry_change_clients_;

  // The latest geometry of each video hole. It also holds the geometry of
  // holes whose client hasn't subscribed yet, so that a client subscribing
  // after its hole was reported still gets its geometry.
  base::flat_map<base::UnguessableToken, Geometry> latest_geometries_;

  // The geometry last sent to each subscribed client.
  base::flat_map<base::UnguessableToken, Geometry> sent_geometries_;

  // The overlay_plane_ids of the latest SetVideoGeometries() call, front-most
  // first. The z-indices are reassigned whenever this changes.
  std::vector<base::UnguessableToken> last_front_to_back_;
  // The smallest z-index that has not been handed out yet. The video holes in
  // |last_front_to_back_| have the z-indices right below it. It only grows when
  // the order of the video holes changes.
  int32_t next_z_index_ = 0;

  mojo::ReceiverSet<mojom::VideoGeometryChangeSubscriber>
      video_geometry_change_subscriber_receivers_;
  mojo::Receiver<mojom::VideoGeometrySetter> video_geometry_setter_receiver_{
      this};

  // NOTE: Do not add member variables after weak_factory_
  // It should be the first one destroyed among all members.
  // See base/memory/weak_ptr.h.
  base::WeakPtrFactory<VideoGeometrySetterService> weak_factory_{this};
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_SERVICE_VIDEO_GEOMETRY_SETTER_SERVICE_H_
