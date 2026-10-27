// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/service/video_geometry_setter_service.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/numerics/checked_math.h"
#include "base/task/sequenced_task_runner.h"

#define MAKE_SURE_ON_SEQUENCE(callback, ...)                                   \
  if (!task_runner_->RunsTasksInCurrentSequence()) {                           \
    task_runner_->PostTask(                                                    \
        FROM_HERE, base::BindOnce(&VideoGeometrySetterService::callback,       \
                                  weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                    \
  }

namespace cobalt {
namespace media {

VideoGeometrySetterService::VideoGeometrySetterService()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      weak_factory_(this) {}

VideoGeometrySetterService::~VideoGeometrySetterService() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

void VideoGeometrySetterService::GetVideoGeometryChangeSubscriber(
    mojo::PendingReceiver<mojom::VideoGeometryChangeSubscriber>
        pending_receiver) {
  MAKE_SURE_ON_SEQUENCE(GetVideoGeometryChangeSubscriber,
                        std::move(pending_receiver));
  video_geometry_change_subscriber_receivers_.Add(this,
                                                  std::move(pending_receiver));
}

base::RepeatingCallback<
    void(mojo::PendingReceiver<mojom::VideoGeometryChangeSubscriber>)>
VideoGeometrySetterService::GetBindSubscriberCallback() {
  return base::BindRepeating(
      &VideoGeometrySetterService::GetVideoGeometryChangeSubscriber,
      weak_factory_.GetWeakPtr());
}

void VideoGeometrySetterService::GetVideoGeometrySetter(
    mojo::PendingReceiver<mojom::VideoGeometrySetter> pending_receiver) {
  MAKE_SURE_ON_SEQUENCE(GetVideoGeometrySetter, std::move(pending_receiver));
  if (video_geometry_setter_receiver_.is_bound()) {
    LOG(ERROR) << __func__ << " VideoGeometrySetter dropped";
    video_geometry_setter_receiver_.reset();
  }
  video_geometry_setter_receiver_.Bind(std::move(pending_receiver));
}

void VideoGeometrySetterService::SubscribeToVideoGeometryChange(
    const base::UnguessableToken& overlay_plane_id,
    mojo::PendingRemote<mojom::VideoGeometryChangeClient> client_pending_remote,
    SubscribeToVideoGeometryChangeCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto client = mojo::Remote<mojom::VideoGeometryChangeClient>(
      std::move(client_pending_remote));
  // The remote end closes the message pipe for the client when it no longer
  // wants to receive updates.
  // If the disconnect_handler is called, |this| must be alive, so Unretained is
  // safe.
  client.set_disconnect_handler(base::BindOnce(
      &VideoGeometrySetterService::OnVideoGeometryChangeClientGone,
      base::Unretained(this), overlay_plane_id));
  video_geometry_change_clients_[overlay_plane_id] = std::move(client);
  sent_geometries_.erase(overlay_plane_id);

  // The video hole may have been reported before the client subscribed.
  MaybeNotifyClient(overlay_plane_id);

  std::move(callback).Run();
}

void VideoGeometrySetterService::SetVideoGeometries(
    std::vector<mojom::VideoHoleGeometryPtr> front_to_back) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  std::vector<base::UnguessableToken> overlay_plane_ids;
  overlay_plane_ids.reserve(front_to_back.size());
  for (const auto& video_hole : front_to_back) {
    overlay_plane_ids.push_back(video_hole->overlay_plane_id);
  }

  if (overlay_plane_ids != last_front_to_back_) {
    // Start above every z-index handed out so far, so that videos whose hole is
    // missing from this frame stay below the ones that are present.
    base::CheckedNumeric<int32_t> next_z_index = next_z_index_;
    next_z_index += overlay_plane_ids.size();
    next_z_index_ = next_z_index.ValueOrDie();
    last_front_to_back_ = std::move(overlay_plane_ids);
  }

  // The front-most video hole gets the largest z-index, i.e., the last one
  // handed out for the current order.
  int32_t z_index = next_z_index_ - 1;
  for (const auto& video_hole : front_to_back) {
    latest_geometries_[video_hole->overlay_plane_id] = {
        video_hole->rect_f, video_hole->transform, z_index--};
    MaybeNotifyClient(video_hole->overlay_plane_id);
  }

  // Forget the holes that are gone and have no client to deliver them to.
  base::EraseIf(latest_geometries_, [this](const auto& entry) {
    return !video_geometry_change_clients_.contains(entry.first) &&
           !base::Contains(last_front_to_back_, entry.first);
  });
}

void VideoGeometrySetterService::MaybeNotifyClient(
    const base::UnguessableToken& overlay_plane_id) {
  auto client = video_geometry_change_clients_.find(overlay_plane_id);
  auto latest = latest_geometries_.find(overlay_plane_id);
  if (client == video_geometry_change_clients_.end() ||
      latest == latest_geometries_.end()) {
    return;
  }

  auto sent = sent_geometries_.find(overlay_plane_id);
  if (sent != sent_geometries_.end() && sent->second == latest->second) {
    return;
  }

  const Geometry& geometry = latest->second;
  sent_geometries_[overlay_plane_id] = geometry;
  client->second->OnVideoGeometryChange(geometry.rect_f, geometry.transform,
                                        geometry.z_index);
}

// When a VideoGeometryChangeClient is gone, delete the corresponding entry in
// the mapping.
void VideoGeometrySetterService::OnVideoGeometryChangeClientGone(
    const base::UnguessableToken overlay_plane_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  video_geometry_change_clients_.erase(overlay_plane_id);
  sent_geometries_.erase(overlay_plane_id);
  latest_geometries_.erase(overlay_plane_id);
}

}  // namespace media
}  // namespace cobalt
