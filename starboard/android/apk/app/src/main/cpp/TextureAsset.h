// Copyright 2024 The Cobalt Authors. All Rights Reserved.

#ifndef ANDROIDGLINVESTIGATIONS_TEXTUREASSET_H  // NOLINT(build/header_guard)
#define ANDROIDGLINVESTIGATIONS_TEXTUREASSET_H

#include <GLES3/gl3.h>
#include <android/asset_manager.h>
#include <memory>
#include <string>
#include <vector>

class TextureAsset {
 public:
  /*!
   * Loads a texture asset from the assets/ directory
   * @param assetManager Asset manager to use
   * @param assetPath The path to the asset
   * @return a shared pointer to a texture asset, resources will be reclaimed
   * when it's cleaned up
   */
  static std::shared_ptr<TextureAsset> loadAsset(AAssetManager* assetManager,
                                                 const std::string& assetPath);

  ~TextureAsset();

  /*!
   * @return the texture id for use with OpenGL
   */
  constexpr GLuint getTextureID() const { return textureID_; }

 private:
  explicit inline TextureAsset(GLuint textureId) : textureID_(textureId) {}

  GLuint textureID_;
};

#endif  // ANDROIDGLINVESTIGATIONS_TEXTUREASSET_H
