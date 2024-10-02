// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_RETICLE_H_
#define CHROME_BROWSER_VR_ELEMENTS_RETICLE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/renderers/base_quad_renderer.h"
#include "ui/gfx/geometry/point3_f.h"

namespace vr {

class UiScene;
struct Model;

class Reticle : public UiElement {
 public:
  Reticle(UiScene* scene, Model* model);

  Reticle(const Reticle&) = delete;
  Reticle& operator=(const Reticle&) = delete;

  ~Reticle() override;

  UiElement* TargetElement() const;

  class Renderer : public BaseQuadRenderer {
   public:
    Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    ~Renderer() override;

    void Draw(float opacity, const gfx::Transform& view_proj_matrix);

    static const char* VertexShader();

   private:
    GLuint model_view_proj_matrix_handle_;
    GLuint color_handle_;
    GLuint ring_diameter_handle_;
    GLuint inner_hole_handle_;
    GLuint inner_ring_end_handle_;
    GLuint inner_ring_thickness_handle_;
    GLuint mid_ring_end_handle_;
    GLuint mid_ring_opacity_handle_;
    GLuint opacity_handle_;
  };

 private:
  void Render(UiElementRenderer* renderer,
              const CameraModel& model) const final;
  gfx::Transform LocalTransform() const final;
  gfx::Transform GetTargetLocalTransform() const final;
  bool ShouldUpdateWorldSpaceTransform(
      bool parent_transform_changed) const final;

  gfx::Point3F origin_;
  gfx::Point3F target_;

  // This is used to convert the target id into a UiElement instance. We are not
  // permitted to retain pointers to UiElements since they may be destructed,
  // but the scene itself is constant, so we will look up our elements on the
  // fly.
  raw_ptr<UiScene, DanglingUntriaged> scene_;

  // Unlike other UiElements which bind their values form the model, the reticle
  // must derive values from the model late in the pipeline after the scene has
  // fully updated its geometry. We therefore retain a pointer to the model and
  // make use of it in |Render|.
  raw_ptr<Model, DanglingUntriaged> model_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_RETICLE_H_
