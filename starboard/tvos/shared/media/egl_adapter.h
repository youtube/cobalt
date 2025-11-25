// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_TVOS_SHARED_MEDIA_EGL_ADAPTER_H_
#define STARBOARD_TVOS_SHARED_MEDIA_EGL_ADAPTER_H_

#include <EGL/egl.h>
#import <Foundation/Foundation.h>

#import "starboard/tvos/shared/media/egl_surface.h"

@class EAGLContext;

NS_ASSUME_NONNULL_BEGIN

/**
 *  @brief Provides the concrete implementation of the functionality needed of
 *      EGL.
 */
@interface SBDEglAdapter : NSObject

/**
 *  @brief The status of the last called EGL function in the current thread.
 *  @remarks Retrieving the error subsequently sets the error to EGL_SUCCESS.
 *      This is thread-local.
 */
@property(nonatomic) GLint threadLocalError;

/**
 *  @brief Returns the framebuffer ID associated with the most recently "made
 *      current" display surface on the calling thread.
 */
@property(nonatomic, readonly) GLuint currentDisplayFramebufferID;

/**
 *  @brief Creates a new EGL context.
 */
- (EAGLContext*)contextWithSharedContext:(nullable EAGLContext*)sharedContext;

/**
 *  @brief Destroy the specified EGL context.
 */
- (void)destroyContext:(EAGLContext*)context;

/**
 *  @brief Sets the specified context as the calling thread's current context
 *      and binds it to the specified surface.
 *  @param context The context to use as the new current context.
 *  @param surface The surface which should be the target of GL draw commands to
 *      the context.
 */
- (void)setCurrentContext:(nullable EAGLContext*)context
         andBindToSurface:(nullable SBDEglSurface*)surface;

/**
 *  @brief Return the EGLContext associated with the given EAGLContext.
 */
- (EGLContext)starboardContextForApplicationContext:(EAGLContext*)context;

/**
 *  @brief Return the EAGLContext associated with the given EGLContext.
 */
- (EAGLContext*)applicationContextForStarboardContext:(EGLContext)context;

/**
 *  @brief Creates a new surface.
 *  @param type The type of surface to create.
 *  @param width The width (in pixels) of the surface to create.
 *  @param height The height (in pixels) of the surface to create.
 */
- (SBDEglSurface*)surfaceOfType:(SBDEAGLSurfaceType)type
                          width:(NSInteger)width
                         height:(NSInteger)height;

/**
 *  @brief Destroys a surface.
 *  @param surface The surface to destroy.
 */
- (void)destroySurface:(SBDEglSurface*)surface;

/**
 *  @brief Returns the @c SBDEglSurface associated with an
 *      @c EGLSurface instance.
 *  @param surface The surface whose platform reference should be returned.
 */
- (SBDEglSurface*)applicationSurfaceForStarboardSurface:(EGLSurface)surface;

/**
 *  @brief Returns the @c EGLSurface associated with a @c SBDEglSurface
 *      instance.
 *  @param surface The surface whose Starboard reference should be returned.
 */
- (EGLSurface)starboardSurfaceForApplicationSurface:(SBDEglSurface*)surface;

/**
 *  @brief The designated initializer.
 */
- (instancetype)init;

@end

NS_ASSUME_NONNULL_END

#endif  // STARBOARD_TVOS_SHARED_MEDIA_EGL_ADAPTER_H_
