// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_SERVICE_VIDEO_GEOMETRY_SETTER_SERVICE_H_
#define COBALT_MEDIA_SERVICE_VIDEO_GEOMETRY_SETTER_SERVICE_H_

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

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace cobalt {
namespace media {

// This service runs and destructs on the sequence where it's constructed, but
// the public methods can be run on any sequence.
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
  // mojom::VideoGeometryChangeSubscriber implementation.
  void SubscribeToVideoGeometryChange(
      const base::UnguessableToken& overlay_plane_id,
      mojo::PendingRemote<mojom::VideoGeometryChangeClient>
          client_pending_remote,
      SubscribeToVideoGeometryChangeCallback callback) override;
  // mojom::VideoGeometrySetter implementation.
  void SetVideoGeometry(
      const gfx::RectF& rect_f,
      gfx::OverlayTransform transform,
      const base::UnguessableToken& overlay_plane_id) override;

  void OnVideoGeometryChangeClientGone(
      const base::UnguessableToken overlay_plane_id);

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::flat_map<base::UnguessableToken,
                 mojo::Remote<mojom::VideoGeometryChangeClient>>
      video_geometry_change_clients_;

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
