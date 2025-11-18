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

#import "starboard/tvos/shared/media/egl_surface.h"

#include <GLES2/gl2.h>

#import "starboard/tvos/shared/defines.h"

@implementation SBDEglSurface

@synthesize framebufferID = _framebufferID;
@synthesize type = _type;
@synthesize view = _view;

- (instancetype)initType:(SBDEAGLSurfaceType)type
                   width:(NSInteger)width
                  height:(NSInteger)height {
  CGRect frame = CGRectMake(0, 0, MAX(width, 1), MAX(height, 1));
  self = [super init];
  if (self) {
    _type = type;
    _view = [[GLKView alloc] initWithFrame:frame];
    _view.drawableColorFormat = GLKViewDrawableColorFormatRGBA8888;
    _view.drawableStencilFormat = GLKViewDrawableStencilFormatNone;
    _view.drawableDepthFormat = GLKViewDrawableDepthFormatNone;
    _view.drawableMultisample = GLKViewDrawableMultisampleNone;
    _view.opaque = NO;
    _view.enableSetNeedsDisplay = NO;
  }
  return self;
}

- (NSString*)description {
  return [NSString stringWithFormat:@"EGL Surface %p", self];
}

- (void)bindToContext:(EAGLContext*)context {
  if (![_view.context isEqual:context]) {
    // It's safe to make these calls outside of the main thread.
    _view.context = context;
    [_view bindDrawable];

    // Get the framebuffer ID.
    GLint constructedFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &constructedFBO);
    _framebufferID = constructedFBO;
  }
}

- (void)swapBuffers {
  [_view display];
}

@end
