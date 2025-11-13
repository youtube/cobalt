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

#import "starboard/tvos/shared/media/egl_adapter.h"

#import <OpenGLES/EAGL.h>

#import "starboard/tvos/shared/defines.h"
#import "starboard/tvos/shared/starboard_application.h"
#import "starboard/tvos/shared/window_manager.h"

/**
 *  @brief The key for a thread-local error.
 */
static NSString* const kThreadLocalErrorKey = @"SBDEGLAdapter_Error";
static NSString* const kThreadLocalFramebufferKey =
    @"SBDEGLAdapter_Framebuffer";

@implementation SBDEglAdapter {
  /**
   *  @brief Strong references to all contexts on the platform.
   */
  NSMutableSet<EAGLContext*>* _eglContexts;

  /**
   *  @brief Strong references to all surfaces on the platform.
   */
  NSMutableSet<SBDEglSurface*>* _eglSurfaces;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _eglContexts = [NSMutableSet set];
    _eglSurfaces = [NSMutableSet set];
  }
  return self;
}

- (EAGLContext*)contextWithSharedContext:(EAGLContext*)sharedContext {
  EAGLSharegroup* sharegroup;
  if (sharedContext) {
    sharegroup = sharedContext.sharegroup;
  }
  EAGLContext* context =
      [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3
                            sharegroup:sharegroup];
  context.multiThreaded = YES;
  @synchronized(_eglContexts) {
    [_eglContexts addObject:context];
  }
  return context;
}

- (void)destroyContext:(EAGLContext*)context {
  @synchronized(_eglContexts) {
    [_eglContexts removeObject:context];
  }
  if ([[EAGLContext currentContext] isEqual:context]) {
    [EAGLContext setCurrentContext:nil];
  }
}

- (void)setCurrentContext:(EAGLContext*)context
         andBindToSurface:(SBDEglSurface*)surface {
  [EAGLContext setCurrentContext:context];
  if (surface) {
    [surface bindToContext:context];
    if (surface.type == SBDEAGLSurfaceTypeWindow) {
      NSMutableDictionary* threadLocalData =
          [NSThread currentThread].threadDictionary;
      threadLocalData[kThreadLocalFramebufferKey] = @(surface.framebufferID);
    }
  }
}

- (GLuint)currentDisplayFramebufferID {
  NSMutableDictionary* threadLocalData =
      [NSThread currentThread].threadDictionary;
  NSNumber* framebufferAsNumber = threadLocalData[kThreadLocalFramebufferKey];
  return framebufferAsNumber.unsignedIntValue;
}

- (EGLContext)starboardContextForApplicationContext:(EAGLContext*)context {
  return (__bridge EGLContext)context;
}

- (EAGLContext*)applicationContextForStarboardContext:(EGLContext)context {
  return (__bridge EAGLContext*)context;
}

- (SBDEglSurface*)surfaceOfType:(SBDEAGLSurfaceType)type
                          width:(NSInteger)width
                         height:(NSInteger)height {
  // Convert pixels to points.
  SBDWindowManager* windowManager = SBDGetApplication().windowManager;
  width /= windowManager.screenScale;
  height /= windowManager.screenScale;
  SBDEglSurface* surface = [[SBDEglSurface alloc] initType:type
                                                     width:width
                                                    height:height];
  if (type == SBDEAGLSurfaceTypeWindow) {
    [windowManager.currentApplicationWindow attachSurface:surface];
  }
  @synchronized(_eglSurfaces) {
    [_eglSurfaces addObject:surface];
  }
  return surface;
}

- (void)destroySurface:(SBDEglSurface*)surface {
  @synchronized(_eglSurfaces) {
    [_eglSurfaces removeObject:surface];
  }
  if (surface.type == SBDEAGLSurfaceTypeWindow) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [surface.view removeFromSuperview];
    });
  }
}

- (EGLSurface)starboardSurfaceForApplicationSurface:(SBDEglSurface*)surface {
  return (__bridge EGLSurface)surface;
}

- (SBDEglSurface*)applicationSurfaceForStarboardSurface:(EGLSurface)surface {
  return (__bridge SBDEglSurface*)surface;
}

- (EGLint)threadLocalError {
  EGLint error;
  NSMutableDictionary* threadLocalData =
      [NSThread currentThread].threadDictionary;
  NSNumber* errorAsNumber = threadLocalData[kThreadLocalErrorKey];
  if (errorAsNumber == nil) {
    error = EGL_SUCCESS;
  } else {
    error = errorAsNumber.shortValue;
  }
  [self setThreadLocalError:EGL_SUCCESS];
  return error;
}

- (void)setThreadLocalError:(EGLint)error {
  NSMutableDictionary* threadLocalData =
      [NSThread currentThread].threadDictionary;
  threadLocalData[kThreadLocalErrorKey] = @(error);
}

@end
