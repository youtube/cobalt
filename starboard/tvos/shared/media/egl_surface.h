// Copyright 2018 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_TVOS_SHARED_MEDIA_EGL_SURFACE_H_
#define STARBOARD_TVOS_SHARED_MEDIA_EGL_SURFACE_H_

#import <Foundation/Foundation.h>
#include <GLES2/gl2.h>
#import <GLKit/GLKit.h>

/**
 *  @brief Types of EGL surfaces.
 */
typedef NS_ENUM(NSUInteger, SBDEAGLSurfaceType) {
  /**
   *  @brief A window surface is typically displayed to the user.
   */
  SBDEAGLSurfaceTypeWindow,

  /**
   *  @brief An off-screen surface is used for loading textures.
   */
  SBDEAGLSurfaceTypeOffscreenSurface,
};

NS_ASSUME_NONNULL_BEGIN

/**
 *  @brief A EGL surface on the host platform.
 */
@interface SBDEglSurface : NSObject

/**
 *  @brief The framebuffer ID associated with the EGL surface if it's
 *      a display surface. Used to automatically replace "0" in calls
 *      to @c glBindFramebuffer.
 */
@property(nonatomic, readonly) GLint framebufferID;

/**
 *  @brief The type of surface (display or offscreen).
 */
@property(nonatomic, readonly) SBDEAGLSurfaceType type;

/**
 *  @brief The view which should be added to the window's subview for surfaces.
 */
@property(nonatomic, readonly) GLKView* view;

- (instancetype)init NS_UNAVAILABLE;

/**
 *  @brief The designated initializer.
 *  @param type Specifies how the surface will be used.
 *  @param width The desired width (in points) of the surface.
 *  @param height The desired height (in points) of the surface.
 */
- (instancetype)initType:(SBDEAGLSurfaceType)type
                   width:(NSInteger)width
                  height:(NSInteger)height NS_DESIGNATED_INITIALIZER;

/**
 *  @brief Sets the surface as the drawing target of OpenGL commands issued to
 *      the specified context.
 *  @remarks For EAGL and GLKit, this means associating the underlying GLKView's
 *      context with the EAGL context, and binding the drawable.
 */
- (void)bindToContext:(EAGLContext*)context;

/**
 *  Swaps the on-screen and off-screen buffers. (Displays the result
 *      of the draw commands that have been issued to the context with
 *      which this surface is bound).
 */
- (void)swapBuffers;

@end

NS_ASSUME_NONNULL_END

#endif  // STARBOARD_TVOS_SHARED_MEDIA_EGL_SURFACE_H_
