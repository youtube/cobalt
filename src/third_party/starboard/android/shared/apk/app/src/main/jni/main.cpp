/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

//BEGIN_INCLUDE(all)
#include <initializer_list>
#include <jni.h>
#include <errno.h>
#include <cassert>
#include <unistd.h>
#include <fcntl.h>

#include <dlfcn.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>

#include <android/native_window_jni.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__))

void *libcobalt;
const char *external_files_dir;
const char *external_cache_dir;
static int msg_read;
static int msg_write;
const static int MESSAGE_FD_ID = 1025;

static pthread_once_t once = PTHREAD_ONCE_INIT;
ANativeWindow *main_window;
ANativeWindow *video_window;

/**
 * Shared state for our app.
 */
struct engine {
    struct android_app* app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width;
    int32_t height;
};

/**
 * Tear down the EGL context currently associated with the display.
 */
static void engine_term_display(struct engine* engine) {
    if (engine->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(engine->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (engine->context != EGL_NO_CONTEXT) {
            eglDestroyContext(engine->display, engine->context);
        }
        if (engine->surface != EGL_NO_SURFACE) {
            eglDestroySurface(engine->display, engine->surface);
        }
        eglTerminate(engine->display);
    }
    engine->display = EGL_NO_DISPLAY;
    engine->context = EGL_NO_CONTEXT;
    engine->surface = EGL_NO_SURFACE;
}

void launch_starboard();

/**
 * Process the next input event.
 */
static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY) {
        int (*starboard_queue_input_event) (AInputEvent*) = (int(*)(AInputEvent*))dlsym(libcobalt, "starboard_queue_input_event");
        starboard_queue_input_event(event);
    }
    return 0;
}

extern "C" {
void Java_com_example_native_1activity_CobaltNativeActivity_onVideoSurfaceCreated(
    JNIEnv *env, jobject thiz, jobject surface) {
    if (surface != NULL) {
        video_window = ANativeWindow_fromSurface(env, surface);
        if (main_window != NULL) {
            pthread_once(&once, launch_starboard);
        }
    }
}
}

/* Parameter for starboard, include file directories and main/video windows */
struct args {
    const char* external_files_dir;
    const char* external_cache_dir;
    ANativeWindow *main_window;
    ANativeWindow *video_window;
    int sbToAppMsgRead;
    int sbToAppMsgWrite;
};


void launch_starboard() {
    void* (*starboard_initialize) (void*) = (void*(*)(void*))dlsym(libcobalt, "starboard_initialize");
    LOGI("starboard_initialize: %p", starboard_initialize);
    /* Launch starboard application in another thread */
    pthread_t starboard_thread;
    struct args *sb_args = (struct args *)malloc(sizeof(struct args));
    sb_args->external_files_dir = external_files_dir;
    sb_args->external_cache_dir = external_cache_dir;
    sb_args->main_window = main_window;
    sb_args->video_window = video_window;
    sb_args->sbToAppMsgRead = msg_read;
    sb_args->sbToAppMsgWrite = msg_write;
    pthread_create(&starboard_thread, NULL, starboard_initialize, sb_args);
    pthread_detach(starboard_thread);
    LOGI("Starboard launched\n");
}


/**
 * Process the next main command.
 */
static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* engine = (struct engine*)app->userData;
    switch (cmd) {
        case APP_CMD_SAVE_STATE:
            break;
        case APP_CMD_INIT_WINDOW:
            // The window is being shown, get it ready.
            if (engine->app->window != NULL) {
                main_window = engine->app->window;
                if(video_window != NULL) {
                    pthread_once(&once, launch_starboard);
                }
            }
            break;
        case APP_CMD_TERM_WINDOW:
            // The window is being hidden or closed, clean it up.
            engine_term_display(engine);
            break;
        case APP_CMD_STOP:
            exit(0);
            break;
        case APP_CMD_GAINED_FOCUS:
            break;
        case APP_CMD_LOST_FOCUS:
            break;
    }
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app* state) {

    LOGI("android_main\n");

    /* Load libcobalt.so */
    libcobalt = dlopen("libcobalt.so", RTLD_NOW | RTLD_GLOBAL);
    LOGI("dlerror = %s\n", dlerror());
    LOGI("Loaded library: %p", libcobalt);

    JNIEnv *env;
    state->activity->vm->AttachCurrentThread(&env, 0);

    jobject me = state->activity->clazz;
    jclass acl = env->GetObjectClass(me); //class pointer of NativeActivity

    jmethodID giid = env->GetMethodID(acl, "getFilePath", "()Ljava/lang/String;");
    jstring path = (jstring)(env->CallObjectMethod(me, giid));
    external_files_dir = env->GetStringUTFChars(path, 0);
//    env->ReleaseStringUTFChars(path, external_files_dir);
    LOGI("External files dir = %s\n", external_files_dir);

    jmethodID giid2 = env->GetMethodID(acl, "getCachePath", "()Ljava/lang/String;");
    jstring path2 = (jstring)(env->CallObjectMethod(me, giid2));
    external_cache_dir = env->GetStringUTFChars(path2, 0);
//    env->ReleaseStringUTFChars(path2, external_cache_dir);
    LOGI("External cache dir = %s\n", external_cache_dir);

    int msgpipe[2];
    if (pipe(msgpipe)) {
        LOGI("could not create pipe: %s", strerror(errno));
    }
    msg_read = msgpipe[0];
    msg_write = msgpipe[1];
    fcntl(msg_read, F_SETFL, O_NONBLOCK);

    ALooper_addFd(state->looper, msg_read, MESSAGE_FD_ID, ALOOPER_EVENT_INPUT, NULL,
                  NULL);

    struct engine engine;

    // Make sure glue isn't stripped.
    app_dummy();

    memset(&engine, 0, sizeof(engine));
    state->userData = &engine;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    engine.app = state;

    // loop waiting for stuff to do.
    while (1) {
        // Read all pending events.
        int ident;
        int events;
        struct android_poll_source* source;

        // If not animating, we will block forever waiting for events.
        // If animating, we loop until all events are read, then continue
        // to draw the next frame of animation.
        while ((ident=ALooper_pollAll(/*engine.animating ? 0 : -1*/ 0, NULL, &events,
                                      (void**)&source)) >= 0) {
            if (ident == MESSAGE_FD_ID) {
                int message = 0;
                // clear the pipe
                while (read(msg_read, &message, sizeof(message)) > 0)
                { /* */ }

                ANativeActivity_finish(state->activity);
                continue;
            }

            // Process this event.
            if (source != NULL) {
                source->process(state, source);
            }

            // Check if we are exiting.
            if (state->destroyRequested != 0) {
                engine_term_display(&engine);
                return;
            }
        }
    }
}
//END_INCLUDE(all)
